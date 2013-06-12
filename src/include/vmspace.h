/**#1
   =================
    The kernel heap
   =================

   Now we get to write our main kernel memory allocator - ``kmalloc()``.

   This is the allocator that will be used liberally around the kernel for dynamically allocating many kinds of objects and buffers. As such it has to deal with allocation sizes ranging from tiny to very large.

   The input sizes to ``kmalloc`` can almost be split into two groups:
     1. Small objects. These are allocated and freed often, and there may be many of them. Many objects are the same or similar size, so aggressive reuse of memory is useful for keeping required physical memory down.
     2. Large objects, such as buffers. These are often allocated more sporadically, live longer, and requests are not often similarly sized.

   Because the usage pattern of small and large objects is so different, it is useful to split ``kmalloc`` up into two sub-allocators - one tailored for small objects with lots of memory reuse and another for large objects with less reuse. We'll deal with this allocator first.

*/
/**
   "vmspace" - another buddy based allocator
   =========================================
   
   So for this large object allocator, we need to:
     1. Carve up part of the kernel virtual address space and hand it out to callers.
     2. Allocate physical memory for the bits of address space given to callers.

   The first requirement sounds kind of similar to what we needed for our physical memory manager. In fact, it's the same! So we can just reuse the "buddy" allocator we created for the PMM. { */

#ifndef VMSPACE_H
#define VMSPACE_H

#include "stdint.h"
#include "adt/buddy.h"

/** Instead of pinning the allocator to just one specific range, let's make a simple ADT for it so we can reuse it again if necessary. { */

typedef struct vmspace {
  uintptr_t start;
  uintptr_t size;
  buddy_t allocator;
  spinlock_t lock;
} vmspace_t;

int vmspace_init(vmspace_t *vms, uintptr_t addr, uintptr_t sz);
uintptr_t vmspace_alloc(vmspace_t *vms, unsigned sz, int alloc_phys);
void vmspace_free(vmspace_t *vms, unsigned sz, uintptr_t addr, int free_phys);

/* Allows accessing the singleton vmspace allocated for kernel heap use. */
extern vmspace_t kernel_vmspace;

#endif
