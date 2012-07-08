#ifndef HOSTED_HAL_H
#define HOSTED_HAL_H

#define THREAD_STACK_SZ 0x10000  /* 64KB of kernel stack. */

typedef struct address_space {
  uint32_t a[1<<20];
  spinlock_t lock;
} address_space_t;

/* For abort() */
extern void abort() __attribute__((noreturn));

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

struct regs {
};

struct jmp_buf_impl {
  uint64_t rsp, rbp, rip, rbx, rsi, rdi, rflags,
    r8, r9, r10, r11, r12, r13, r14, r15;
};

typedef struct jmp_buf_impl jmp_buf[1];

static inline void jmp_buf_set_stack(jmp_buf buf, uintptr_t stack) {
  buf[0].rsp = stack;
}

static inline void jmp_buf_to_regs(struct regs *r, jmp_buf buf) {
}

#endif
