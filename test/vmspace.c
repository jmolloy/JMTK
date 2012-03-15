// RUN: %compile %s -o %t && %run %t only-run vmspace-test 2>&1 | %FileCheck %s

#include "hal.h"
#include "stdio.h"
#include "vmspace.h"

int f () {
    vmspace_t vms;
    // CHECK: init: 0
    kprintf("init: %d\n", vmspace_init(&vms, 0xC0000000, 0x1C000000));

    // CHECK: alloc1: dbfea000
    kprintf("alloc1: %x\n", vmspace_alloc(&vms, 0x1000, 0));
    // CHECK: alloc2: dbfe8000
    kprintf("alloc2: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc3: dbfe9000
    kprintf("alloc3: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc4: dbfe0000
    kprintf("alloc4: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc5: dbfe1000
    kprintf("alloc5: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc6: dbfe2000
    kprintf("alloc6: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc7: dbfc0000
    kprintf("alloc7: %x\n", vmspace_alloc(&vms, 0x10000, 0)); 

    vmspace_free(&vms, 0x1000, 0xdbfe2000, 0);
    // CHECK: alloc8: dbfe2000
    kprintf("alloc8: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 

    // If we free everything we just allocated, and then allocate
    // them again, we can check buddies were correctly merged
    // by observing that the allocations return the same values
    // in the same order.
    vmspace_free(&vms, 0x1000, 0xdbfea000, 0);
    vmspace_free(&vms, 0x1000, 0xdbfe8000, 0);
    vmspace_free(&vms, 0x1000, 0xdbfe9000, 0);
    vmspace_free(&vms, 0x1000, 0xdbfe0000, 0);
    vmspace_free(&vms, 0x1000, 0xdbfe1000, 0);
    vmspace_free(&vms, 0x1000, 0xdbfe2000, 0);
    vmspace_free(&vms, 0x10000, 0xdbfc0000, 0);

    // CHECK: alloc1: dbfea000
    kprintf("alloc1: %x\n", vmspace_alloc(&vms, 0x1000, 0));
    // CHECK: alloc2: dbfe8000
    kprintf("alloc2: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc3: dbfe9000
    kprintf("alloc3: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc4: dbfe0000
    kprintf("alloc4: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc5: dbfe1000
    kprintf("alloc5: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc6: dbfe2000
    kprintf("alloc6: %x\n", vmspace_alloc(&vms, 0x1000, 0)); 
    // CHECK: alloc7: dbfc0000
    kprintf("alloc7: %x\n", vmspace_alloc(&vms, 0x10000, 0)); 


    return 0;
}

static const char *p[] = {"console", "x86/serial",
                          "x86/free_memory", "hosted/free_memory", NULL};

static init_fini_fn_t run_on_startup x = {
  .name = "vmspace-test",
  .prerequisites = p,
  .fn = &f
};
