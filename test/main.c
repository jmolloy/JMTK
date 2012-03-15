// RUN: %compile -DTEST0 %s -o %t1 && %t1 only-run X | %FileCheck --check-prefix=TEST0 %s
// RUN: %compile -DTEST1 %s -o %t2 && %t2 only-run X | %FileCheck --check-prefix=TEST1 %s
// RUN: %compile -DTEST2 %s -o %t3 && %t3 only-run X | %FileCheck --check-prefix=TEST2 %s
// RUN: %compile -DTEST3 %s -o %t4 && %t4 only-run Y | %FileCheck --check-prefix=TEST3 %s

// XFAIL: X86
// XFAIL: X64
// XFAIL: ARM

#if !defined(HOSTED)
# error This test must be run on a hosted kernel!
#endif
#include "hal.h"
#include "string.h"
#include <stdio.h>

#ifdef TEST0
static int f() {
  // CHECK-TEST0: f()
  printf("f()\n");
  return 0;
}
static init_fini_fn_t x run_on_startup = {
  .name = "X",
  .prerequisites = NULL,
  .fn = &f
};
// CHECK-TEST0: Running startup functions, status = 0
#endif

#ifdef TEST1
static int f() {
  // CHECK-TEST1: f()
  printf("f()\n");
  return 0;
}
static init_fini_fn_t x run_on_shutdown = {
  .name = "X",
  .prerequisites = NULL,
  .fn = &f
};
// CHECK-TEST1: Running shutdown functions, status = 0
#endif

#ifdef TEST2
static int f() {
  // CHECK-TEST2: f()
  printf("f()\n");
  return 0;
}
static int g() {
  // CHECK-TEST2: g()
  printf("g()\n");
  return 0;
}

static const char *p[] = {"Y", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "X",
  .prerequisites = p,
  .fn = &g
};
static init_fini_fn_t y run_on_startup = {
  .name = "Y",
  .prerequisites = NULL,
  .fn = &f
};
// CHECK-TEST2: Running startup functions, status = 0
#endif

#ifdef TEST3
static int f() {
  // CHECK-TEST3: f()
  printf("f()\n");
  return 0;
};
static int g() {
  // CHECK-TEST3: g()
  printf("g()\n");
  return 0;
};
static int h() {
  // CHECK-TEST3: h()
  printf("h()\n");
  return 0xff;
};

static const char *p[] = {"Z", NULL};
static const char *q[] = {"X", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "X",
  .prerequisites = p,
  .fn = &g
};
static init_fini_fn_t y run_on_startup = {
  .name = "Y",
  .prerequisites = q,
  .fn = &h
};
static init_fini_fn_t z run_on_startup = {
  .name = "Z",
  .prerequisites = NULL,
  .fn = &f
};
// CHECK-TEST3: Running startup functions, status = 255
#endif
