#include "hal.h"
#include "string.h"
#include <stdio.h>

console_t c2 = {
  .open = NULL,
  .close = NULL,
  .read = NULL,
  .write = NULL,
  .flush = NULL,
  .data = NULL
};

static int test() {
  // CHECK: reg c: 0
  printf("reg c: %d\n", register_console(&c2));
  return 0;
}
static int test_end() {
  unregister_console(&c2);
  
  // CHECK: ok?
  printf("ok?\n");
  return 0;
}

static prereq_t p[] = { {"console",NULL}, {NULL,NULL} };

static module_t run_on_startup x = {
  .name = "console-test",
  .required = p,
  .load_after = NULL,
  .init = &test,
  .fini = &test_end
};
