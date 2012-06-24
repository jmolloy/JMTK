#include "hal.h"
#include "mmap.h"

#define __USE_MISC /* Workaround to get MAP_ANON defined */
#include <sys/mman.h>
#include <stdlib.h>

static int free_memory() {
  if (mmap( (void*)MMAP_PHYS_BASE, MMAP_PHYS_END-MMAP_PHYS_BASE,
            PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED, -1, 0) !=
      (void*)MMAP_PHYS_BASE)
    panic("mmap() failed in free_memory()!");

  range_t r = {MMAP_PHYS_BASE, MMAP_PHYS_END - MMAP_PHYS_BASE};

  init_virtual_memory(&r, 1);
  init_physical_memory(&r, 1, MMAP_PHYS_END);

  return 0;
}

static prereq_t prereqs[] = { {"console", NULL}, {"hosted/console",NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "hosted/free_memory",
  .required = NULL,
  .load_after = prereqs,
  .init = &free_memory,
  .fini = NULL
};
