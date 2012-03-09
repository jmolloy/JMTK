#include "hal.h"
#include "stdio.h"
#include "string.h"
#include "x86/regs.h"

#define NUM_TRAP_STRS 20
static const char *trap_strs[NUM_TRAP_STRS] = {
  "Divide error",
  "Reserved",
  "Non maskable interrupt",
  "Breakpoint",
  "Overflow'",
  "BOUND range exceeded",
  "Invalid opcode",
  "Device not available (No math coprocessor)",
  "Double fault",
  "Coprocessor segment overrun",
  "Bad TSS",
  "Segment not present",
  "Stack-segment fault",
  "General protection fault",
  "Page fault",
  "Reserved",
  "x87 FPU floating-point error",
  "Alignment check exception",
  "Machine check exception",
  "SIMD floating-point exception"
};

typedef struct idt_entry {
  uint16_t base_low;
  uint16_t sel;
  uint8_t zero1;
  uint8_t one_one_zero : 3;
  uint8_t d : 1;
  uint8_t zero2 : 1;
  uint8_t dpl : 2;
  uint8_t p : 1;
  uint16_t base_high;
} idt_entry_t;

typedef struct idt_ptr {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed)) idt_ptr_t;

typedef void* ty;
extern ty isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7, isr8, isr9,
  isr10, isr11, isr12, isr13, isr14, isr15, isr16, isr17, isr18, isr19,
  isr20, isr21, isr22, isr23, isr24, isr25, isr26, isr27, isr28, isr29,
  isr30, isr31, isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39,
  isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47;

#define NUM_HANDLERS 48
#define MAX_HANDLERS_PER_INT 4
static void **_handlers[NUM_HANDLERS] = {
  &isr0, &isr1, &isr2, &isr3, &isr4, &isr5, &isr6,
  &isr7, &isr8, &isr9, &isr10, &isr11, &isr12, &isr13,
  &isr14, &isr15, &isr16, &isr17, &isr18, &isr19, &isr20,
  &isr21, &isr22, &isr23, &isr24, &isr25, &isr26, &isr27,
  &isr28, &isr29, &isr30, &isr31, &isr32, &isr33, &isr34,
  &isr35, &isr36, &isr37, &isr38, &isr39, &isr40, &isr41,
  &isr42, &isr43, &isr44, &isr45, &isr46, &isr47};

static idt_entry_t entries[256];
static idt_ptr_t   idt_ptr;

static struct {
  interrupt_handler_t handler;
  void *p;
} handlers[NUM_HANDLERS][MAX_HANDLERS_PER_INT];
unsigned num_handlers[NUM_HANDLERS];

static void print_idt_entry(unsigned i, idt_entry_t e) {
  kprintf("#%02d: Base %#08x Sel %#04x\n", i, e.base_low | (e.base_high<<16), e.sel);
}

static void print_idt(const char *cmd, core_debug_state_t *states) {
  for (unsigned i = 0; i < NUM_HANDLERS; ++i) {
    if (i == 20) {
      kprintf("Press any key to continue...\n");
      char c;
      read_console(&c, 1);
    }
    print_idt_entry(i, entries[i]);
  }
}

static void set_idt_entry(idt_entry_t *e, uint32_t base, uint16_t sel, uint8_t dpl) {
  e->base_low = base & 0xFFFF;
  e->sel = sel;
  e->zero1 = 0;
  e->p = 1;
  e->dpl = dpl;
  e->zero2 = 0;
  e->d = 1;
  e->one_one_zero = 6; /* 0b110 = 6 */
  e->base_high = (base >> 16) & 0xFFFF;
}

static void print_handlers(const char *cmd, core_debug_state_t *states) {
  for (unsigned i = 0; i < NUM_HANDLERS; ++i) {
    if (num_handlers[i] == 0) continue;

    kprintf("#%02d: ");
    for (unsigned j = 0; j < num_handlers[i]; ++j) {
      interrupt_handler_t h = handlers[i][j].handler;

      int offs;
      const char *sym = lookup_kernel_symbol((uintptr_t)h, &offs);
      if (sym)
        kprintf("%s+%#x ", sym, offs);
      else
        kprintf("%p ", h);
    }
    kprintf("\n");
  }
}

static int init_idt() {
  register_debugger_handler("print-idt", "Print the IDT", &print_idt);
  register_debugger_handler("print-interrupt-handlers",
                            "Print all known interrupt handlers", &print_handlers);

  memset((uint8_t*)&num_handlers, 0, sizeof(unsigned)*NUM_HANDLERS);

  memset((uint8_t*)entries, 0, sizeof(idt_entry_t)*256);
  for (unsigned i = 0; i < NUM_HANDLERS; ++i)
    set_idt_entry(&entries[i], (uint32_t)_handlers[i], /*CS=*/0x08, /*DPL=*/0x00);

  idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
  idt_ptr.base  = (uint32_t)&entries[0];

  __asm volatile("lidt %0" : : "m" (idt_ptr));

  return 0;
}

int register_interrupt_handler(int num, interrupt_handler_t handler, void *p) {
  if (num >= NUM_HANDLERS)
    return -1;
  if (num_handlers[num] >= MAX_HANDLERS_PER_INT)
    return -1;
  handlers[num][num_handlers[num]].handler = handler;
  handlers[num][num_handlers[num]++].p = p;

  return 0;
}

int unregister_interrupt_handler(int num, interrupt_handler_t handler, void *p) {
  if (num >= NUM_HANDLERS)
    return -1;

  int found = 0;
  for (unsigned i = 0, e = num_handlers[num]; i < e; ++i) {
    if (handlers[num][i].handler == handler &&
        handlers[num][i].p == p)
      found = 1;

    if (found && i != e-1)
      handlers[num][i] = handlers[num][i+1];
  }

  if (found) {
    --num_handlers[num];
    return 0;
  }
  return 1;
}

void interrupt_handler(x86_regs_t *regs) {
  unsigned num = regs->interrupt_num;

  const char *desc = "";
  if (regs->interrupt_num < NUM_TRAP_STRS)
    desc = trap_strs[num];

  if (num_handlers[num]) {
    for (unsigned i = 0, e = num_handlers[num];
         i != e; ++i)
      handlers[num][i].handler(regs, handlers[num][i].p);
  } else if (num == 3) {
    debugger_trap(regs);
  } else {
    debugger_except(regs, desc);
  }
}

static const char *prereqs[] = {"x86/gdt", "debugger", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "interrupts",
  .prerequisites = prereqs,
  .fn = &init_idt
};
