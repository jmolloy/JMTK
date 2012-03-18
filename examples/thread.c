#include "hal.h"
#include "thread.h"
#include "stdio.h"

void g(void *p) {
  kprintf("g: p = %x\n", p);
  thread_yield();
  kprintf("About to die!\n");
}

int f() {
  thread_t *t = thread_current();
  kprintf("Current: %p\n", t);

  t = thread_spawn(g, (void*)0x1234, 1);

  thread_yield();

  thread_yield();
  
  return 0;
}

static const char *p[] = {"x86/screen", "x86/keyboard", "x86/serial",
                          "threading", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "thread-example",
  .prerequisites = p,
  .fn = &f
};
