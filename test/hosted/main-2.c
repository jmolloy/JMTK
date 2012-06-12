#if 0
exit `$1 | ./test/FileCheck $0`
#endif

#include "hal.h"
#include <stdio.h>

static int f() {
  // CHECK: f()
  printf("f()\n");
  return 0;
}
static int g() {
  // CHECK: g()
  printf("g()\n");
  return 0;
}

static prereq_t p[] = { {"Y",NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "X",
  .required = p,
  .load_after = NULL,
  .init = &g,
  .fini = NULL
};
static module_t y run_on_startup = {
  .name = "Y",
  .required = NULL,
  .load_after = NULL,
  .init = &f,
  .fini = NULL
};
module_t *test_module = &x;
