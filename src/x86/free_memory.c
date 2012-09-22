#include "hal.h"
#include "stdio.h"
#include "x86/multiboot.h"

extern multiboot_t mboot;

static void remove_range(range_t *r, uint64_t start, uint64_t extent) {
  /* FIXME: Assumes that a range exists that actually starts at 'start' and
     has extent greater than or equal to 'extent'. */
  if (r->start == start) {
    r->start += extent;
    r->extent -= extent;
  }
}

static int free_memory() {
  if ((mboot.flags & MBOOT_MMAP) == 0)
    panic("Bootloader did not provide memory map info!");

  range_t ranges[32], ranges_cpy[32];

  uint32_t i = mboot.mmap_addr;
  unsigned n = 0;
  uint64_t extent = 0;
  while (i < mboot.mmap_addr+mboot.mmap_length) {
    if (n >= 128) break;

    multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t*)i;

    if (MBOOT_IS_MMAP_TYPE_RAM(entry->type)) {
      ranges[n].start = entry->base_addr;
      ranges[n++].extent = entry->length;

      if (entry->base_addr + entry->length > extent)
        extent = entry->base_addr + entry->length;
    }
    kprintf("e: sz %x addr %x len %x ty %x\n", entry->size, (uint32_t)entry->base_addr, (uint32_t)entry->length, entry->type);

    i += entry->size + 4;
  }

  extern int __start, __end;
  uintptr_t end = (((uintptr_t)&__end) & ~get_page_mask()) + get_page_size();

  for (i = 0; i < n; ++i)
    remove_range(&ranges[i], (uintptr_t)&__start, end);

  for (i = 0; i < n; ++i) {
    kprintf("r: %x ext %x\n", (uint32_t)ranges[i].start, (uint32_t)ranges[i].extent);
  }

  /* Copy the ranges to a backup, as init_physical_memory mutates them and 
     init_cow_refcnts needs to run after init_physical_memory */
  for (i = 0; i < n; ++i)
    ranges_cpy[i] = ranges[i];

  init_virtual_memory(ranges, n);
  init_physical_memory(ranges, n, extent);
  init_cow_refcnts(ranges_cpy, n);

  return 0;
}

static prereq_t prereqs[] = { {"console",NULL}, {"x86/serial",NULL},
                              {"x86/screen", NULL}, {"x86/keyboard", NULL},
                              {"debugger", NULL}, {"debugger-cmds", NULL},
                              {NULL, NULL} };
static module_t x run_on_startup = {
  .name = "x86/free_memory",
  .required = NULL,
  .load_after = prereqs,
  .init = &free_memory,
  .fini = NULL
};
