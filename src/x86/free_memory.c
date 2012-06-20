#include "hal.h"
#include "stdio.h"
#include "x86/multiboot.h"

extern multiboot_t mboot;

static void maybe_init_vmm(uint64_t page) {
  static uintptr_t initial_pages[NUM_INITIAL_PAGES];
  static int num_initial_pages = 0;

  if (num_initial_pages < NUM_INITIAL_PAGES) {
    initial_pages[num_initial_pages++] = (uintptr_t)page;
    if (num_initial_pages == NUM_INITIAL_PAGES)
      init_virtual_memory(initial_pages);
    return;
  }

  free_page(page);
}

static int free_memory() {
  if ((mboot.flags & MBOOT_MMAP) == 0)
    panic("Bootloader did not provide memory map info!");

  uint32_t i = mboot.mmap_addr;
  while (i < mboot.mmap_addr+mboot.mmap_length) {
    multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t*)i;

    if (MBOOT_IS_MMAP_TYPE_RAM(entry->type)) {
      for (uint32_t j = entry->base_addr; j < entry->base_addr+entry->length; j += 0x1000) {
        maybe_init_vmm(j);
      }
    }
    kprintf("e: sz %x addr %x len %x ty %x\n", entry->size, entry->base_addr, entry->length, entry->type);

    i += entry->size + 4;
  }
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
