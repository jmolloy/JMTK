#include "hal.h"
#include "mmap.h"
#include "stdio.h"
#include "string.h"

#define __USE_MISC /* Workaround to get MAP_ANON defined */
#include <sys/mman.h>
#include <stdlib.h>
#define __USE_POSIX199309 /* Workaround to get siginfo_t defined */
#define __USE_POSIX /* Workaround to get siginfo_t defined */
#include <signal.h>

struct address_space {
  uint32_t a[1<<20];
};

address_space_t *current, *kernel;

static int map_one_page(uintptr_t v, uint64_t p, unsigned flags) {

  /* Sanity check - if CoW, disable write access. */
  if (flags & PAGE_COW)
    flags &= ~PAGE_WRITE;

  address_space_t *a = current;
  if (v >= MMAP_KERNEL_START)
    a = kernel;
  uint32_t *entry = &a->a[v>>12];

  if (*entry)
    panic("Tried to map a page that was already mapped!");
  if (p > 0xFFFFFFFF)
    panic("Hosted mode doesn't support 64-bit phys addresses!");
  *entry = (uint32_t)p | flags;
    
  unsigned prot = ((flags & PAGE_WRITE) ? PROT_WRITE : 0) |
    ((flags & PAGE_EXECUTE) ? PROT_EXEC : 0) | PROT_READ;

  /* We need to memcpy the current physical memory value in, so make sure
     the map is writeable first. */
  if (mmap((void*)v, 0x1000, PROT_WRITE|PROT_READ, MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED, -1, 0) !=
      (void*)v)
    panic("mmap() failed!");

  memcpy((uint8_t*)v, (uint8_t*)(p+MMAP_PHYS_BASE), 0x1000);

  /* Now unmap and map again with the correct permissions! */
  if (munmap((void*)v, 0x1000) == -1)
    panic("munmap() failed!");

  if (mmap((void*)v, 0x1000, prot, MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED, -1, 0) !=
      (void*)v)
    panic("mmap() failed!");

  return 0;
}

int map(uintptr_t v, uint64_t p, int num_pages, unsigned flags) {
  for (int i = 0; i < num_pages; ++i) {
    if (map_one_page(v+i*0x1000, p+i*0x1000, flags) == -1)
      return -1;
  }
  return 0;
}

static int unmap_one_page(uintptr_t v) {
  address_space_t *a = current;
  if (v >= MMAP_KERNEL_START)
    a = kernel;
  uint32_t *entry = &a->a[v>>12];

  if (*entry == 0)
    panic("Tried to unmap a page that wasn't mapped!");

  *entry = 0;

  if (munmap((void*)v, 0x1000) == -1)
    panic("munmap() failed!");
  return 0;
}

int unmap(uintptr_t v, int num_pages) {
  for (int i = 0; i < num_pages; ++i) {
    if (unmap_one_page(v+i*0x1000) == -1)
      return -1;
  }
  return 0;
}

uintptr_t iterate_mappings(uintptr_t v) {
  while (v < 0xFFFFF000) {
    v += 0x1000;
    if (is_mapped(v))
      return v;
  }
  return ~0UL;
}

uint64_t get_mapping(uintptr_t v, unsigned *flags) {
  address_space_t *a = current;
  if (v >= MMAP_KERNEL_START)
    a = kernel;
  uint32_t *entry = &a->a[v>>12];

  if (*entry == 0)
    return ~0ULL;

  uint32_t p = *entry & 0xFFFFF000;
  *flags = *entry & 0xFFF;

  return p;
}

int is_mapped(uintptr_t v) {
  unsigned flags;
  return get_mapping(v, &flags) != ~0ULL;
}

static void segv(int sig, siginfo_t *si, void *unused) {
  uintptr_t addr = (uintptr_t)si->si_addr;

  unsigned flags;
  uint32_t p = (uint32_t)get_mapping(addr, &flags);

  if (p != ~0U && (flags & PAGE_COW)) {
    /* Page was marked copy-on-write. */
    uint32_t p2 = (uint32_t)alloc_page(PAGE_REQ_UNDER4GB);

    /* We have to copy the page. In order to avoid a costly and 
       non-reentrant map/unmap pair to temporarily have them
       both mapped into memory, copy first into a buffer on
       the stack (this means the stack must be >4KB). */
    static uint8_t buffer[4096];

    uint32_t v = addr & 0xFFFFF000;
    memcpy(buffer, (uint8_t*)(uintptr_t)v, 0x1000);
    
    if (unmap(v, 1) == -1)
      panic("unmap() failed during copy-on-write!");

    if (map(v, p2, 1, (flags & ~PAGE_COW)|PAGE_WRITE) == -1)
      panic("map() failed during copy-on-write!");

    memcpy((uint8_t*)(uintptr_t)v, buffer, 0x1000);

    return;
  }

  kprintf("*** Page fault @ 0x%08x\n", addr);
  void abort();
  abort();
}

int init_virtual_memory(uintptr_t *pages) {
  void *malloc(unsigned);
  address_space_t *a = malloc(sizeof(address_space_t));

  if (!a)
    kprintf("malloc failed!\n");

  current = a;
  kernel = malloc(sizeof(address_space_t));

  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = &segv;
  if (sigaction(SIGSEGV, &sa, NULL) == -1)
    panic("sigaction() failed!");

  return 0;
}
