#if 0
exit `$1 $2 | ./test/FileCheck $0`
#endif

#include "hal.h"
#include "stdio.h"
#include "thread.h"

// CHECK: g(): start
// CHECK: f(): signalling
// CHECK: g(): wait() returned
// CHECK: g(): signalled()
// CHECK: f(): wait()ing
// CHECK: f(): waited!

static void g (void *p) {
  semaphore_t *s = (semaphore_t*)p;

  kprintf("g(): start\n");
  semaphore_wait(s);
  kprintf("g(): wait() returned\n");
  semaphore_signal(s);
  kprintf("g(): signalled()\n");
}

static int f () {
  semaphore_t s;
  semaphore_init(&s);

  thread_spawn(&g, (void*)&s, 0);
  thread_yield();

  kprintf("f(): signalling\n");
  semaphore_signal(&s);

  thread_yield();

  kprintf("f(): wait()ing\n");
  semaphore_wait(&s);
  kprintf("f(): waited!");

  return 0;
}

static prereq_t p[] = { {"console",NULL}, {"x86/serial",NULL},
                        {"hosted/console",NULL}, {NULL,NULL} };
static prereq_t p2[] = { {"threading",NULL}, {NULL,NULL} };

static module_t run_on_startup x = {
  .name = "semaphore-test",
  .required = p2,
  .load_after = p,
  .init = &f,
  .fini = NULL
};
module_t *test_module = &x;
