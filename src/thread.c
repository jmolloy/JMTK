#include "hal.h"
#include "thread.h"
#include "slab.h"
#include "assert.h"
#include "scheduler.h"

/* The slab cache for thread_t objects. */
slab_cache_t thread_cache;

static uintptr_t alloc_stack_and_tls() {
  unsigned pagesz = get_page_size();

  unsigned tlssz = THREAD_STACK_SZ >> 1;
  unsigned canarysz = THREAD_STACK_SZ >> 1;

  assert((tlssz & (pagesz-1)) == 0 &&
         "Thread stack size was not large enough to halve and be page aligned!");

  uintptr_t addr = vmspace_alloc(&kernel_vmspace, THREAD_STACK_SZ*2, 0);

  for (unsigned i = 0; i < THREAD_STACK_SZ>>1; i += pagesz)
    map(addr+i, alloc_page(PAGE_REQ_NONE), 1, PAGE_WRITE);
  /* FIXME: map a canary page at addr+tlssz. */
  for (unsigned i = 0; i < THREAD_STACK_SZ; i += pagesz)
    map(addr+tlssz+canarysz+i, alloc_page(PAGE_REQ_NONE), 1, PAGE_WRITE);

  return addr + tlssz + canarysz;
}

static void free_stack_and_tls(uintptr_t stack) {
  unsigned pagesz = get_page_size();

  uintptr_t tls = stack & ~(THREAD_STACK_SZ*2-1);
  /*uintptr_t canary = tls + pagesz;*/

  unsigned flags;
  for (unsigned i = 0; i < THREAD_STACK_SZ>>1; i += pagesz) {
    free_page(get_mapping(tls+i, &flags));
    unmap(tls+i, 1);
  }

  /* FIXME: unmap canary page. */

  for (unsigned i = 0; i < THREAD_STACK_SZ; i += pagesz) {
    free_page(get_mapping(stack+i, &flags));
    unmap(stack+i, 1);
  } 
}

static void yield() {
  thread_t *t = scheduler_next();

  t->state = THREAD_RUN;
  longjmp(t->jmpbuf, 1);
}

static void trampoline() {
  void (*fn)(void*) = (void (*)(void*)) *thread_tls_slot(1);
  void *p = (void*) *thread_tls_slot(2);

  fn(p);

  thread_t *t = thread_current();
  t->state = THREAD_DEAD;
  free_stack_and_tls(t->stack);

  if (t->auto_free)
    slab_cache_free(&thread_cache, (void*)t);

  yield();
}

static uintptr_t *tls_slot(unsigned idx, uintptr_t stack_pointer) { 
  assert(idx < (THREAD_STACK_SZ/2)/sizeof(uintptr_t) && "TLS index out of range!");
  uintptr_t *tls = (uintptr_t*) (stack_pointer & ~(THREAD_STACK_SZ*2-1));
  return &tls[idx];
}

uintptr_t *thread_tls_slot(unsigned idx) {
  /* __builtin_frame_address is a platform-agnostic way to get a pointer on to
     the stack. */
  return tls_slot(idx, (uintptr_t)__builtin_frame_address(0));
}

thread_t *thread_current() {
  return (thread_t*) *thread_tls_slot(0);
}

thread_t *thread_spawn(void (*fn)(void*), void *p, uint8_t auto_free) {
  thread_t *t = (thread_t*)slab_cache_alloc(&thread_cache);

  t->auto_free = auto_free;
  t->stack = alloc_stack_and_tls();
  
  /* TLS slot zero always contains the thread object. */
  *tls_slot(0, t->stack) = (uintptr_t)t;

  /* Store the function and argument temporarily in TLS */
  *tls_slot(1, t->stack) = (uintptr_t)fn;
  *tls_slot(2, t->stack) = (uintptr_t)p;

  if (setjmp(t->jmpbuf) == 0) {
    scheduler_ready(t);
    return t;
  } else {
    far_call(&trampoline, t->stack+THREAD_STACK_SZ);
    assert(0 && "unreachable!");
  }
}

void thread_sleep() {
  thread_t *t = thread_current();
  t->state = THREAD_SLEEP;
  yield();
}

int thread_wake(thread_t *t) {
  if (__sync_bool_compare_and_swap(&t->state, THREAD_SLEEP, THREAD_READY) == 1) {
    scheduler_ready(t);
    return 0;
  }
  return -1;
}

void thread_yield() {
  thread_t *t = thread_current();
  scheduler_ready(t);
  yield();
}

void thread_kill(thread_t *t) {
  __sync_bool_compare_and_swap(&t->request_kill, 0, 1);
}
