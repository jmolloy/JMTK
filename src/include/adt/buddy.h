#ifndef BUDDY_H
#define BUDDY_H

#include "stdint.h"
#include "adt/xbitmap.h"

/* log2 of the maximum buddy node size. */
#define MAX_BUDDY_SZ_LOG2 28 /* 2^28 = 256MB */
/* log2 of the minimum buddy node size. */
#define MIN_BUDDY_SZ_LOG2 12 /* 2^12 = 4KB */

#define NUM_BUDDY_BUCKETS (MAX_BUDDY_SZ_LOG2 - MIN_BUDDY_SZ_LOG2 + 1)

typedef struct buddy {
  uint64_t start, size;
  xbitmap_t orders[NUM_BUDDY_BUCKETS];
} buddy_t;

int buddy_init(buddy_t *bd, alloc_fn_t alloc, free_fn_t free, void *p,
               range_t r, int start_freed);
uint64_t buddy_alloc(buddy_t *bd, unsigned sz);
void buddy_free_range(buddy_t *bd, range_t range);
void buddy_free(buddy_t *bd, uint64_t addr, unsigned sz);

extern buddy_t kernel_buddy;

#endif
