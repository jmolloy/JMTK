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

static prereq_t p2[] = { {"x86/screen",NULL}, {"x86/keyboard",NULL},
                         {"x86/serial",NULL}, {"hosted/console",NULL},
                         {NULL,NULL} };

static prereq_t p[] = { {"debugger",NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "debugger-example",
  .required = p,
  .load_after = p2,
  .init = &f,
  .fini = NULL
};
