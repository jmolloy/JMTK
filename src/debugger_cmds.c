#include "hal.h"
#include "stdlib.h"
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
  while ((ip = backtrace(&data, states[core].registers)) != 0) {
    const char *sym = lookup_kernel_symbol(ip, &offs);
    kprintf("%08x", ip);
    if (sym)
      kprintf(" %s+%#x", sym, offs);
    kprintf("\n");
  }
}

static void dbg_mappings(const char *cmd, core_debug_state_t *states, int core) {
  uintptr_t v = 0, old_v = 0, start = 0;
  unsigned int flags = 0, old_flags = 0;

  /* If the user specified an address, print the mapping for that address
     instead of all addresses. */
  if (strchr(cmd, ' ')) {
    unsigned long addr = strtoul(strchr(cmd, ' ')+1, NULL, 0);

    uint64_t p = get_mapping(addr, &flags);
    if (p == ~0ULL) {
      kprintf("%08x - not mapped\n", addr);
    } else {
      /* FIXME: Support 64-bit types in printf! */
      kprintf("%08x -> %08x ", addr, (uint32_t)p);
      kprint_bitmask("cuxw", flags);
      kprintf("\n");
    }
    return;
  }

  (void)get_mapping(0, &old_flags);

  while (v <= UINTPTR_MAX - get_page_size()) {
    v = iterate_mappings(v);

    (void)get_mapping(v, &flags);
    if (old_flags != flags || v != old_v + 0x1000) {
      kprintf("%08x..%08x ", start, old_v+0x1000);
      kprint_bitmask("cuxw", old_flags);
      kprintf("\n");

      old_flags = flags;
      start = v;
    }
    old_v = v;
  }

}


static int register_commands() {
  register_debugger_handler("print-regs", "Print register values", &dbg_info_regs);
  register_debugger_handler("backtrace", "Print a backtrace", &dbg_backtrace);
  register_debugger_handler("inspect-mappings", "Print the V->P mappings", &dbg_mappings);
  return 0;
}

void panic(const char *message) {
  kprintf("*** System panic!: %s\n", message);
  trap();
  abort();
  for(;;);
}

void assert_fail(const char *cond, const char *file, int line) {
  kprintf("*** Assertion failed: %s\n***   @ %s:%d\n", cond, file, line);
  trap();
  abort();
  for(;;);
}

static prereq_t p[] = { {"debugger",NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "debugger-cmds",
  .required = p,
  .load_after = NULL,
  .init = &register_commands,
  .fini = NULL
};
