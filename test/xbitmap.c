// RUN: %compile -g %s -o %t && %run %t only-run xbitmap-test | %FileCheck %s

#include "hal.h"
#include "adt/xbitmap.h"
#include "stdio.h"

static void *alloc(unsigned sz, void *obj) {
  uintptr_t *x = (uintptr_t*)obj;
  uintptr_t r = *x;

  *x += sz;

  map(r, alloc_page(PAGE_REQ_NONE), 1, PAGE_WRITE);

  return (void*)r;
}
static void free(void *ptr, void *obj) {
}

static int test() {
  xbitmap_t xb;
  uintptr_t loc = 0x20000000;
  xbitmap_init(&xb, get_page_size(), &alloc, &free, (void*)&loc);

  // CHECK: isset(0) = 0
  kprintf("isset(0) = %d\n", xbitmap_isset(&xb, 0));
  // CHECK: isclear(0) = 1
  kprintf("isclear(0) = %d\n", xbitmap_isclear(&xb, 0));

  xbitmap_set(&xb, 0);
  // CHECK: isset(0) = 1
  kprintf("isset(0) = %d\n", xbitmap_isset(&xb, 0));
  // CHECK: isclear(0) = 0
  kprintf("isclear(0) = %d\n", xbitmap_isclear(&xb, 0));

  xbitmap_clear(&xb, 0);
  // CHECK: isset(0) = 0
  kprintf("isset(0) = %d\n", xbitmap_isset(&xb, 0));

  xbitmap_set(&xb, 5);
  xbitmap_set(&xb, 7);
  xbitmap_set(&xb, 12);
  xbitmap_set(&xb, 20);
  // CHECK: first_set() = 5
  kprintf("first_set() = %d\n", xbitmap_first_set(&xb));
  xbitmap_clear(&xb, 5);
  // CHECK: first_set() = 7
  kprintf("first_set() = %d\n", xbitmap_first_set(&xb));

  // CHECK: isset(0x3456) = 0
  kprintf("isset(0x3456) = %d\n", xbitmap_isset(&xb, 0x3456));
  xbitmap_set(&xb, 0x3456);
  // CHECK: isset(0x3456) = 1
  kprintf("isset(0x3456) = %d\n", xbitmap_isset(&xb, 0x3456));
  // CHECK: isset(0x3455) = 0
  kprintf("isset(0x3455) = %d\n", xbitmap_isset(&xb, 0x3455));
  // CHECK: isset(12) = 1
  kprintf("isset(12) = %d\n", xbitmap_isset(&xb, 12));

  xbitmap_clear(&xb, 7);
  xbitmap_clear(&xb, 12);
  xbitmap_clear(&xb, 20);

  // CHECK: isset(6) = 0
  kprintf("isset(6) = %d\n", xbitmap_isset(&xb, 6));
  
  // CHECK: first_set() = 0x3456
  kprintf("first_set() = 0x%x\n", xbitmap_first_set(&xb));
  xbitmap_clear(&xb, 0x3456);
  // CHECK: first_set() = -1
  kprintf("first_set() = %d\n", xbitmap_first_set(&xb));
  
  return 0;
}

static const char *p[] = {"hosted/free_memory", "x86/free_memory", NULL};
static init_fini_fn_t run_on_startup x = {
  .name = "xbitmap-test",
  .prerequisites = p,
  .fn = &test
};
