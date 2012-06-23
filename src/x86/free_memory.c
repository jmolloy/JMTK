#include "hal.h"
#include "stdio.h"
#include "x86/multiboot.h"

extern multiboot_t mboot;

static int free_memory() {
  if ((mboot.flags & MBOOT_MMAP) == 0)
    panic("Bootloader did not provide memory map info!");

  range_t ranges[128];

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

  init_virtual_memory(ranges, n);
  init_physical_memory(ranges, n, extent);

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
