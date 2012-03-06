#include "hal.h"

int f() {
  debugger_trap(NULL);
  return 0;
}

static const char *p[] = {"x86/screen", "x86/keyboard", "x86/serial",
                          "hosted/console", "x86/gdt", "debugger", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "gdt-example",
  .prerequisites = p,
  .fn = &f
};
