#if !defined(X86)
# error This example must be run on an x86 bare kernel!
#endif

#include "hal.h"
#include "string.h"

static int f() {
  const char *c = "Hello, world!\n";
  write_console(c, strlen(c));
  return 0;
}

static const char *p[] = {"x86/screen", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "screen_example",
  .prerequisites = p,
  .fn = &f
};
 
