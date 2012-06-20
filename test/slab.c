#if 0
exit `$1 $2 | ./test/FileCheck $0`
#endif

#include "hal.h"
#include "stdio.h"
#include "slab.h"
#include "vmspace.h"

int f () {

  vmspace_t vms;
  // CHECK: vminit: 0
  kprintf("vminit: %d\n", vmspace_init(&vms, 0xC1000000, 0x100000));

  slab_cache_t c;
  // CHECK: create: 0
  kprintf("create: %d\n", slab_cache_create(&c, &vms, 1024, NULL));

  // alloc1: c10ec000
  // alloc2: c10ec400
  // alloc3: c10ec800
  // alloc4: c10ecc00
  // alloc5: c10ed000
  // alloc6: c10ed400
  kprintf("alloc1: %x\n", slab_cache_alloc(&c));
  kprintf("alloc2: %x\n", slab_cache_alloc(&c));
  kprintf("alloc3: %x\n", slab_cache_alloc(&c));
  kprintf("alloc4: %x\n", slab_cache_alloc(&c));
  kprintf("alloc5: %x\n", slab_cache_alloc(&c));
  kprintf("alloc6: %x\n", slab_cache_alloc(&c));

  slab_cache_free(&c, (void*)0xc10ec000);
  slab_cache_free(&c, (void*)0xc10ec400);
  slab_cache_free(&c, (void*)0xc10ec800);
  slab_cache_free(&c, (void*)0xc10ecc00);
  slab_cache_free(&c, (void*)0xc10ed000);

  // alloc1: c10ec000
  // alloc2: c10ec400
  // alloc3: c10ec800
  // alloc4: c10ecc00
  // alloc5: c10ed000
  // alloc6: c10ed800
  kprintf("alloc1: %x\n", slab_cache_alloc(&c));
  kprintf("alloc2: %x\n", slab_cache_alloc(&c));
  kprintf("alloc3: %x\n", slab_cache_alloc(&c));
  kprintf("alloc4: %x\n", slab_cache_alloc(&c));
  kprintf("alloc5: %x\n", slab_cache_alloc(&c));
  kprintf("alloc6: %x\n", slab_cache_alloc(&c));

  return 0;
}

static prereq_t p[] = { {"console",NULL}, {"x86/serial",NULL},
                        {"x86/free_memory",NULL},
                        {"hosted/free_memory",NULL},
                        {NULL,NULL} };

static module_t run_on_startup x = {
  .name = "slab-test",
  .load_after = p,
  .init = &f,
  .fini = NULL
};
module_t *test_module = &x;
