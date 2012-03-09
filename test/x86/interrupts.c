// RUN: %compile %s -o %t && %run %t only-run interrupts-test 2>&1 | %FileCheck %s
// XFAIL: Hosted
// XFAIL: X64

#if !defined(X86)
# error This test must be run on an x86 bare kernel!
#endif

#include "hal.h"
#include "stdio.h"

int h(struct regs *r, void *p) {
  // CHECK: p = deadbeef
  // CHECK: p = deadbaba
  kprintf("p = %x\n", p);
  return 0;
}

int f () {
  register_interrupt_handler(2, &h, (void*)0xdeadbeef);
  register_interrupt_handler(2, &h, (void*)0xdeadbaba);

  __asm volatile("int $2");
  return 0;
}

static const char *p[] = {"console", "x86/serial",
                          "interrupts", NULL};

static init_fini_fn_t run_on_startup x = {
  .name = "interrupts-test",
  .prerequisites = p,
  .fn = &f
};
