#include "hal.h"
#include "mmap.h"

static spinlock_t lock = SPINLOCK_RELEASED;

typedef struct stack {
  uint64_t *base, *addr, *limit, *max;
} stack_t;

static stack_t stacks[3] = {
  {.base = (uint64_t*)MMAP_PMM_STACK0, .addr = 0, .limit = 0,
   .max = (uint64_t*)MMAP_PMM_STACK1},
  {.base = (uint64_t*)MMAP_PMM_STACK1, .addr = 0, .limit = 0,
   .max = (uint64_t*)MMAP_PMM_STACK2},
  {.base = (uint64_t*)MMAP_PMM_STACK2, .addr = 0, .limit = 0,
   .max = (uint64_t*)MMAP_PMM_STACKEND} };

static void stack_push(stack_t *stack, uint64_t value) {
  if (stack->addr == 0) {
    if (map((uintptr_t)stack->base, value, 1, PAGE_WRITE) == -1)
      panic("map() failed!");

    stack->addr = stack->base;
    stack->limit = stack->addr + 0x1000/sizeof(uint64_t);
  } else if (stack->addr >= stack->limit) {
    if (map((uintptr_t)stack->addr, value, 1, PAGE_WRITE) == -1)
      panic("map failed!");
    stack->limit += 0x1000/sizeof(uint64_t);
  } else {
    *stack->addr++ = value;
  }
}

static uint64_t stack_pop(stack_t *stack) {
  if (stack->addr == 0)
    return ~0ULL;
  
  if (stack->addr == stack->base)
    return ~0ULL;
  return *--stack->addr;
}

uint64_t alloc_page(int req) {
  spinlock_acquire(&lock);
  
  uint64_t val = stack_pop(&stacks[req]);

  if (val == ~0ULL && req == PAGE_REQ_NONE)
    val = stack_pop(&stacks[PAGE_REQ_UNDER4GB]);

  spinlock_release(&lock);
  return val;
}

int free_page(uint64_t page) {
  int req;
  if (page < 0x100000)
    req = PAGE_REQ_UNDER1MB;
  else if (sizeof(void*) == 4 || page < 0x10000000UL)
    req = PAGE_REQ_UNDER4GB;
  else
    req = PAGE_REQ_NONE;

  spinlock_acquire(&lock);
  stack_push(&stacks[req], page);
  spinlock_release(&lock);

  return 0;
}
