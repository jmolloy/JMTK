#ifndef X86_HAL_H
#define X86_HAL_H

#define THREAD_STACK_SZ 0x2000  /* 8KB of kernel stack. */

#define X86_PRESENT 0x1
#define X86_WRITE   0x2
#define X86_USER    0x4
#define X86_EXECUTE 0x200
#define X86_COW     0x400

typedef struct address_space {
  uint32_t *directory;
  spinlock_t lock;
} address_space_t;

static inline unsigned get_page_size() {
  return 4096;
}

static inline unsigned get_page_shift() {
  return 12;
}

static inline unsigned get_page_mask() {
  return 0xFFF;
}

static inline uintptr_t round_to_page_size(uintptr_t x) {
  if ((x & 0xFFF) != 0)
    return ((x >> 12) + 1) << 12;
  else
    return x;
}

static inline void abort() {
  for(;;);
}

#include "x86/regs.h"

struct jmp_buf_impl {
  uint32_t esp, ebp, eip, ebx, esi, edi, eflags;
};

typedef struct jmp_buf_impl jmp_buf[1];

static inline void jmp_buf_set_stack(jmp_buf buf, uintptr_t stack) {
  buf[0].esp = stack;
}

static inline void jmp_buf_to_regs(struct regs *r, jmp_buf buf) {
  r->esp = buf[0].esp;
  r->ebp = buf[0].ebp;
  r->eip = buf[0].eip;
  r->ebx = buf[0].ebx;
  r->esi = buf[0].edi;
  r->eflags = buf[0].eflags;
}

#define abort() (void)0

#endif
