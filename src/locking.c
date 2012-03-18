#include "hal.h"

void spinlock_init(spinlock_t *lock) {
  lock->val = 0;
}

void spinlock_acquire(spinlock_t *lock) {
  while (__sync_bool_compare_and_swap(&lock->val, 0, 1) == 0)
    ;
}

int spinlock_tryacquire(spinlock_t *lock) {
  return __sync_bool_compare_and_swap(&lock->val, 0, 1);
}

void spinlock_release(spinlock_t *lock) {
  lock->val = 0;
}
