#include "hal.h"

int f() {
  // debugger_trap(NULL);
   __asm volatile("int3");

  return 0;
}

static const char *p[] = {"x86/screen", "x86/keyboard", "x86/serial",
                          "interrupts", "debugger", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "idt-example",
  .prerequisites = p,
  .fn = &f
};
