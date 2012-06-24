#if 0
exit `$1 $2 | ./test/FileCheck $0`
#endif

#include "hal.h"
#include "adt/xbitmap.h"
#include "stdio.h"

static int test() {
  xbitmap_t xb;
  uintptr_t loc = 0x20000000;

  map(loc, alloc_page(PAGE_REQ_NONE), 1, PAGE_WRITE);

  xbitmap_init(&xb, (void*)loc, 0x1000);

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

  // CHECK: isset(0x456) = 0
  kprintf("isset(0x456) = %d\n", xbitmap_isset(&xb, 0x456));
  xbitmap_set(&xb, 0x456);
  // CHECK: isset(0x456) = 1
  kprintf("isset(0x456) = %d\n", xbitmap_isset(&xb, 0x456));
  // CHECK: isset(0x455) = 0
  kprintf("isset(0x455) = %d\n", xbitmap_isset(&xb, 0x455));
  // CHECK: isset(12) = 1
  kprintf("isset(12) = %d\n", xbitmap_isset(&xb, 12));

  xbitmap_clear(&xb, 7);
  xbitmap_clear(&xb, 12);
  xbitmap_clear(&xb, 20);

  // CHECK: isset(6) = 0
  kprintf("isset(6) = %d\n", xbitmap_isset(&xb, 6));
  
  // CHECK: first_set() = 0x456
  kprintf("first_set() = 0x%x\n", xbitmap_first_set(&xb));
  xbitmap_clear(&xb, 0x456);
  
  // CHECK: first_set() = -1
  kprintf("first_set() = %d\n", xbitmap_first_set(&xb));
  
  return 0;
}

static prereq_t p[] = { {"hosted/free_memory",NULL},
                        {"x86/free_memory",NULL},
                        {"hosted/console", NULL}, {"x86/screen",NULL},
                        {"x86/serial",NULL}, {"console",NULL}, {NULL,NULL} };
static module_t run_on_startup x = {
  .name = "xbitmap-test",
  .required = NULL,
  .load_after = p,
  .init = &test,
  .fini = NULL
};
module_t *test_module = &x;
