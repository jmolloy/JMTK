#ifndef HOSTED_HAL_H
#define HOSTED_HAL_H

#include <setjmp.h>

typedef struct address_space {
  uint32_t a[1<<20];
} address_space_t;

/* For abort() */
extern void abort() __attribute__((noreturn));

static inline void far_call(void *fn, uintptr_t stack) {
  __asm__ volatile("mov %0, %%rsp;"
                   "call *%1" : : "rm" (stack), "r" (fn) );
}

#endif
