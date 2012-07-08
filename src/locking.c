#include "assert.h"
#include "hal.h"
#include "kmalloc.h"
#include "stdlib.h"
#include "thread.h"

void spinlock_init(spinlock_t *lock) {
  lock->val = 0;
  lock->interrupts = 0;
}

void spinlock_acquire(spinlock_t *lock) {
  int interrupts = get_interrupt_state();

  disable_interrupts();
  while (__sync_bool_compare_and_swap(&lock->val, 0, 1) == 0)
    ;

  lock->interrupts = interrupts;
}

void spinlock_release(spinlock_t *lock) {
  while (__sync_bool_compare_and_swap(&lock->val, 1, 0) == 0)
    ;
  set_interrupt_state(lock->interrupts);
}

void semaphore_init(semaphore_t *s) {
  s->val = 0;
  spinlock_init(&s->queue_lock);
  s->queue_head = NULL;
}

semaphore_t *semaphore_new() {
  /* FIXME: Make this a slab cache. */
  semaphore_t *p = kmalloc(sizeof(semaphore_t));
  semaphore_init(p);
  return p;
}

void semaphore_wait(semaphore_t *s) {
  assert(s && "NULL semaphore given!");

  while (1) {
    /* First, try and decrement the semaphore until it hits zero. */
    unsigned val;
    while ( (val = s->val) != 0) {
      if (__sync_bool_compare_and_swap(&s->val, val, val-1) == 1)
        /* Compare and swap succeeded - we decremented val. */
        return;
    }

    /* Semaphore is at zero, we must sleep. */
    spinlock_acquire(&s->queue_lock);
    thread_t *t = thread_current();
    t->semaphore_next = s->queue_head;
    s->queue_head = t;
    spinlock_release(&s->queue_lock);

    thread_sleep();

  }
}

void semaphore_signal(semaphore_t *s) {
  assert(s && "NULL semaphore given!");

  /* FIXME: The current implementation is not FIFO, but LIFO, so
     thread starvation could occur. */
  __sync_fetch_and_add(&s->val, 1);

  /* Wake one thread from the queue. */
  spinlock_acquire(&s->queue_lock);
  thread_t *t = s->queue_head;
  if (t)
    s->queue_head = t->semaphore_next;
  spinlock_release(&s->queue_lock);
  
  if (t)
    thread_wake(t);
}

void rwlock_init(rwlock_t *l) {
  semaphore_init(&l->r);
  semaphore_init(&l->w);
  semaphore_signal(&l->r);
  semaphore_signal(&l->w);
  spinlock_init(&l->lock);
  l->readcount = l->writecount = 0;
}

void rwlock_read_acquire(rwlock_t *l) {
  assert(l);

  spinlock_acquire(&l->lock);
  semaphore_wait(&l->r);
  if (__sync_fetch_and_add(&l->readcount, 1) == 0)
    semaphore_signal(&l->w);
  semaphore_signal(&l->r);
  spinlock_release(&l->lock);
}

void rwlock_read_release(rwlock_t *l) {
  assert(l);
  if (__sync_fetch_and_sub(&l->readcount, 1) == 1)
    semaphore_signal(&l->w);
}

void rwlock_write_acquire(rwlock_t *l) {
  if (__sync_fetch_and_add(&l->writecount, 1) == 0)
    semaphore_wait(&l->r);

  semaphore_wait(&l->w);
}

void rwlock_write_release(rwlock_t *l) {
  semaphore_signal(&l->w);

  if (__sync_fetch_and_sub(&l->writecount, 1) == 1)
    semaphore_signal(&l->r);
}
