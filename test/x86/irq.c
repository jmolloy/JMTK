// RUN: %compile %s -o %t && %run %t only-run irq-test 2>&1 | %FileCheck %s
// XFAIL: Hosted
// XFAIL: X64

#if !defined(X86)
# error This test must be run on an x86 bare kernel!
#endif

#include "hal.h"
#include "stdio.h"
#include "x86/io.h"

int n = 0;

int h(struct regs *r, void *p) {
  /* Look for at least 3 runs of this function before stopping. */
  // CHECK: p = deadbeef
  // CHECK: p = deadbeef
  // CHECK: p = deadbeef
  kprintf("p = %x\n", p);

  if (n == 2)
    __asm volatile("cli; hlt");
  ++n;
  return 0;
}

int f () {
  /* Handle IRQ0 = PIT timer. */
  register_interrupt_handler(IRQ(0), &h, (void*)0xdeadbeef);

  __asm volatile("sti");

  for (;;);
  
  return 0;
}

static const char *p[] = {"console", "x86/serial",
                          "interrupts", NULL};

static init_fini_fn_t run_on_startup x = {
  .name = "irq-test",
  .prerequisites = p,
  .fn = &f
};
