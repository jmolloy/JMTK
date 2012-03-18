#include "hal.h"

void spinlock_init(spinlock_t *lock) {
  lock->val = 0;
  lock->interrupts = 0;
}

void spinlock_acquire(spinlock_t *lock) {
  lock->interrupts = get_interrupt_state();
  disable_interrupts();
  while (__sync_bool_compare_and_swap(&lock->val, 0, 1) == 0)
    ;
}

void spinlock_release(spinlock_t *lock) {
  lock->val = 0;
  set_interrupt_state(lock->interrupts);
}
