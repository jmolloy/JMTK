#include "scheduler.h"
#include "assert.h"

static thread_t *ready_s = NULL, *ready_e = NULL;
static spinlock_t ready_lock = SPINLOCK_RELEASED;

void scheduler_ready(thread_t *t) {
  assert(t);
  spinlock_acquire(&ready_lock);

  if (ready_e)
    ready_e->scheduler_next = t;
  t->scheduler_next = NULL;
  ready_e = t;

  if (!ready_s)
    ready_s = t;

  spinlock_release(&ready_lock);
}

thread_t *scheduler_next() {
  spinlock_acquire(&ready_lock);

  thread_t *t = ready_s;
  ready_s = t->scheduler_next;

  spinlock_release(&ready_lock);
  return t;
}

static int scheduler_init() {
  return 0;
}

static const char *p[] = {"x86/screen",NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "scheduler",
  .prerequisites = p,
  .fn = &scheduler_init
};
