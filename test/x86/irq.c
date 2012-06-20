#if 0
exit `$1 $2 | ./test/FileCheck $0`
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

static prereq_t p[] = { {"console",NULL}, {"x86/serial",NULL},
                        {"interrupts",NULL}, {NULL,NULL} };

static module_t run_on_startup x = {
  .name = "irq-test",
  .required = p,
  .init = &f,
  .fini = NULL
};
module_t *test_module = &x;
