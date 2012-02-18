// RUN: %compile %s -o %t
// XFAIL: Hosted
// XFAIL: ARM

#if !defined(X86)
# error This test must be run on an x86 bare kernel!
#endif

#include "hal.h"

static int f() {
  char buf = '1';
  while (1) {
    read_console(&buf, 1);
    write_console(&buf, 1);
  }
  return 0;
}

static const char *p[] = {"x86/screen", "x86/keyboard", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "test",
  .prerequisites = p,
  .fn = &f
};
