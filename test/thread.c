#if 0
exit `$1 $2 | ./test/FileCheck $0`
#endif

#include "hal.h"
#include "stdio.h"
#include "thread.h"

static void g(void*);
static void h(void*);
static void i(void*);

static int f () {
  thread_t *t = thread_current();
  // CHECK: t: 0x{{[1-f]}}
  kprintf("t: %p\n", t);

  t = thread_spawn(&g, (void*)0x1234, 0);
  thread_yield();
  // CHECK: g: p = 1234

  // CHECK: a
  // CHECK: b
  // CHECK: a
  // CHECK: b
  // CHECK: a
  // CHECK: b
  // CHECK: a
  // CHECK: b

  // CHECK: About to die!
  for (unsigned i = 0; i < 4; ++i) {
    kprintf("b\n");
    thread_yield();
  }
  // CHECK: End!
  kprintf("End!\n");
  thread_yield();
  thread_yield();

  thread_destroy(t);

  // CHECK: h: sleeping!
  t = thread_spawn(&h, NULL, 0);
  thread_yield();
  thread_yield();
  thread_yield();
  thread_yield();

  // CHECK: waking: 0
  kprintf("waking: %d\n", thread_wake(t));

  // CHECK: h: woken!
  thread_yield();
  
  thread_destroy(t);

  t = thread_spawn(&i, NULL, 0);
  thread_yield();

  // CHECK: i: started!
  kprintf("Killing\n");
  thread_kill(t);
  // CHECK-NOT: i: alive!
  thread_yield();
  thread_yield();
  thread_yield();

  // CHECK: end
  kprintf("end\n");

  return 0;
}

static void g(void *p) {
  kprintf("g: p = %x\n", p);

  for (unsigned i = 0; i < 4; ++i) {
    kprintf("a\n");
    thread_yield();
  }
  kprintf("\nAbout to die!\n");
}

static void h(void *p) {
  kprintf("h: sleeping!\n");
  thread_sleep();
  kprintf("h: woken!\n");
}

static void i(void *p) {
  kprintf("i: started!\n");
  thread_yield();
  kprintf("i: alive!\n");
}

static prereq_t p[] = { {"console",NULL}, {"x86/serial",NULL},
                        {"hosted/console",NULL}, {NULL,NULL} };
static prereq_t p2[] = { {"threading",NULL}, {NULL,NULL} };

static module_t run_on_startup x = {
  .name = "thread-test",
  .load_after = p,
  .required = p2,
  .init = &f,
  .fini = NULL
};
module_t *test_module = &x;
