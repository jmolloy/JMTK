#include "hal.h"
#include "stdio.h"
#include "string.h"

/**
===
GDT
===

This tutorial chapter will deal with defining some of the machine state that GRUB left us with in an undefined state. Particularly, the Global Descriptor Table (GDT) and the list of free memory on the system.

Let's first start with some x86 basics as an introduction.

Memory management in the x86
============================

The x86, like most non-microcontroller architectures, has a memory management unit. This is a piece of hardware that controls how addresses given to the CPU in instructions actually map to addresses on the memory bus and how memory should be protected (read-only, kernel mode only etc).

The x86 has two methods of controlling memory protection and how addresses map to physical addresses; *segmentation* and *paging*.

Segmentation
------------

Back in the 8086 days (and still in 16-bit real mode which the core boots into) memory addressing was done via a *segment-offset pair*, which gave a formula similar to below as the mapping between addresses as written and how they would appear on the physical bus::

    phys_addr = (segment << 4) + offset

This was useful because in a 16-bit machine, an immediate or register can only hold 16 bits, which is only enough to address 64KB of memory. Adding the segment allowed 20 bits of address, which was enough to address 1MB.

An instruction would specify a *segment register* (one of ``ds``,``es``,``fs`` or ``gs``) with its offset; other instructions could load values into these registers.

When 32-bit protected mode was introduced, Intel kept the segment-offset idea and used it for memory protection and partitioning - providing what we now know as virtual memory.

In protected mode the segment registers' behaviour are changed. No longer are they just addresses to be shifted left by 4; they're indices into a table of *segment descriptors*. A segment descriptor describes the *base* of the segment, which was previously ``segment_reg << 4``, and the *limit* of the segment, which was previously ``0xFFFF`` and is actually an *extent*, not a limit (i.e. ``last_usable_address = base + limit``). It also specifies whether the segment is meant to contain code or data, and its *privilege level*. We'll come on to that.

Paging
------

Paging all but deprecates segmentation as the preferred form of memory mapping and protection in the x86. Indeed, in x86-64, a "flat memory model" is expected by some of the new instructions. A flat memory model is effectively disabling segmentation, by giving all segments a base of ``0`` and a limit of ``0xFFFFFFFF``.

We'll come more onto paging later in the virtual memory management chapter, but it splits up the address space into *pages*, which are chunks usually of 4KB. There is a *page table* which for every page in the virtual address space gives a mapping to the physical address space.

Privilege modes
===============

The x86 has four privilege modes. It calls them *rings*, with ring 0 being the most privileged and ring 3 being the least.

Mostly, rings 1 and 2 are unused. It is normal to either be in ring 0 or ring 3. Some hypervisors use rings 1 & 2 because they are conventionally unused by operating systems.

.. ../../../doc/gdt-rings.svg
   :width: 550px
   :height: 300px
*/
/**
Hardware multitasking
=====================

The x86 includes support for hardware multitasking. That is, changing from one process or thread to another on an interrupt. I should mention that this feature is widely unused - software multitasking is actually easier and faster, and because the big operating systems don't use this I doubt it is an optimised codepath in the core any more.

The CPU controls hardware multitasking through a special descriptor called a *Task State Segment descriptor*. This stores most of the state of a task and allows it to be restored.

"It's never used, so why do we care?" I hear you cry. Well, there are two fields that are *always* used, whether you use hardware multitasking or not. These are the ``ss0`` and ``esp0`` fields.

They store the stack segment and stack pointer in ring 0. I.e., they store the location of a task's *kernel stack*. When switching from user mode to kernel mode, we can't be sure that the stack pointer is pointing to a "good" location, so we always switch stacks to a known-good kernel stack. The processor does this for us, and gets the details of the kernel stack from these fields. { */

typedef struct tss_entry {
  uint32_t prev_tss;
  uint32_t esp0, ss0, esp1, ss1, esp2, ss2;
  uint32_t cr3, eip, eflags;
  uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
  uint32_t es, cs, ss, ds, fs, gs;
  uint32_t ldt;
  uint16_t trap, iomap_base;
} tss_entry_t;

/**
Global Descriptor Table
=======================

So what is the GDT and why should you care? The GDT is the descriptor table I mentioned in the previous section. And even though we don't really care about segmentation we still need to do some setup of it:

   1. The processor requires it. And although GRUB has set us up a valid GDT, its location is undefined and may be overwritten at any time.
   2. It is the only way to get the CPU to switch into user mode.
   3. It provides pointers to Task State Segment descriptors (see later) which we need for switching from user to kernel mode.

The GDT is a table of "descriptors", which are 64-bit bitfields each describing a *segment* of memory. A segment has a *base* and a *limit*, which is its extent in memory (extends from ``base`` to ``base+limit``.

A descriptor takes a form like this:

.. image:: ../../../doc/gdt-descriptor.svg
    :width: 500px
    :height: 200px

The fields represented are:

   * **A** ccessed; Set by the CPU when the segment has been read by an instruction fetch or data read depending whether this is a code or data segment.
   * **AV** ai **L** able; Unused by the CPU.
   * **C** onforming; relates to usage of call gates across privilege levels; safely ignored.
   * **D** efault: If '1', 32-bit operands are used by default. Else 16-bit operands are used.
   * **D** escriptor **P** rivilege **Level**; a privilege level the CPU must be in to use this segment. 0 is kernel mode, 3 is user mode. 1 and 2 are rarely used.
   * **E** xpansion direction; Only applicable to stack segments and largely unused.
   * **G** ranularity; If '1', ``base = base * 4096`` and ``limit = limit * 4096``.
   * **R** eadable, **W** ritable; self explanatory
   * **P** resent; Must be set to '1' else the CPU will fault.

{*/

typedef struct gdt_entry {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t  base_mid;
  uint8_t  type : 4;
  uint8_t  s    : 1;            /* 's' should always be 1, except for */
  uint8_t  dpl  : 2;            /* the NULL segment. */
  uint8_t  p    : 1;
  uint8_t  limit_high : 4;
  uint8_t  avail: 1;
  uint8_t  l    : 1;
  uint8_t  d    : 1;
  uint8_t  g    : 1;
  uint8_t  base_high;
} gdt_entry_t;

/** Now, in order to tell the CPU where the GDT is kept, we execute the special instruction ``lgdt`` and give it as an operand a pointer to a special structure { */

typedef struct gdt_ptr {
  uint16_t limit;               /* Size of the GDT */
  uint32_t base;                /* Start of the GDT */
} __attribute__((packed)) gdt_ptr_t;

/** Now we get to filling in the GDT itself. We will need:

      1. [0x0] A NULL descriptor. This is mandatory, and just has all bits set to 0.
      2. [0x8] A code descriptor for the kernel.
      3. [0x10] A data descriptor for the kernel.
      4. [0x18] A code descriptor for user mode.
      5. [0x20] A data descriptor for user mode.
      6. [0x28] A TSS descriptor, and we'll need one for every core (because each core will require a different kernel stack).

    {*/

static gdt_ptr_t gdt_ptr;
static gdt_entry_t entries[MAX_CORES+5];
static tss_entry_t tss_entries[MAX_CORES];

unsigned num_gdt_entries, num_tss_entries;

/** Let's define some helper functions to calculate the base and limit of a GDT entry from the entry itself, and dump a GDT and TSS entry out to console, for debugging { */

static uint32_t base(gdt_entry_t e) {
  return e.base_low | (e.base_mid << 16) | (e.base_high << 24);
}
static uint32_t limit(gdt_entry_t e) {
  return e.limit_low | (e.limit_high << 16);
}

static void print_gdt_entry(unsigned i, gdt_entry_t e) {
  uint32_t *m = (uint32_t*)&e;
  kprintf("#%02d: %08x %08x\n", i, m[0], m[1]);
  kprintf("#%02d: Base %#08x Limit %#08x Type %d\n",
          i, base(e), limit(e), e.type);
  kprintf("     s %d dpl %d p %d l %d d %d g %d\n",
          e.s, e.dpl, e.p, e.l, e.d, e.g);
}

static void print_tss_entry(unsigned i, tss_entry_t e) {
  kprintf("#%02d: esp0 %#08x ss0 %#02x cs %#02x\n"
          "ss %#02x ds %#02x es %#02x fs %#02x gs %#02x\n",
          i, e.esp0, e.ss0, e.cs, e.ss, e.ds, e.es, e.fs, e.gs);
}

static void print_gdt(const char *cmd, core_debug_state_t *states, int core) {
  for (unsigned i = 0; i < num_gdt_entries; ++i)
    print_gdt_entry(i, entries[i]);
}

static void print_tss(const char *cmd, core_debug_state_t *states, int core) {
  for (unsigned i = 0; i < num_tss_entries; ++i)
    print_tss_entry(i, tss_entries[i]);
}

/** Now we get to actually populating a GDT entry. This function takes the base, limit and other flags as arguments and packs them into the given GDT entry struct. { */

static
void set_gdt_entry(gdt_entry_t *e, uint32_t base, uint32_t limit,
                   uint8_t type, uint8_t s, uint8_t dpl, uint8_t p, uint8_t l,
                   uint8_t d, uint8_t g) {
  e->limit_low  = limit & 0xFFFF;
  e->base_low   = base & 0xFFFF;
  e->base_mid   = (base >> 16) & 0xFF;
  e->type       = type & 0xF;
  e->s          = s & 0x1;
  e->dpl        = dpl & 0x3;
  e->p          = p & 0x1;
  e->limit_high = (limit >> 16) & 0xF;
  e->avail      = 0;
  e->l          = l & 0x1;
  e->d          = d & 0x1;
  e->g          = g & 0x1;
  e->base_high  = (base >> 24) & 0xFF;
}

/** Similarly for a TSS entry, but we know we don't care about most of the fields in the TSS, and all of the segment registers we know statically; the data segments should point to our kernel data descriptor (0x10 - index 2 into the GDT) and CS should point to the kernel code descriptor (0x08 - index 1 into the GDT) {*/

static void set_tss_entry(tss_entry_t *e) {
  memset((uint8_t*)e, 0, sizeof(tss_entry_t));
  e->ss0 = e->ss = e->ds = e->es = e->fs = e->gs = 0x10;
  e->cs = 0x08;
}

/** The 'type' field is actually a bitmask, with the 3rd bit representing if this is a code or data segment ('1' = code segment). The other three bits depend on the code/data type. 

    'Conforming' and 'readable' apply to code segments; Conforming is only applicable to call-gates which we don't care about.

    'Expand_Direction' is for stack segments; the hardware can expand a segment in certain circumstances. We ignore this as, much like call-gates, they're often unused. { */

#define TY_CODE 8

/* Applies to code segments */
#define TY_CONFORMING 4
#define TY_READABLE 2

/* Applies to data segments. */
#define TY_DATA_EXPAND_DIRECTION 4
#define TY_DATA_WRITABLE 2

/* Applies to both; set by the CPU. */
#define TY_ACCESSED 1


/** Finally we get to initialise the GDT. We create our 5 code/data descriptors and NumCores TSS descriptors. {*/
static int init_gdt() {
  register_debugger_handler("print-gdt", "Print the GDT", &print_gdt);
  register_debugger_handler("print-tss", "Print all TSS entries", &print_tss);

  /*                         Base Limit Type                 S  Dpl P  L  D  G*/
  set_gdt_entry(&entries[0], 0,  0xFFF0, 0,                  0, 0,  0, 0, 0, 0);
  set_gdt_entry(&entries[1], 0,   ~0U,  TY_CODE|TY_READABLE, 1, 0,  1, 0, 1, 1);
  set_gdt_entry(&entries[2], 0,   ~0U,  TY_DATA_WRITABLE,    1, 0,  1, 0, 1, 1);
  set_gdt_entry(&entries[3], 0,   ~0U,  TY_CODE|TY_READABLE, 1, 3,  1, 0, 1, 1);
  set_gdt_entry(&entries[4], 0,   ~0U,  TY_DATA_WRITABLE,    1, 3,  1, 0, 1, 1);

  int num_processors = get_num_processors();
  if (num_processors == -1)
    num_processors = 1;
  for (int i = 0; i < num_processors; ++i) {
    set_tss_entry(&tss_entries[i]);
    set_gdt_entry(&entries[i+5], (uint32_t)&tss_entries[i],
                                      /* Type                S  Dpl P  L  D  G*/
                  sizeof(tss_entry_t)-1, TY_CODE|TY_ACCESSED,0, 3,  1, 0, 0, 1);
  }

  num_gdt_entries = num_processors + 5;
  num_tss_entries = num_processors;

  /** Then we set up the GDT pointer struct, and inform the CPU about it.

      The 'limit' field is actually the *last valid addressable byte* of the GDT, so it is the size of the GDT - 1.

      We use the ``lgdt`` instruction to load our GDT, then make sure all segment registers are set correctly; data segments to 0x10 and code to 0x08. Note that we can't set ``%cs`` directly - we have to perform a 'far jump' which performs a jump with change of segment. That's what the ``ljmp`` is for. {*/
  gdt_ptr.base = (uint32_t)&entries[0];
  gdt_ptr.limit = sizeof(gdt_entry_t) * num_gdt_entries - 1;

  __asm volatile("lgdt %0;"
                 "mov  $0x10, %%ax;"
                 "mov  %%ax, %%ds;"
                 "mov  %%ax, %%es;"
                 "mov  %%ax, %%fs;"
                 "mov  %%ax, %%gs;"
                 "ljmp $0x08, $1f;"
                 "1:" : : "m" (gdt_ptr) : "eax");

  return 0;
}

static prereq_t prereqs[] = { {"console",NULL}, {"debugger",NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "x86/gdt",
  .required = NULL,
  .load_after = prereqs,
  .init = &init_gdt,
  .fini = NULL
};
