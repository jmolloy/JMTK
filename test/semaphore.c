// RUN: %compile %s -o %t && %run %t only-run semaphore-test 2>&1 | %FileCheck %s

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

static const char *p[] = {"console", "x86/serial", "hosted/console",
                          "threading", NULL};

static init_fini_fn_t run_on_startup x = {
  .name = "semaphore-test",
  .prerequisites = p,
  .fn = &f
};
