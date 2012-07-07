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

  // CHECK: alloc1: c10fc000
  // CHECK: alloc2: c10fc400
  // CHECK: alloc3: c10fc800
  // CHECK: alloc4: c10fcc00
  // CHECK: alloc5: c10fd000
  // CHECK: alloc6: c10fd400
  kprintf("alloc1: %x\n", slab_cache_alloc(&c));
  kprintf("alloc2: %x\n", slab_cache_alloc(&c));
  kprintf("alloc3: %x\n", slab_cache_alloc(&c));
  kprintf("alloc4: %x\n", slab_cache_alloc(&c));
  kprintf("alloc5: %x\n", slab_cache_alloc(&c));
  kprintf("alloc6: %x\n", slab_cache_alloc(&c));

  slab_cache_free(&c, (void*)0xc10fc000);
  slab_cache_free(&c, (void*)0xc10fc400);
  slab_cache_free(&c, (void*)0xc10fc800);
  slab_cache_free(&c, (void*)0xc10fcc00);
  slab_cache_free(&c, (void*)0xc10fd000);

  // CHECK: alloc1: c10fc000
  // CHECK: alloc2: c10fc400
  // CHECK: alloc3: c10fc800
  // CHECK: alloc4: c10fcc00
  // CHECK: alloc5: c10fd000
  // CHECK: alloc6: c10fd800
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
