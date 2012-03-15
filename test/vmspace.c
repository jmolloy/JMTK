// RUN: %compile %s -o %t && %run %t only-run vmspace-test 2>&1 | %FileCheck %s

#include "hal.h"
#include "stdio.h"
#include "vmspace.h"

int f () {
    vmspace_t vms;
    // CHECK: init: 1
    kprintf("init: %d\n", vmspace_init(&vms, 0xC0000000, 0x1C000000));

    return 0;
}

static const char *p[] = {"console", "x86/serial",
                          "x86/free_memory", "hosted/free_memory", NULL};

static init_fini_fn_t run_on_startup x = {
  .name = "vmspace-test",
  .prerequisites = p,
  .fn = &f
};
