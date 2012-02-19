// RUN: %compile -g %s -o %t && %t only-run stdlib-test | %FileCheck %s
// XFAIL: X86
// XFAIL: X64
// XFAIL: ARM

#if !defined(HOSTED)
# error This test must be run on a hosted kernel!
#endif

#include "stdlib.h"
#include "hal.h"
#include <stdio.h>

static int test() {
  char buf[64];
  char *str;
  char *endptr;
  long int li;
  long unsigned int lu;

  // CHECK: '1234' -> (1234, '0')
  str = "1234";
  li = strtol(str, &endptr, 10);
  printf("'%s' -> (%ld, '%u')\n", str, li, *endptr);

  // CHECK: '0x1234' -> (0x1234, '0')
  str = "0x1234";
  li = strtol(str, &endptr, 16);
  printf("'%s' -> (%#lx, '%u')\n", str, li, *endptr);

  // CHECK: '01234' -> (01234, '0')
  str = "01234";
  li = strtol(str, &endptr, 8);
  printf("'%s' -> (%#lo, '%u')\n", str, li, *endptr);

  // CHECK: '1234' -> (0x1234, '0')
  str = "1234";
  li = strtol(str, &endptr, 16);
  printf("'%s' -> (%#lx, '%u')\n", str, li, *endptr);
  
  // CHECK: '-1234' -> (-1234, '0')
  str = "-1234";
  li = strtol(str, &endptr, 10);
  printf("'%s' -> (%ld, '%u')\n", str, li, *endptr);

  // Ensure that strtoul fails on a negative string.
  // CHECK: '-1' -> (0, '45')
  str = "-1";
  li = strtoul(str, &endptr, 10);
  printf("'%s' -> (%ld, '%u')\n", str, li, *endptr);

  // CHECK: '-0x10' -> (-16, '0')
  str = "-0x10";
  li = strtol(str, &endptr, 16);
  printf("'%s' -> (%ld, '%u')\n", str, li, *endptr);

  // Check automatic base detection.
  // CHECK: '0x10' -> (16, '0')
  str = "0x10";
  li = strtol(str, &endptr, 0);
  printf("'%s' -> (%ld, '%u')\n", str, li, *endptr);

  // CHECK: '16' -> (16, '0')
  str = "16";
  li = strtol(str, &endptr, 0);
  printf("'%s' -> (%ld, '%u')\n", str, li, *endptr);

  // CHECK: '020' -> (16, '0')
  str = "020";
  li = strtol(str, &endptr, 0);
  printf("'%s' -> (%ld, '%u')\n", str, li, *endptr);

  return 0;
}

static init_fini_fn_t run_on_startup x = {
  .name = "stdlib-test",
  .prerequisites = NULL,
  .fn = &test
};
