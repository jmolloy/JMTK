#if 0
exit `$1 $2 | ./test/FileCheck $0`
#endif

#include "hal.h"
#include <stdio.h>

static int f() {
  // CHECK: f()
  printf("f()\n");
  return 0;
}
static module_t x run_on_startup = {
  .name = "X",
  .required = NULL,
  .load_after = NULL,
  .init = &f,
  .fini = NULL
};
module_t *test_module = &x;
