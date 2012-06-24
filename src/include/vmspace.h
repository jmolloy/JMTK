#ifndef VMSPACE_H
#define VMSPACE_H

#include "stdint.h"
#include "adt/buddy.h"

typedef struct vmspace {
  uintptr_t start;
  uintptr_t size;
  buddy_t allocator;
  spinlock_t lock;
} vmspace_t;

int vmspace_init(vmspace_t *vms, uintptr_t addr, uintptr_t sz);
uintptr_t vmspace_alloc(vmspace_t *vms, unsigned sz, int alloc_phys);
void vmspace_free(vmspace_t *vms, unsigned sz, uintptr_t addr, int free_phys);

extern vmspace_t kernel_vmspace;

#endif
