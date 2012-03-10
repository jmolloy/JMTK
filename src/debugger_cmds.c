#include "hal.h"
#include "string.h"
#include "stdio.h"

static void dbg_info_regs(const char *cmd, core_debug_state_t *states, int core) {
  const char *names[64];
  uintptr_t   values[64];
  int n = describe_regs(states[core].registers, 64, names, values);

  if (n == -1) {
    kprintf("describe_regs() failed!\n");
    return;
  }

  for (int i = 0; i < n; ++i) {
    if (i > 0 && i % 2 == 0)
      kprintf("\n");
    kprintf("%6s: %08x ", names[i], values[i]);
  }
  kprintf("\n");
}

static void dbg_backtrace(const char *cmd, core_debug_state_t *states, int core) {
  uintptr_t data = 0;
  int offs;
  uintptr_t ip;
  while ((ip = backtrace(&data)) != 0) {
    const char *sym = lookup_kernel_symbol(ip, &offs);
    kprintf("%08x", ip);
    if (sym)
      kprintf(" %s+%#x", sym, offs);
    kprintf("\n");
  }
}

static int register_commands() {
  register_debugger_handler("print-regs", "Print register values", &dbg_info_regs);
  register_debugger_handler("backtrace", "Print a backtrace", &dbg_backtrace);
  return 0;
}

static const char *p[] = {"debugger", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "debugger-cmds",
  .prerequisites = p,
  .fn = &register_commands
};
