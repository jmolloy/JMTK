#include "hal.h"
#include "mmap.h"
#include "stdio.h"
#include "string.h"
#include "x86/io.h"
#include "x86/regs.h"

#define X86_PRESENT 0x1
#define X86_WRITE   0x2
#define X86_USER    0x4
#define X86_EXECUTE 0x200
#define X86_COW     0x400

#define PAGE_DIR_IDX(x) (x>>22)
#define PAGE_TABLE_IDX(x) (x>>12)

static address_space_t *current = NULL;

static spinlock_t global_vmm_lock = SPINLOCK_RELEASED;

static int from_x86_flags(int flags) {
  int f = 0;
  if (flags & X86_WRITE) f |= PAGE_WRITE;
  if (flags & X86_EXECUTE) f |= PAGE_EXECUTE;
  if (flags & X86_USER) f |= PAGE_USER;
  if (flags & X86_COW) f |= PAGE_COW;
  return f;
}
static int to_x86_flags(int flags) {
  int f = 0;
  if (flags & PAGE_WRITE) f |= X86_WRITE;
  if (flags & PAGE_USER) f |= X86_USER;
  if (flags & PAGE_EXECUTE) f |= X86_EXECUTE;
  if (flags & PAGE_COW) f |= X86_COW;
  return f;
}

int clone_address_space(address_space_t *dest, int make_cow) {
  spinlock_acquire(&global_vmm_lock);

  uint32_t p = alloc_page(PAGE_REQ_UNDER4GB);
  
  spinlock_init(&dest->lock);
  dest->directory = (uint32_t*)p;

  map(MMAP_KERNEL_TMP1, p, 1, PAGE_WRITE);
  uint32_t *d_dir = (uint32_t*)MMAP_KERNEL_TMP1;
  uint32_t *s_dir = (uint32_t*)MMAP_PAGE_DIR;

  for (unsigned i = 0; i < 1023; ++i) {
    d_dir[i] = s_dir[i];

    int is_user = ! IS_KERNEL_ADDR( 0x400000 * i );

    if (s_dir[i] & X86_PRESENT) {
      if (is_user || i == 1022) {
        uint32_t p2 = alloc_page(PAGE_REQ_UNDER4GB);
        d_dir[i] = p2 | X86_WRITE | X86_USER | X86_PRESENT;

        map(MMAP_KERNEL_TMP2, p2, 1, PAGE_WRITE);

        uint32_t *d_table = (uint32_t*)MMAP_KERNEL_TMP2;
        uint32_t *s_table = (uint32_t*)(MMAP_PAGE_TABLES + i*0x1000);
        for (unsigned j = 0; j < 1024; ++j) {
          d_table[j] = s_table[j];
          if (make_cow && is_user && s_table[j] & X86_WRITE) {
            d_table[j] = (s_table[j] & ~X86_WRITE) | X86_COW;
          }
        }

        /* tables[1022][1023] is mapped to the directory for the recursive
           page dir trick. */
        if (i == 1022)
          d_table[1023] = p | X86_PRESENT | X86_WRITE;
        unmap(MMAP_KERNEL_TMP2, 1);
      }
    }
  }

  /* tables[1023] is mapped to the directory for the recursive page dir
     trick. */
  d_dir[1023] = p | X86_PRESENT | X86_WRITE;

  unmap(MMAP_KERNEL_TMP1, 1);

  spinlock_release(&global_vmm_lock);

  return 0;
}

int switch_address_space(address_space_t *dest) {
  write_cr3((uintptr_t)dest->directory | X86_PRESENT | X86_WRITE);
  return 0;
}

address_space_t *get_current_address_space() {
  return current;
}

static int map_one_page(uintptr_t v, uint64_t p, unsigned flags) {
  spinlock_acquire(&current->lock);

  /* Quick sanity check - a page with CoW must not be writable. */
  if (flags & PAGE_COW)
    flags &= ~PAGE_WRITE;

  uint32_t *page_dir_entry = (uint32_t*) (MMAP_PAGE_DIR + PAGE_DIR_IDX(v)*4);
  if ((*page_dir_entry & X86_PRESENT) == 0) {
    //  kprintf("Done3?\n");
    uint64_t p = alloc_page(PAGE_REQ_UNDER4GB);
    
    if (p == ~0ULL)
      panic("alloc_page failed in map()!");

    *page_dir_entry = (p & 0xFFFFF000) | X86_PRESENT | X86_WRITE | X86_USER;

    memset((uint8_t*) (MMAP_PAGE_TABLES + PAGE_DIR_IDX(v)*0x1000), 0, 0x1000);
  }

  uint32_t *page_table_entry = (uint32_t*) (MMAP_PAGE_TABLES + PAGE_TABLE_IDX(v)*4);
  if (*page_table_entry & X86_PRESENT)
    panic("Tried to map a page that was already mapped!");

  *page_table_entry = (p & 0xFFFFF000) | (to_x86_flags(flags) | X86_PRESENT);

  spinlock_release(&current->lock);
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
  spinlock_acquire(&current->lock);

  uint32_t *page_dir_entry = (uint32_t*) (MMAP_PAGE_DIR + PAGE_DIR_IDX(v)*4);
  if ((*page_dir_entry & X86_PRESENT) == 0)
    panic("Tried to unmap a page that doesn't have its table mapped!");

  uint32_t *page_table_entry = (uint32_t*) (MMAP_PAGE_TABLES + PAGE_TABLE_IDX(v)*4);
  if ((*page_table_entry & X86_PRESENT) == 0)
    panic("Tried to unmap a page that isn't mapped!");

  *page_table_entry = 0;

  /* Invalidate TLB entry. */
  uintptr_t *pv = (uintptr_t*)v;
  __asm__ volatile("invlpg %0" : : "m" (*pv));

  spinlock_release(&current->lock);
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
  uint32_t *page_dir_entry = (uint32_t*) (MMAP_PAGE_DIR + PAGE_DIR_IDX(v)*4);
  if ((*page_dir_entry & X86_PRESENT) == 0)
    return ~0ULL;

  uint32_t *page_table_entry = (uint32_t*) (MMAP_PAGE_TABLES + PAGE_TABLE_IDX(v)*4);
  if ((*page_table_entry & X86_PRESENT) == 0)
    return ~0ULL;

  if (flags)
    *flags = from_x86_flags(*page_table_entry & 0xFFF);

  return *page_table_entry & 0xFFFFF000;
}

int is_mapped(uintptr_t v) {
  unsigned flags;
  return get_mapping(v, &flags) != ~0ULL;
}

static int page_fault(x86_regs_t *regs, void *ptr) {
  uint32_t cr2 = read_cr2();
  unsigned flags;

  uint32_t p = (uint32_t)get_mapping(cr2, &flags);

  if ((regs->error_code & (X86_PRESENT|X86_WRITE)) &&
      p != ~0UL && (flags & PAGE_COW) ) {
    /* Page was marked copy-on-write. */
    uint32_t p2 = (uint32_t)alloc_page(PAGE_REQ_UNDER4GB);

    /* We have to copy the page. In order to avoid a costly and 
       non-reentrant map/unmap pair to temporarily have them
       both mapped into memory, copy first into a buffer on
       the stack (this means the stack must be >4KB). */
    uint8_t buffer[4096];

    uint32_t v = cr2 & 0xFFFFF000;
    memcpy(buffer, (uint8_t*)v, 0x1000);
    
    if (unmap(v, 1) == -1)
      panic("unmap() failed during copy-on-write!");

    unsigned f = ((flags & X86_USER) ? PAGE_USER : 0) |
      ((flags & X86_EXECUTE) ? PAGE_EXECUTE : 0) |
      PAGE_WRITE;
    if (map(v, p2, 1, f) == -1)
      panic("map() failed during copy-on-write!");

    memcpy((uint8_t*)v, buffer, 0x1000);

    return 0;
  }

  kprintf("*** Page fault @ 0x%08x (", cr2);
  kprint_bitmask("iruwp", regs->error_code);
  kprintf(")\n");
  debugger_trap(regs);
  return 0;
}

static uint32_t page_from_ranges(range_t *ranges, unsigned nranges) {
  for (unsigned i = 0; i < nranges; ++i) {
    if (ranges[i].extent <= 0x1000 || ranges[i].start >= 0x100000000ULL)
      /* Discard any pages over 4GB physical or less than 4K in size. */
      continue;
    
    uint32_t ret = (uint32_t)ranges[i].start;
    ranges[i].start += 0x1000;
    ranges[i].extent -= 0x1000;

    kprintf("Returning %x\n", ret);
    return ret;
  }
  panic("init_virtual_memory couldn't find any pages to use!");
}

int init_virtual_memory(range_t *ranges, unsigned nranges) {
  /* Initialise the initial address space object. */
  static address_space_t a;
  uint32_t d = read_cr3();
  a.directory = (uint32_t*) (d & 0xFFFFF000);

  spinlock_init(&a.lock);
  
  current = &a;

  /* We normally can't write directly to the page directory because it will
     be in physical memory that isn't mapped. However, the initial directory
     was identity mapped during bringup. */

  /* Recursive page directory trick - map the page directory onto itself. */
  a.directory[1023] = (uint32_t)a.directory | X86_PRESENT | X86_WRITE;

  /* Recursive page directory trick stage 2 - map a new page table and 
     map the directory again in the last entry of the page table. */
  a.directory[1022] = page_from_ranges(ranges, nranges) | X86_PRESENT | X86_WRITE;

  memset((uint8_t*) (MMAP_PAGE_TABLES + 1022*0x1000), 0, 0x1000);
  uint32_t *page_table_entry = (uint32_t*) (MMAP_PAGE_TABLES + PAGE_TABLE_IDX(MMAP_PAGE_DIR) * 4);
  *page_table_entry = (uint32_t)a.directory | X86_PRESENT | X86_WRITE;

  /* Ensure the page table is mapped for the area required by the PMM. */
  unsigned last_table = ~0U;
  for (uintptr_t addr = MMAP_PMM_BITMAP; addr <= MMAP_PMM_BITMAP_END; addr += 0x1000) {
    if (PAGE_DIR_IDX(addr) != last_table) {
      uint32_t *page_dir_entry = (uint32_t*) (MMAP_PAGE_DIR + PAGE_DIR_IDX(addr) * 4);
      if ((*page_dir_entry & X86_PRESENT) == 0) {
        *page_dir_entry = page_from_ranges(ranges, nranges) | X86_PRESENT | X86_WRITE;

        memset((uint8_t*) (MMAP_PAGE_TABLES + PAGE_DIR_IDX(addr)*0x1000), 0, 0x1000);
      }

      last_table = PAGE_DIR_IDX(addr);
    }
  }

  register_interrupt_handler(14, &page_fault, NULL);

  /* Enable write protection, which allows page faults for read-only addresses
     in kernel mode. */
  write_cr0( read_cr0() | CR0_WP );

  return 0;
}
