// RUN: %compile %s -o %t && %run %t only-run vmm-test 2>&1 | %FileCheck %s

#include "hal.h"
#include "stdio.h"

int f () {

  // CHECK: ps: 4096
  kprintf("ps: %d\n", get_page_size());

  // CHECK-NOT: addrspace: 0
  kprintf("addrspace: %x\n", get_current_address_space);

  // CHECK: map: 0
  kprintf("map: %d\n",
          map(0x50000000, 0x1000, 1, PAGE_WRITE|PAGE_USER|PAGE_EXECUTE));
  
  // CHECK: is_mapped: 1
  kprintf("is_mapped: %d\n",
          is_mapped(0x50000000));

  // CHECK: get_mapping: 1000 7
  unsigned f;
  uint32_t p = (uint32_t)get_mapping(0x50000000, &f);
  kprintf("get_mapping: %x %x\n", p, f);


  // CHECK: unmap: 0
  kprintf("unmap: %d\n", unmap(0x50000000, 1));

  // CHECK: is_mapped: 0
  kprintf("is_mapped: %d\n", is_mapped(0x50000000));

  // CHECK: map: 0
  kprintf("map: %d\n",
          map(0x40000000, 0x100000, 4, 0));

  // CHECK: 3ffff000: 0
  // CHECK: 40000000: 1
  // CHECK: 40001000: 1
  // CHECK: 40002000: 1
  // CHECK: 40003000: 1
  // CHECK-NOT: 40004000: 1
  // CHECK: end

  uintptr_t v = 0x3FFFF000;
  for (unsigned i = 0; i < 6; ++i) {
    kprintf("%x: %d\n", v, is_mapped(v));
    v = iterate_mappings(v);
  }
  kprintf("end\n");

  // Check copy-on-write.

  // CHECK: map: 0
  kprintf("map: %d\n",
          map(0x60000000, 0x100000, 1, PAGE_COW|PAGE_WRITE));
  volatile char *x = (volatile char*)0x60000000;
  x[0] = 42;
  // CHECK-NOT: p 0x100000
  // CHECK: flags 1
  p = (uint32_t)get_mapping(0x60000000, &f);
  kprintf("p %x\nflags %d\n", p, f);

  // CHECK: x[0] = 42, x[1] = 24
  x[1] = 24;
  kprintf("x[0] = %d, x[1] = %d\n", x[0], x[1]);

  // Check cloning.
  
  // CHECK: map: 0
  kprintf("map: %d\n",
          map(0x61000000, 0x100000, 1, PAGE_WRITE));

  static address_space_t d __attribute__((aligned(4096)));

  // CHECK: clone: 0
  kprintf("clone: %d\n",
          clone_address_space(&d, /*make_cow=*/1));

  // CHECK: map: 0
  kprintf("map: %d\n",
          map(0x62000000, 0x100000, 1, PAGE_WRITE));

  // CHECK: switch: 0
  kprintf("switch: %d\n",
          switch_address_space(&d));

  // CHECK: is_mapped1: 1
  kprintf("is_mapped1: %d\n", is_mapped(0x61000000));
  // CHECK: is_mapped2: 0
  kprintf("is_mapped2: %d\n", is_mapped(0x62000000));

  // Flags should be COW
  // CHECK: flags: 8
  get_mapping(0x61000000, &f);
  kprintf("flags: %d\n", f);

  return 0;
}

static const char *p[] = {"console", "x86/serial",
                          "x86/free_memory", "hosted/free_memory", NULL};

static init_fini_fn_t run_on_startup x = {
  .name = "vmm-test",
  .prerequisites = p,
  .fn = &f
};
