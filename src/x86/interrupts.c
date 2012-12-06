/**#1
   Interrupt handling and IRQs
   ===========================

   The next important stage in bringing up an OS kernel is the ability to handle interrupts, both internal and external. Interrupts are events that change the normal execution flow of the processor. These can include external events (Interrupt requests) or internal events (division by zero).

   First, some nomenclature. The X86 manual defines several terms that mean very similar things:

   *Interrupt*
     This comes from an external source; the "IRQ" line on the processor being held low. Interrupts are by their nature *asynchronous* with the program execution flow. That is, they could come in between an instruction boundary. The processor ensures that all instructions either complete or do not complete in their entirety (no half-executed instructions allowed) and jumps to an interrupt handler.

   *Exception*
     An *exception* is any *synchronous* change in execution flow. An exception is an umbrella term for *faults*, *traps* and *aborts*.

   *Faults*
     An exception that occurs upon execution of an instruction, that can generally be corrected and that, once corrected, allows the program to be restarted with no loss of continuity [3A5.5]_. When a fault handler returns, the faulting instruction is re-attempted.

   *Traps*
     An exception that is reported immediately following the execution of the trapping instruction. When a trap handler returns, execution begins immediately *after* the trapping instruction.

   *Aborts*
     An abort is an exception that does not always report the precise location of the instruction causing the exception and does no allow a restart of the program or task that caused the exception [3A5.5]_. These are the kinds of exception that occur due to hardware faults.

   In normal usage, people tend to use the words "interrupt", "fault" and "trap" interchangeably. However as mentioned above, when referring specifically to the X86 they have slightly different semantics.

   .. [3A5.5] Intel 64 and IA-32 architectures software development manual volume 3A: ss. 5.5
*/
/**
   Exceptions
   ==========

   There are 20 defined exception numbers (0..19), plus 12 more reserved for use by Intel to give 32 reserved numbers (these are often also referred to as exception *vectors*). Exceptions may potentially carry with them an error code; we will explain more about this later.

   They are as follows:

   +--------+-----------------------------------------------+-----------+
   | Vector | Description                                   | Type      |
   +========+===============================================+===========+
   | 0   #DE| Divide error. Divide by zero.                 | Fault     |
   +--------+-----------------------------------------------+-----------+
   | 1   #DB| Reserved                                      | Fault     |
   +--------+-----------------------------------------------+-----------+
   | 2      | NMI - non-maskable interrupt                  | Interrupt |
   +--------+-----------------------------------------------+-----------+
   | 3   #BP| Breakpoint (INT3 instruction)                 | Trap      |
   +--------+-----------------------------------------------+-----------+
   | 4   #OF| Overflow (INTO instruction)                   | Trap      |
   +--------+-----------------------------------------------+-----------+
   | 5   #BR| BOUND range exceeded (BOUND instruction)      | Fault     |
   +--------+-----------------------------------------------+-----------+
   | 6   #UD| Invalid opcode                                | Fault     |
   +--------+-----------------------------------------------+-----------+
   | 7   #NM| Device not available                          | Fault     |
   +--------+-----------------------------------------------+-----------+
   | 8   #DF| Double fault                                  | Abort     |
   +--------+-----------------------------------------------+-----------+
   | 9      | Coprocessor segment overrun                   | Fault     |
   +--------+-----------------------------------------------+-----------+
   | 10  #TS| Invalid TSS                                   | Fault     |
   +--------+-----------------------------------------------+-----------+
   | 11  #NP| Segment not present                           | Fault     |
   +--------+-----------------------------------------------+-----------+
   +--------+-----------------------------------------------+-----------+
   | 12  #SS| Stack-segment fault                           | Fault     |
   +--------+-----------------------------------------------+-----------+
   | 13  #GP| General protection fault (GPF)                | Fault     |
   +--------+-----------------------------------------------+-----------+
   | 14  #PF| Page fault (PF)                               | Fault     |
   +--------+-----------------------------------------------+-----------+
   | 15     | Reserved                                      | Fault     |
   +--------+-----------------------------------------------+-----------+
   | 16  #MF| x86 FPU error                                 | Fault     |
   +--------+-----------------------------------------------+-----------+
   | 17  #AC| Alignment check                               | Fault     |
   +--------+-----------------------------------------------+-----------+
   | 18  #MC| Machine check                                 | Abort     |
   +--------+-----------------------------------------------+-----------+
   | 19  #XM| SSE error                                     | Fault     |
   +--------+-----------------------------------------------+-----------+

   There are several exceptions that are rare in practice; The ones you are most likely to see, with some likely causes are:

   *#DE*
     You divided by zero. Silly you.

   *#NM*
     You tried to do a floating point instruction without first setting up the floating point unit.

   *#UD*
     You wandered off into undefined or invalid memory, and started executing code there. Or your relocations / ELF loader is wrong.

   *#DF*
     You took a fault in your fault handler. One more and you'll get a *triple fault* and your machine will reset itself.

   *#GP*
     You you tried to do kernel-mode stuff in user mode.

   *#PF*
     The most common. You tried to write to a read-only page, read/write from a kernel only page in user mode, or access a page that wasn't present.  { */

#include "hal.h"
#include "stdio.h"
#include "string.h"
#include "x86/io.h"
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

/**
   The IDT
   =======

   Now we've defined all the exception types, let's discuss how to actually handle them.

   For every exception or interrupt, you create an *interrupt descriptor*. This is very similar to the segment descriptors in the GDT chapter, and similarly they live in a table called the *Interrupt Descriptor Table* (IDT) and that is loaded into the processor by a special instruction - ``lidt``.

   .. image:: ../../../doc/idt-descriptor.svg
       :width: 500px
       :height: 200px

   The IDT descriptor is described in the above image, and has these constituent parts:

   *Base*
     The address of the interrupt handler.

   *Sel*
     The segment selector for the code segment to jump to; this is always
     0x08 for our kernel code segment.

   *D*
     The instruction size this refers to. 0 for 16 bits, or always 1 in our
     case for 32 bits.

   *DPL*
     The Descriptor Privilege Level. This gives the highest (numerically) privilege level that an interrupt using this gate can be taken at. For example, if the DPL is 0, code executing in ring 3 *cannot trigger* this interrupt, but this restriction applies only to the "software interrupt" instructions ``INT n``, ``INT3`` and ``INTO``. This is intended to stop software pretending as though a page fault has occurred when it hasn't, for example.

   *P*
     Segment present. Always 1, for any segment we define. { */

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

/** Similarly to the GDT, the IDT has a structure that is given to the processor. It contains the start and size of the descriptor table. { */

typedef struct idt_ptr {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed)) idt_ptr_t;

/** The interrupt service routines are defined in assembly, because they must do some acrobatics to get the machine in a shape suitable for calling C code. We import their symbols here. { */

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

/** Then we create a table of interrupt handlers that we should dispatch to. Each contains the handler function and also an optional parameter that can be passed to ``register_interrupt_handler``.

    In our design, each interrupt can have multiple (up to 4) handlers attached. { */

static struct {
  interrupt_handler_t handler;
  void *p;
} handlers[NUM_HANDLERS][MAX_HANDLERS_PER_INT];
unsigned num_handlers[NUM_HANDLERS];

/** These functions are purely for aiding debugging. The ``print_idt`` and ``print_handlers`` functions are designed to be called from an optional kernel debugger module. { */

static void print_idt_entry(unsigned i, idt_entry_t e) {
  kprintf("#%02d: Base %#08x Sel %#04x\n", i, e.base_low | (e.base_high<<16), e.sel);
}

static void print_idt(const char *cmd, core_debug_state_t *states, int core) {
  for (unsigned i = 0; i < NUM_HANDLERS; ++i) {
    if (i == 20) {
      kprintf("Press any key to continue...\n");
      char c;
      read_console(&c, 1);
    }
    print_idt_entry(i, entries[i]);
  }
}

static void print_handlers(const char *cmd, core_debug_state_t *states, int core) {
  for (unsigned i = 0; i < NUM_HANDLERS; ++i) {
    if (num_handlers[i] == 0) continue;

    kprintf("#%02d: ", i);
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

/** Here we set up an IDT descriptor. I hope this should all be self-explanatory... { */

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

/** 
    Interrupt requests (IRQs)
    =========================

    Now that we've dealt with exceptions, I need to talk a little about interrupts.

    Interrupts come (historically, and conceptually) from outside the chip boundary. The X86 has only one pin for this, called the "interrupt request" or IRQ pin.

    An external device can trigger an interrupt by pulling the IRQ pin low. The external device can then broadcast to the CPU a number, which will be the interrupt number taken.

    The CPU will service the interrupt at some point (not necessarily immediately - interrupts may be disabled). It is the external device's job to then push the IRQ pin high again to allow the CPU to continue.

    PICs
    ----

    Obviously, this requires a lot of compliance from the device and only one device can feasibly be connected. So the device that is connected is a special multiplexer called a PIC - *Programmable Interrupt Controller*.

    The standardised PIC for the x86 platform is the Intel 8259 spec. This can connect to 8 external devices and can interface with the x86 itself. When an input device triggers an interrupt, the PIC will lower the IRQ pin and broadcast a number to the CPU. This number can be changed programmatically.

    The PIC will hold IRQ low until it is sent a special *End Of Interrupt* (EOI) message. It will queue up interrupts, such that if one arrives while another is being serviced, as soon as the EOI occurs it will pull IRQ low again and force the CPU to take another interrupt. However note that it will only queue up *one interrupt per source*. That is, if the CPU is handling an interrupt and these interrupts come in: line 1, line 2, line 1 - the PIC will pass on one interrupt for line 1 and one interrupt for line 2 to the CPU.

    Chaining PICs
    -------------

    As an 8259 PIC can only service 8 devices, IBM standardised on using two PICs, daisy-chained together. So there are two independent PICs - one master, one slave. The slave has its IRQ line connected to the master's *line 2 input*. So, this cannot be connected to another device and you have 15 usable IRQs.
**/

/**
    Programming the PICs
    --------------------

    The PICs live on the I/O bus, and have two registers each: "Command" and "Data". { */

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

/**
    Now, on CPU bootup the PICs will report numbers for their external inputs that collide with the Intel reserved exception numbers we mentioned above. So the first thing we need to do is remap the PICs to give us a set of interrupt numbers that do not collide with others.

    The canonical way of doing this is mapping them to numbers [32..48). That is, the master PIC deals with interrupt numbers 32..40, and the slave 40..48. We have a macro in ``include/x86/hal.h`` called ``IRQ()`` that will map from an IRQ number (based at 0) to an interrupt vector number by simply adding 32.

    This function does the remapping; the way it does it is somewhat irrelevant as it is copy-pasted from a datasheet and has made the rounds in the same form for every hobby OS that's ever existed! { */

#define PIC_INIT  0x10
#define PIC_ICW4  0x01
#define PIC_8086  0x01

static void pic_init() {
  /* Remap the irq table to [32,48) */
  outb(PIC1_CMD, PIC_INIT|PIC_ICW4);
  outb(PIC2_CMD, PIC_INIT|PIC_ICW4);
  outb(PIC1_DATA, 0x20);            /* Offset 32 */
  outb(PIC2_DATA, 0x28);            /* Offset 40 */
  outb(PIC1_DATA, 1 << 2);          /* Slave PIC @ irq 2 */
  outb(PIC2_DATA, 0x02);            /* Slave PIC's identity is 2 */
  outb(PIC1_DATA, PIC_8086);
  outb(PIC2_DATA, PIC_8086);
  outb(PIC1_DATA, ~(1<<2));         /* Mask all interrupts but IRQ2 */
  outb(PIC2_DATA, 0xFF);            /* Mask all interrupts */
}

/** Each interrupt line can be enabled or disabled. Disabling an interrupt in the PIC stops it from ever reaching or bothering the processor.

    This can be done simply by writing an 8-bit bitmask to ``PICN_DATA`` (where N is 1 or 2 for master or slave). A '1' in this bitmask indicates to disable the interrupt, '0' means to enable it. { */

static void pic_enable_irq(uint8_t irq, unsigned enable) {
  uint8_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
  uint8_t irq_bit = 1 << (irq & 0x7);

  uint8_t value = inb(port);
  if (enable)
    value &= ~irq_bit;
  else
    value |= irq_bit;
                            
  outb(port, value);
}

/** Also acknowledging the IRQ is incredibly important. If you don't send the end of interrupt (EOI) message, you will never receive any more interrupts. This is done simply by sending the value 0x20 to the command port.

    *However*. If you're ACKing an interrupt from the *slave*, you must *also* ACK the master because the master and slave are daisy-chained. { */

static void pic_ack_irq(unsigned num) {
  /* Was this an IRQ from the slave PIC? */
  if (num >= 40)
    /* ACK the slave. */
    outb(PIC2_CMD, 0x20);
  /* Was this an IRQ from the master or slave PICs? */
  if (num >= 32)
    /* ACK the master. */
    outb(PIC1_CMD, 0x20);
}

static void (*ack_irq)(unsigned) = 0;
static void (*enable_irq)(uint8_t, unsigned) = 0;

/** Now we get to initialise interrupt handling. { */

static int init_idt() {
  register_debugger_handler("print-idt", "Print the IDT", &print_idt);
  register_debugger_handler("print-interrupt-handlers",
                            "Print all known interrupt handlers", &print_handlers);

  memset((uint8_t*)&num_handlers, 0, sizeof(unsigned)*NUM_HANDLERS);

  /** Here we disable most entries in the IDT, then fill it out for all the interrupt handlers we care about. We set the DPL of these to ``0`` so they can't be maliciously called by user code! { */

  memset((uint8_t*)entries, 0, sizeof(idt_entry_t)*256);
  for (unsigned i = 0; i < NUM_HANDLERS; ++i)
    set_idt_entry(&entries[i], (uint32_t)_handlers[i], /*CS=*/0x08, /*DPL=*/0x00);

  /** Then we inform the processor about the table in the same way we did for the GDT. { */

  idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
  idt_ptr.base  = (uint32_t)&entries[0];

  __asm volatile("lidt %0" : : "m" (idt_ptr));

  /* FIXME: Implement APIC */
  if (1) {
    pic_init();
    ack_irq = &pic_ack_irq;
    enable_irq = &pic_enable_irq;
  }

  return 0;
}

/** All that is left is some non-machine-specific code that will register and unregister interrupt handlers. If we are registering a handler for an IRQ, this function ensures that it is unmasked and enabled. { */

int register_interrupt_handler(int num, interrupt_handler_t handler, void *p) {
  if (num >= NUM_HANDLERS)
    return -1;
  if (num_handlers[num] >= MAX_HANDLERS_PER_INT)
    return -1;
  handlers[num][num_handlers[num]].handler = handler;
  handlers[num][num_handlers[num]++].p = p;

  if (num >= 32 && enable_irq)
    enable_irq(num-32, 1);

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

    if (num_handlers[num] == 0 && num >= 32 && enable_irq)
      enable_irq(num-32, 0);
    
    return 0;
  }
  return 1;
}

/** Now we get to the interrupt handler - the C part anyway.

    Each handler for each interrupt is different, because the CPU does not provide a way to query which vector interrupt was taken any other way. All these small, separate handlers will converge on a common handler, which will then call this C function.

    This function takes a pointer to the register state; all these registers will be restored after it returns to it can make modifications to the register state too. { */

void interrupt_handler(x86_regs_t *regs) {
  unsigned num = regs->interrupt_num;

  /** The first thing we do is ensure any IRQ is ACK'd. { */
  ack_irq(num);

  /** Then we search for registered interrupt handlers and call them all. { */
  if (num_handlers[num]) {
    for (unsigned i = 0, e = num_handlers[num];
         i != e; ++i)
      handlers[num][i].handler(regs, handlers[num][i].p);
  } else if (num == 3) {
    debugger_trap(regs);
  } else {
    /** If we can't find a handler, we try and invoke the optional kernel debugger. { */
    const char *desc = "";
    if (regs->interrupt_num < NUM_TRAP_STRS)
      desc = trap_strs[num];
    else {
      char buf[32];
      ksnprintf(buf, 32, "Exception #%d", regs->interrupt_num);
      desc = buf;
    }

    debugger_except(regs, desc);
  }
}

static prereq_t prereqs[] = { {"x86/gdt",NULL}, {NULL,NULL} };
static prereq_t load_after[] = { {"debugger",NULL}, {NULL,NULL} };
                                
static module_t x run_on_startup = {
  .name = "interrupts",
  .required = prereqs,
  .load_after = load_after,
  .init = &init_idt,
  .fini = NULL
};
