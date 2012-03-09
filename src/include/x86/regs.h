#ifndef X86_REGS_H
#define X86_REGS_H

typedef struct regs {
  uint32_t ds;
  uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
  uint32_t interrupt_num, error_code;
  uint32_t eip, cs, eflags, useresp, ss;
} x86_regs_t;

#endif
