#include "hal.h"
#include "stdio.h"
#include "string.h"

typedef struct gdt_entry {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t  base_mid;
  uint8_t  type : 4;
  uint8_t  s    : 1;
  uint8_t  dpl  : 2;
  uint8_t  p    : 1;
  uint8_t  limit_high : 4;
  uint8_t  avail: 1;
  uint8_t  l    : 1;
  uint8_t  d    : 1;
  uint8_t  g    : 1;
  uint8_t  base_high;
} gdt_entry_t;

typedef struct tss_entry {
  uint32_t prev_tss;
  uint32_t esp0, ss0, esp1, ss1, esp2, ss2;
  uint32_t cr3, eip, eflags;
  uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
  uint32_t es, cs, ss, ds, fs, gs;
  uint32_t ldt;
  uint16_t trap, iomap_base;
} tss_entry_t;

typedef struct gdt_ptr {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

static gdt_ptr_t gdt_ptr;
static gdt_entry_t entries[MAX_CORES+5];
static tss_entry_t tss_entries[MAX_CORES];

unsigned num_gdt_entries, num_tss_entries;

static uint32_t base(gdt_entry_t e) {
  return e.base_low | (e.base_mid << 16) | (e.base_high << 24);
}
static uint32_t limit(gdt_entry_t e) {
  return e.limit_low | (e.limit_high << 16);
}

static void print_gdt_entry(unsigned i, gdt_entry_t e) {
  uint32_t *m = (uint32_t*)&e;
  kprintf("#%02d: %08x %08x\n", i, m[0], m[1]);
  kprintf("#%02d: Base %#08x Limit %#08x Type %d\n", i, base(e), limit(e), e.type);
  kprintf("     s %d dpl %d p %d l %d d %d g %d\n", e.s, e.dpl, e.p, e.l, e.d, e.g);
}

static void print_tss_entry(unsigned i, tss_entry_t e) {
  kprintf("#%02d: esp0 %#08x ss0 %#02x cs %#02x\n     ss %#02x ds %#02x es %#02x fs %#02x gs %#02x\n",
          i, e.esp0, e.ss0, e.cs, e.ss, e.ds, e.es, e.fs, e.gs);
}

static void print_gdt(const char *cmd, core_debug_state_t *states) {
  for (unsigned i = 0; i < num_gdt_entries; ++i)
    print_gdt_entry(i, entries[i]);
}

static void print_tss(const char *cmd, core_debug_state_t *states) {
  for (unsigned i = 0; i < num_tss_entries; ++i)
    print_tss_entry(i, tss_entries[i]);
}

static void set_gdt_entry(gdt_entry_t *e, uint32_t base, uint32_t limit,
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

static void set_tss_entry(tss_entry_t *e) {
  memset((uint8_t*)e, 0, sizeof(tss_entry_t));
  e->ss0 = e->ss = e->ds = e->es = e->fs = e->gs = 0x10;
  e->cs = 0x08;
}

#define TY_CODE 8

#define TY_CONFORMING 4
#define TY_READABLE 2

#define TY_DATA_EXPAND_DIRECTION 4
#define TY_DATA_WRITABLE 2

#define TY_ACCESSED 1

static int init_gdt() {
  register_debugger_handler("print-gdt", "Print the GDT", &print_gdt);
  register_debugger_handler("print-tss", "Print all TSS entries", &print_tss);

  /*                         Base Limit Type                 S  Dpl P  L  D  G*/
  set_gdt_entry(&entries[0], 0,   0xFFF0,    0,                   0, 0,  0, 0, 0, 0);
  set_gdt_entry(&entries[1], 0,   ~0U,  TY_CODE|TY_READABLE, 1, 0,  1, 0, 1, 1);
  set_gdt_entry(&entries[2], 0,   ~0U,  TY_DATA_WRITABLE|TY_ACCESSED,    1, 0,  1, 0, 1, 1);
  set_gdt_entry(&entries[3], 0,   ~0U,  TY_CODE|TY_READABLE, 1, 3,  1, 0, 1, 1);
  set_gdt_entry(&entries[4], 0,   ~0U,  TY_DATA_WRITABLE,    1, 3,  1, 0, 1, 1);

  int num_processors = get_num_processors();
  if (num_processors == -1)
    num_processors = 1;
  for (int i = 0; i < num_processors; ++i) {
    set_tss_entry(&tss_entries[i]);
    set_gdt_entry(&entries[i+5], (uint32_t)&tss_entries[i],
                  sizeof(tss_entry_t)-1, TY_CODE|TY_ACCESSED,0, 3,  1, 0, 0, 1);
                                      /* Type                S  Dpl P  L  D  G*/
  }

  num_gdt_entries = num_processors + 5;
  num_tss_entries = num_processors;

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

static const char *prereqs[] = {"console", "debugger", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "x86/gdt",
  .prerequisites = prereqs,
  .fn = &init_gdt
};
