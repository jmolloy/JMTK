#if 0
exit `$1 $2 --trace --symbols --timeout 15000 | ./test/FileCheck $0`
#endif

#include "hal.h"

// CHECK: higherhalf:
// CHECK: bringup:
// CHECK: main:
// CHECK: hlt

static module_t x run_on_startup = {
  .name = "X",
  .required = NULL,
  .load_after = NULL,
  .init = NULL,
  .fini = NULL
};
module_t *test_module = &x;
