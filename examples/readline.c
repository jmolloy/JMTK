#include "hal.h"
#include "stdio.h"
#include "readline.h"

int rl() {
  char buf[64];
  while (1) {
    readline(buf, 64, "> ", NULL);
    kprintf("  -> '%s'\n", buf);
  }
  return 0;
}

static const char *p[] = {"x86/screen", "x86/keyboard", "x86/serial",
                          "hosted/console", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "readline-example",
  .prerequisites = p,
  .fn = &rl
};
