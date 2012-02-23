#include "hal.h"

int f() {
  debugger_except(NULL, "Totally fake trap");
  return 0;
}

static const char *p[] = {"x86/screen", "x86/keyboard", "x86/serial",
                          "hosted/console", "debugger", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "debugger-example",
  .prerequisites = p,
  .fn = &f
};
