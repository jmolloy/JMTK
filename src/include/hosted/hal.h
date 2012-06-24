#ifndef HOSTED_HAL_H
#define HOSTED_HAL_H

typedef struct address_space {
  uint32_t a[1<<20];
  spinlock_t lock;
} address_space_t;

/* For abort() */
extern void abort() __attribute__((noreturn));

static inline unsigned get_page_size() {
  return 4096;
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
  uint32_t rsp, rbp, rip, rbx, rsi, rdi, rflags;
};

typedef struct jmp_buf_impl jmp_buf[1];

static inline void jmp_buf_set_stack(jmp_buf buf, uintptr_t stack) {
  buf[0].rsp = stack;
}

static inline void jmp_buf_to_regs(struct regs *r, jmp_buf buf) {
}

#endif
