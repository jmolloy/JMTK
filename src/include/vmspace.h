#ifndef VMSPACE_H
#define VMSPACE_H

#include "stdint.h"
#include "adt/xbitmap.h"

/* log2 of the maximum buddy node size. */
#define MAX_BUDDY_SZ_LOG2 28 /* 2^28 = 256MB */
/* log2 of the minimum buddy node size. */
#define MIN_BUDDY_SZ_LOG2 12 /* 2^12 = 4KB */

typedef struct vmspace {
  uintptr_t start, size;
  xbitmap_t *orders[MAX_BUDDY_SZ_LOG2-MIN_BUDDY_SZ_LOG2];
} vmspace_t;

uintptr_t vmspace_alloc(vmspace_t *vms, unsigned sz, int alloc_phys);
void vmspace_free(vmspace_t *vms, unsigned sz, uintptr_t addr, int free_phys);

#endif
