#if 0
exit `$1 $2 | ./test/FileCheck $0`
#endif

#include "stdio.h"
#include "hal.h"

static int test() {

  // CHECK: aBC-DEF-G
  kprint_bitmask("abc-def-g", 0xFF);
  kprintf("\n");

  // CHECK: abC-DEF-g
  kprint_bitmask("abc-def-g", 0x7E);
  kprintf("\n");

  return 0;
}

static prereq_t p[] = { {"console",NULL}, {"x86/screen",NULL},
                        {"x86/serial",NULL}, {"hosted/console",NULL},
                        {NULL,NULL} };
static module_t run_on_startup x = {
  .name = "stdio-test",
  .load_after = p,
  .required = NULL,
  .init = &test,
  .fini = NULL
};
module_t *test_module = &x;
