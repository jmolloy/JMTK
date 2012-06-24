#if 0
exit `$1 $2 | ./test/FileCheck $0`
#endif

#include "hal.h"
#include "stdio.h"
#include "vmspace.h"

int f () {
    vmspace_t vms;
    // CHECK: init: 0
    kprintf("init: %d\n", vmspace_init(&vms, 0xC1000000, 0x1C000000));

    // CHECK: alloc1: dcff0000
    kprintf("alloc1: %x\n", vmspace_alloc(&vms, 0x1000, 0));
    // CHECK: alloc2: dcff1000
    kprintf("alloc2: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc3: dcff2000
    kprintf("alloc3: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc4: dcff3000
    kprintf("alloc4: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc5: dcff4000
    kprintf("alloc5: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc6: dcff5000
    kprintf("alloc6: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc7: dcfe0000
    kprintf("alloc7: %x\n", vmspace_alloc(&vms, 0x10000, 0)); 

    vmspace_free(&vms, 0x1000, 0xdcff2000, 0);
    // CHECK: alloc8: dcff2000
    kprintf("alloc8: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 

    // If we free everything we just allocated, and then allocate
    // them again, we can check buddies were correctly merged
    // by observing that the allocations return the same values
    // in the same order.
    vmspace_free(&vms, 0x1000, 0xdcff0000, 0);
    vmspace_free(&vms, 0x1000, 0xdcff1000, 0);
    vmspace_free(&vms, 0x1000, 0xdcff2000, 0);
    vmspace_free(&vms, 0x1000, 0xdcff3000, 0);
    vmspace_free(&vms, 0x1000, 0xdcff4000, 0);
    vmspace_free(&vms, 0x1000, 0xdcff5000, 0);
    vmspace_free(&vms, 0x10000, 0xdcfe0000, 0);

    // CHECK: alloc1: dcff0000
    kprintf("alloc1: %x\n", vmspace_alloc(&vms, 0x1000, 0));
    // CHECK: alloc2: dcff1000
    kprintf("alloc2: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc3: dcff2000
    kprintf("alloc3: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc4: dcff3000
    kprintf("alloc4: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc5: dcff4000
    kprintf("alloc5: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc6: dcff5000
    kprintf("alloc6: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc7: dcfe0000
    kprintf("alloc7: %x\n", vmspace_alloc(&vms, 0x10000, 0)); 

    // CHECK-NOT: Page fault
    uintptr_t *addr = (uintptr_t*)vmspace_alloc(&vms, 0x1000, 1);
    *addr = 0x42;

    return 0;
}

static prereq_t p[] = { {"console",NULL}, {"x86/serial",NULL},
                        {"x86/free_memory",NULL}, {"hosted/free_memory",NULL},
                        {NULL,NULL} };

static module_t run_on_startup x = {
  .name = "vmspace-test",
  .load_after = p,
  .required = NULL,
  .init = &f,
  .fini = NULL
};
module_t *test_module = &x;
