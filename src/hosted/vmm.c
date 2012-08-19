#include "assert.h"
#include "hal.h"
#include "mmap.h"
#include "stdio.h"
#include "string.h"

/* FIXME: Find out why we need these workarounds and remove them. */
#define __USE_MISC /* Workaround to get MAP_ANON defined */
#include <sys/mman.h>
#include <stdlib.h>
#define __USE_POSIX199309 /* Workaround to get siginfo_t defined */
#define __USE_POSIX /* Workaround to get siginfo_t defined */
#include <signal.h>

address_space_t *current, *kernel;
static spinlock_t global_vmm_lock = SPINLOCK_RELEASED;

int clone_address_space(address_space_t *dest, int make_cow) {
  spinlock_acquire(&current->lock);
  
  memcpy(dest, current, sizeof(address_space_t));
  spinlock_init(&dest->lock);

  if (make_cow) {
    for (unsigned i = 0; i < (1<<20); ++i) {
      if (dest->a[i] & PAGE_WRITE)
        dest->a[i] = (dest->a[i] & ~PAGE_WRITE) | PAGE_COW;
    }
  }

  spinlock_release(&current->lock);
  return 0;
}

int switch_address_space(address_space_t *dest) {
  spinlock_acquire(&global_vmm_lock);
  spinlock_acquire(&current->lock);

  for (unsigned i = 0; i < (1<<20); ++i) {
    if (current->a[i])
      munmap((void*)(uint64_t)(i*0x1000), 0x1000);
  }

  for (unsigned i = 0; i < (1<<20); ++i) {
    if (dest->a[i] == 0) continue;

    unsigned flags = dest->a[i] & 0xFFF;
    unsigned prot = ((flags & PAGE_WRITE) ? PROT_WRITE : 0) |
      ((flags & PAGE_EXECUTE) ? PROT_EXEC : 0) | PROT_READ;
    
    void *v = (void*) (uint64_t)(i*0x1000);
    if (mmap(v, dest->a[i] & 0xFFFFF000, prot, MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED, -1, 0) != v) {
      kprintf("v: %p\n", v);
      panic("mmap failed in switch_address_space!");
    }

  }
  spinlock_release(&current->lock);

  current = dest;

  spinlock_release(&global_vmm_lock);
  return 0;
}

static int map_one_page(uintptr_t v, uint64_t p, unsigned flags) {
  assert(p != ~0ULL && "Invalid physical address given to map(): ~0ULL!");

  /* Sanity check - if CoW, disable write access. */
  if (flags & PAGE_COW)
    flags &= ~PAGE_WRITE;

  address_space_t *a = current;
  if (v >= MMAP_KERNEL_START)
    a = kernel;

  spinlock_acquire(&a->lock);
  uint32_t *entry = &a->a[(uint32_t)v>>12];

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

  if (p >= MMAP_PHYS_BASE && p < MMAP_PHYS_END)
    memcpy((uint8_t*)v, (uint8_t*)p, 0x1000);

  if (mprotect((void*)v, 0x1000, prot) != 0)
    panic("mprotect() failed!");

  spinlock_release(&a->lock);
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
  spinlock_acquire(&a->lock);
  uint32_t *entry = &a->a[(uint32_t)v>>12];

  if (*entry == 0)
    panic("Tried to unmap a page that wasn't mapped!");

  uint32_t p = *entry & 0xFFFFF000;
  if (p >= MMAP_PHYS_BASE && p < MMAP_PHYS_END)
    memcpy((uint8_t*)(uintptr_t)p, (uint8_t*)v, 0x1000);

  *entry = 0;

  if (munmap((void*)v, 0x1000) == -1)
    panic("munmap() failed!");
  spinlock_release(&a->lock);
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
  if (flags)
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

int init_virtual_memory(range_t *ranges, unsigned nranges) {
  void *malloc(unsigned);
  address_space_t *a = malloc(sizeof(address_space_t));
  spinlock_init(&a->lock);

  if (!a)
    panic("malloc failed!\n");

  current = a;
  memset(a, 0, sizeof(address_space_t));
  kernel = malloc(sizeof(address_space_t));
  memset(kernel, 0, sizeof(address_space_t));

  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = &segv;
  if (sigaction(SIGSEGV, &sa, NULL) == -1)
    panic("sigaction() failed!");

  return 0;
}
