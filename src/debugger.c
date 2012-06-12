#include "hal.h"
#include "readline.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

/** The interprocessor interrupt number that the debugger can use. */
#define DEBUG_IPI 0

/** The maximum number of debugger commands that can be registered. */
#define MAX_CMDS 32

/** Used for multicore - are we in the debugger? */
static volatile int in_debugger = 0;
/** Used for multicore - how many cores are currently in the debugger? */
static volatile int num_cores_in_debugger = 0;

/** State of all cores. */
static volatile core_debug_state_t states[MAX_CORES];

/** A debugger command. */
typedef struct cmd {
  const char *cmd;  /* The command name */
  const char *help; /* Help text */
  debugger_fn_t fn; /* The function itself */
} cmd_t;

static int num_cmds = 0;
static cmd_t cmds[MAX_CMDS];

/* Prints the n'th string in a table */
static void print_tabular(const char *str, int n) {
#define NUM_COLS 4

  if (n != 0 && n % NUM_COLS == 0 )
    kprintf("\n");
  kprintf("%-20s", str);
}

/* Assume that 'cmd' must be unambiguous, and try and 
   fuzzy match it. Return the index into 'cmds' on success or
   -1 on failure. */
static int get_unambiguous_cmd(const char *cmd) {
  /* Scan for the first space. */
  int len = 0;
  while (cmd[len] != '\0' && cmd[len] != ' ')
    ++len;
  for (; len != 0; --len) {
    int matches = 0;
    int match = -1;
    for (int i = 0; i < num_cmds; ++i) {
      if (!strncmp(cmd, cmds[i].cmd, len)) {
        ++matches;
        match = i;
      }
    }

    if (matches == 1)
      return match;
    else if (matches > 1)
      return -1;
  }
  return -1;
}

/* Assume that 'cmd' can be ambiguous, and print out the
   possibly disambiguations for the user. */
static void print_ambiguous(const char *cmd) {
  for (int len = strlen(cmd); len != 0; --len) {
    int matches = 0;
    for (int i = 0; i < num_cmds; ++i) {
      if (!strncmp(cmd, cmds[i].cmd, len))
        ++matches;
    }

    if (matches == 1) {
      /* Shouldn't get here. */
      kprintf("Algorithmic error in debugger!");
      return;
    } else if (matches > 1) {
      kprintf("%s is ambiguous - did you mean one of these?:\n", cmd);

      matches = 0;
      for (int i = 0; i < num_cmds; ++i) {
        if (!strncmp(cmd, cmds[i].cmd, len))
          print_tabular(cmds[i].cmd, matches++);
      }
      kprintf("\n");
      return;
    }
  }

  kprintf("%s is not a known command.\n", cmd);
}

/* The 'help' command - print out the help text for all known commands, or
   if a parameter is given, just the help for that command. */
static void help(const char *cmd, core_debug_state_t *states, int core) {
  /* Strip the "help" and following whitespace off the front. */
  cmd = &cmd[4];
  while (*cmd == ' ')
    ++cmd;
  
  /* No parameters, list all commands. */
  if (*cmd == '\0') {
    for (int i = 0; i < num_cmds; ++i)
      kprintf("%10s - %s\n", cmds[i].cmd, cmds[i].help);
  } else {
    int c = get_unambiguous_cmd(cmd);
    if (c != -1)
      kprintf("%s\n", cmds[c].help);
    else
      print_ambiguous(cmd);
  }
}

static void stop_other_processors() {
  send_ipi(IPI_ALL_BUT_THIS, (void*)DEBUG_IPI);
}

static void save_regs(struct regs *regs) {
  int id = get_processor_id();
  if (id == -1) id = 0;

  states[id].registers = regs;
}

/* Main REPL for the debugger. */
static void do_repl() {
  int core = get_processor_id();
  if (core == -1)
    core = 0;

  char line[256];
  while (1) {
    readline(line, 256, "(db) ", NULL);

    if (!strcmp(line, "exit"))
      break;

    if (!strncmp(line, "core", 4)) {
      core = strtoul(line+5, NULL, 10);
      kprintf("Processor switched to #%d\n", core);
      continue;
    }

    int id = get_unambiguous_cmd(line);
    if (id == -1)
      print_ambiguous(line);
    else
      cmds[id].fn(line, (core_debug_state_t*)states, core);
      
  }
}

/* Enter the debugger proper. */
static void do_debug() {
  num_cores_in_debugger = 0;
  in_debugger = 1;
  stop_other_processors();

  /* Wait until all processors are stopped before continuing. */
  int num_other_processors = get_num_processors();
  if (num_other_processors != -1)
    while (num_cores_in_debugger != num_other_processors)
      ;
  
  kprintf("*** Kernel debugger entered from core #%d\n",
          get_processor_id() == -1 ? 0 : get_processor_id());
  
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

    while (in_debugger)
      ;

    set_interrupt_state(ints);
  }
  return 0;
}

int register_debugger_handler(const char *name, const char *help,
                              debugger_fn_t fn) {
  if (num_cmds >= MAX_CMDS)
    return -1;

  cmds[num_cmds].cmd = name;
  cmds[num_cmds].help = help;
  cmds[num_cmds].fn   = fn;
  ++num_cmds;

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

  if (register_debugger_handler("help", "Display help for a command.",
                                &help) == -1) {
    kprintf("Unable to register 'help' debugger handler!\n");
    return -1;
  }

  return 0;
}

static module_t x run_on_startup = {
  .name = "debugger",
  .required = NULL,
  .load_after = NULL,
  .init = &debugger_register,
  .fini = NULL
};
