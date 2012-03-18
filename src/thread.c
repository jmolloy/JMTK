#include "hal.h"
#include "thread.h"
#include "slab.h"
#include "assert.h"
#include "scheduler.h"
#include "stdio.h"

/* The slab cache for thread_t objects. */
static slab_cache_t thread_cache;

static thread_t *thread_list_head = NULL;
static spinlock_t thread_list_lock;

#define CANARY_VAL 0x4321abcd

static void inspect_threads(const char *cmd, core_debug_state_t *states, int core_num) {
  thread_t *t = thread_list_head;
  while (t) {
    struct regs r;
    jmp_buf_to_regs(&r, t->jmpbuf);

    uintptr_t data = NULL;
    uintptr_t pc = backtrace(&data, &r);
    int offs;
    const char *sym = lookup_kernel_symbol(pc, &offs);

    const char *state_str;
    switch (t->state) {
    case THREAD_READY: state_str = "READY"; break;
    case THREAD_RUN: state_str = "RUN"; break;
    case THREAD_SLEEP: state_str = "SLEEP"; break;
    case THREAD_DEAD: state_str = "DEAD"; break;
    default: state_str = "UNKNOWN"; break;
    }

    if (sym)
      kprintf("#%3d: %-5s [%s+%d]\n", t->id, state_str, sym, offs);
    else
      kprintf("#%3d: %-5s 0x%x\n", t->id, state_str, pc);
    
    t = t->next;
  }
}

static uintptr_t alloc_stack_and_tls() {
  unsigned pagesz = get_page_size();

  uintptr_t addr = vmspace_alloc(&kernel_vmspace, THREAD_STACK_SZ, 0);

  for (unsigned i = 0; i < THREAD_STACK_SZ; i += pagesz)
    map(addr+i, alloc_page(PAGE_REQ_NONE), 1, PAGE_WRITE);

  return addr;
}

static void free_stack_and_tls(uintptr_t stack) {
  unsigned pagesz = get_page_size();

  unsigned flags;
  for (unsigned i = 0; i < THREAD_STACK_SZ; i += pagesz) {
    free_page(get_mapping(stack+i, &flags));
    unmap(stack+i, 1);
  }
}

static void yield() {
  thread_t *t = scheduler_next();

  if (!t) return;
  if (t->request_kill) {
    t->state = THREAD_DEAD;
    yield();
    assert(0 && "Unreachable!");
    return;
  }

  t->state = THREAD_RUN;
  longjmp(t->jmpbuf, 1);
}

static void trampoline() __attribute__((noreturn,noinline));
static void trampoline() {
  void (*fn)(void*) = (void (*)(void*)) *thread_tls_slot(1);
  void *p = (void*) *thread_tls_slot(2);

  fn(p);

  thread_t *t = thread_current();
  t->state = THREAD_DEAD;

  yield();
  assert(0 && "Unreachable!");
  for (;;) ;
}

static uintptr_t *tls_slot(unsigned idx, uintptr_t stack_pointer) { 
  uintptr_t *tls = (uintptr_t*) (stack_pointer & ~(THREAD_STACK_SZ-1));
  return &tls[idx];
}

uintptr_t *thread_tls_slot(unsigned idx) {
  /* __builtin_frame_address is a platform-agnostic way to get a pointer on to
     the stack. */
  return tls_slot(idx, (uintptr_t)__builtin_frame_address(0));
}

thread_t *thread_current() {
  return (thread_t*) *thread_tls_slot(TLS_SLOT_TCB);
}

thread_t *thread_spawn(void (*fn)(void*), void *p, uint8_t auto_free) {
  thread_t *t = (thread_t*)slab_cache_alloc(&thread_cache);

  t->auto_free = auto_free;
  t->stack = alloc_stack_and_tls();
 
  spinlock_acquire(&thread_list_lock);
  t->next = thread_list_head;
  t->next->prev = t;
  thread_list_head = t;
  spinlock_release(&thread_list_lock);
 
  /* TLS slot zero always contains the thread object. */
  *tls_slot(TLS_SLOT_TCB, t->stack) = (uintptr_t)t;

  /* Store the function and argument temporarily in TLS */
  *tls_slot(1, t->stack) = (uintptr_t)fn;
  *tls_slot(2, t->stack) = (uintptr_t)p;

  /* In the last valid TLS slot, store a canary. */
  *tls_slot(TLS_SLOT_CANARY, t->stack) = CANARY_VAL;

  if (setjmp(t->jmpbuf) == 0) {
    jmp_buf_set_stack(t->jmpbuf, t->stack + THREAD_STACK_SZ);

    scheduler_ready(t);

    return t;
  } else {
    /* Tail call to trampoline which is defined as noinline, to force the creation
       of a new stack frame as the previous stack frame is now invalid! */
    trampoline();
  }
}

void thread_destroy(thread_t *t) {
  spinlock_acquire(&thread_list_lock);
  t->next->prev = t->prev;
  t->prev->next = t->next;
  spinlock_release(&thread_list_lock);

  free_stack_and_tls(t->stack);
  slab_cache_free(&thread_cache, (void*)t);
}  

void thread_sleep() {
  thread_t *t = thread_current();
  t->state = THREAD_SLEEP;
  if (setjmp(t->jmpbuf) == 0) {
    if (t->request_kill)
      t->state = THREAD_DEAD;
    yield();
  }
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
  if (setjmp(t->jmpbuf) == 0) {
    if (t->request_kill)
      t->state = THREAD_DEAD;
    else
      scheduler_ready(t);
    yield();
  }
}

void thread_kill(thread_t *t) {
  __sync_bool_compare_and_swap(&t->request_kill, 0, 1);
}

static int threading_init() {
  static thread_t dummy_t = {
    .id = 0,
    .prev = NULL, .next = NULL,
    .scheduler_next = NULL,
    .semaphore_next = NULL,
    .stack = 0,
    .request_kill = 0,
    .state = 0,
    .priority = 0,
    .auto_free = 0
  };

  int r = slab_cache_create(&thread_cache, &kernel_vmspace, sizeof(thread_t), (void*)&dummy_t);
  assert(r == 0 && "slab_cache_create failed!");

  thread_t *t = (thread_t*)slab_cache_alloc(&thread_cache);
  t->stack = (uintptr_t)__builtin_frame_address(0) & ~(THREAD_STACK_SZ-1);

  *tls_slot(TLS_SLOT_TCB, t->stack) = (uintptr_t)t;
  *tls_slot(TLS_SLOT_CANARY, t->stack) = CANARY_VAL;

  thread_list_head = t;
  spinlock_init(&thread_list_lock);

  register_debugger_handler("threads", "List all thread states", &inspect_threads);
  
  return 0;
}

static const char *p[] = {"kmalloc", "scheduler", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "threading",
  .prerequisites = p,
  .fn = &threading_init
};
