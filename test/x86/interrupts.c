#if 0
exit `$1 $2 | ./test/FileCheck $0`
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

  // CHECK: ... and I'm back!
  kprintf("... and I'm back!");

  return 0;
}

static prereq_t p[] = { {"console",NULL}, {"x86/serial",NULL},
                        {"interrupts",NULL}, {NULL,NULL} };

static module_t run_on_startup x = {
  .name = "interrupts-test",
  .required = p,
  .load_after = NULL,
  .init = &f,
  .fini = NULL
};
module_t *test_module = &x;
