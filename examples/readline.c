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

static prereq_t p[] = { {"x86/screen",NULL}, {"x86/keyboard",NULL},
                        {"x86/serial",NULL}, {"hosted/console",NULL},
                        {NULL,NULL} };

static module_t x run_on_startup = {
  .name = "readline-example",
  .load_after = p,
  .required = NULL,
  .init = &rl,
  .fini = NULL
};
