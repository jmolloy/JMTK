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

  init_virtual_memory(NULL);

  for (uint64_t i = MMAP_PHYS_BASE; i < MMAP_PHYS_END; i += 0x1000)
    free_page(i-MMAP_PHYS_BASE);

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
