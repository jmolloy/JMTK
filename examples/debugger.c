#include "hal.h"

void null(const char *cmd, core_debug_state_t *states, int core) {

}

int f() {
  register_debugger_handler("null",
                            "This command does absolutely nothing, at all.",
                            &null);

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
