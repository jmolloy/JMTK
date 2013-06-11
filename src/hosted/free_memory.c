#include "hal.h"
#include "mmap.h"

#define _BSD_SOURCE /* Workaround to get MAP_ANON defined */
#define __USE_MISC  /* Workaround to get MAP_ANON defined */
#include <sys/mman.h>
#include <stdlib.h>
#undef _BSD_SOURCE
#undef __USE_MISC

static int free_memory() {
  if (mmap( (void*)MMAP_PHYS_BASE, MMAP_PHYS_END-MMAP_PHYS_BASE,
            PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED, -1, 0) !=
      (void*)MMAP_PHYS_BASE)
    panic("mmap() failed in free_memory()!");

  range_t r = {MMAP_PHYS_BASE, MMAP_PHYS_END - MMAP_PHYS_BASE};

  init_physical_memory_early(&r, 1, MMAP_PHYS_END);
  init_virtual_memory(&r, 1);
  init_physical_memory();
  init_cow_refcnts(&r, 1);

  return 0;
}

static prereq_t prereqs[] = { {"console", NULL}, {"hosted/console",NULL},
                              {"gcov", NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "hosted/free_memory",
  .required = NULL,
  .load_after = prereqs,
  .init = &free_memory,
  .fini = NULL
};
