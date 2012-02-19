// RUN: %compile -g %s -o %t && %run %t only-run stdio-test 2>&1 | %FileCheck %s

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

static const char *p[] = {"console","x86/screen", "x86/serial",
                          "hosted/console", NULL};
static init_fini_fn_t run_on_startup x = {
  .name = "stdio-test",
  .prerequisites = p,
  .fn = &test
};
