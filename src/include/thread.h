#ifndef THREAD_H
#define THREAD_H

#include "hal.h"

#define THREAD_READY 0
#define THREAD_RUN   1
#define THREAD_SLEEP 2
#define THREAD_DEAD  3

#define TLS_SLOT_TCB 0    /* TLS slot index for the thread control block (thread_t*) */
#define TLS_SLOT_LAST 8   /* Final valid TLS slot entry. */
#define TLS_SLOT_CANARY 9 /* Used internally to detect stack overrun. */

/* A thread in the system. This struct should be as lightweight as possible.

   None of this state must be mutated outside of the thread_* functions. */
typedef struct thread {
  /* Thread ID - *not* guaranteed to be unique in the system. */
  unsigned id;

  /* Intrusive doubly linked list for iterating through all known
     threads. */
  struct thread *prev, *next;

  /* Intrusive linked list for the scheduler's use. */
  struct thread *scheduler_next;

  /* Intrusive linked list for when this thread is queued
     on a semaphore or condvar.

     If queued on a semaphore, this gives the next thread in the
     queue. */
  struct thread *semaphore_next;

  /* Jump buffer to longjmp to. */
  jmp_buf jmpbuf;
  
  /* Stack base (lowest address in memory) */
  uintptr_t stack;

  /* Atomically increment to force this thread to stop when next preempted. */
  uintptr_t request_kill;

  /* Thread state */
  volatile uintptr_t state;

  /* Thread priority (0 = highest) */
  uint8_t priority;

  /* Free the thread_t object on finish? */
  uint8_t auto_free : 1;
} thread_t;

/* Creates a new thread object, starts it, and returns it.

   'fn' is the function to run, and to which 'p' is passed. The thread
   has a stack allocated and reserved for it.

   If 'auto_free' is nonzero, the thread will free all its resources when
   it finishes. Else it will free all its resources except the thread_t*
   itself. */
thread_t *thread_spawn(void (*fn)(void*), void *p, uint8_t auto_free);

/* Destroys a thread created with thread_spawn. */
void thread_destroy(thread_t *t);

/* Requests that the given thread be killed at the next opportunity. */
void thread_kill(thread_t *t);

/* Return the current thread. */
thread_t *thread_current();

/* Puts the current thread to sleep. It can be woken with thread_wake(). */
void thread_sleep();

/* Wakes the given thread. If the thread was in sleep mode and was woken,
   return 0. Else, return -1. */
int thread_wake(thread_t *t);

/* Yield execution resources to another thread. */
void thread_yield();

/* Returns a pointer to the 'idx'th entry in thread local storage. */
uintptr_t *thread_tls_slot(unsigned idx);

#endif
