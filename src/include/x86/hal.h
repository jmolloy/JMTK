#ifndef X86_HAL_H
#define X86_HAL_H

typedef struct address_space {
  uint32_t *directory;
  /* FIXME: Put a lock here. */
} address_space_t;

static inline void abort() {
  for(;;);
}

#include "x86/regs.h"

typedef struct {
  uint32_t esp, eip, ebx, esi, edi, eflags;
} jmp_buf;

static inline void far_call(void *fn, uintptr_t stack) {
  __asm__ volatile("mov %0, %%esp;"
                   "call *%1" : : "r" (stack), "r" (fn) );
}

#define abort() (void)0

#endif
