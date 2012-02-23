#include "hal.h"
#include "readline.h"
#include "string.h"
#include "stdio.h"

#define DEBUG_IPI 0

#define MAX_CORES 256
#define MAX_BACKTRACE 32

static volatile int in_debugger = 0;
static volatile int num_cores_in_debugger = 0;

volatile int kdb_backtraces[MAX_CORES][MAX_BACKTRACE];
volatile struct regs *kdb_registers[MAX_CORES];

static void stop_other_processors() {
  send_ipi(-2, (void*)DEBUG_IPI);
}

static void save_backtrace() {
  uintptr_t data = 0;
  uintptr_t bt;

  int id = get_processor_id();
  if (id == -1) id = 0;

  for (int i = 0; i < MAX_BACKTRACE; ++i) {
    bt = backtrace(&data);
    kdb_backtraces[id][i] = bt;
    if (bt == 0) break;
  }
}

static void save_regs(struct regs *regs) {
  int id = get_processor_id();
  if (id == -1) id = 0;

  kdb_registers[id] = regs;
}

static void do_repl() {
  char line[256];
  while (1) {
    readline(line, 256, "(db) ", NULL);

    if (!strcmp(line, "exit"))
      break;
  }
}

static void do_debug() {
  num_cores_in_debugger = 0;
  in_debugger = 1;
  stop_other_processors();

  int num_other_processors = get_num_processors();
  if (num_other_processors != -1)
    while (num_cores_in_debugger != num_other_processors)
      ;
  save_backtrace();
  
  kprintf("*** Kernel debugger entered\n");
  
  do_repl();

  /* Allow other cores to continue. */
  in_debugger = 0;
}

void debugger_trap(struct regs *regs) {
  save_regs(regs);
  do_debug();
}

void debugger_except(struct regs *regs, const char *description) {
  save_regs(regs);
  kprintf("*** Exception: %s\n", description);
  do_debug();
}

static int debugger_handle_ipi(struct regs *regs, void *p) {
  void *value = get_ipi_data(regs);
  if (value == (void*)DEBUG_IPI) {
    int ints = get_interrupt_state();
    enable_interrupts();

    __sync_fetch_and_add(&num_cores_in_debugger, 1);
    save_backtrace();

    while (in_debugger)
      ;

    set_interrupt_state(ints);
  }
  return 0;
}

static int debugger_register() {

  if (get_ipi_interrupt_num() != -1 &&
      register_interrupt_handler(get_ipi_interrupt_num(),
                                 &debugger_handle_ipi,
                                 NULL) == -1) {
    kprintf("Unable to register interrupt handler for IPIs!\n");
    return -1;
  }

  return 0;
}

static const char *p[] = {"interrupts", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "debugger",
  .prerequisites = p,
  .fn = &debugger_register
};
