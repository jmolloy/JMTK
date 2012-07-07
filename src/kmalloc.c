#include "assert.h"
#include "hal.h"
#include "kmalloc.h"
#include "math.h"
#include "mmap.h"
#include "slab.h"
#include "vmspace.h"

#define MAX_CACHESZ_LOG2 9 /* 2**9 = 512 */
#define MIN_CACHESZ_LOG2 3 /* 2**3 = 8 */

#define KMALLOC_CANARY 0xDEAD12

vmspace_t kernel_vmspace;

static slab_cache_t caches[MAX_CACHESZ_LOG2-MIN_CACHESZ_LOG2+1];

void *kmalloc(unsigned sz) {
  /* We need to add a small header to the allocation to track which
     cache (if any) it came from. It must be a multiple of the pointer
     size in order that the address after it (which we will be returning)
     has natural alignment. */
  sz += sizeof(uintptr_t);

  uintptr_t *ptr;

  unsigned l2 = log2_roundup(sz);
  if (l2 < MIN_CACHESZ_LOG2) l2 = MIN_CACHESZ_LOG2;

  if (l2 >= MIN_CACHESZ_LOG2 && l2 <= MAX_CACHESZ_LOG2) {
    ptr = (uintptr_t*)slab_cache_alloc(&caches[l2-MIN_CACHESZ_LOG2]);
  } else {
    /* Get the size as the smallest power of 2 >= sz */
    unsigned sz_p2 = 1U << l2;
    if (sz_p2 < get_page_size()) {
      sz_p2 = get_page_size();
      l2 = log2_roundup(sz_p2);
    }

    ptr = (uintptr_t*)vmspace_alloc(&kernel_vmspace, sz_p2, 1);
  }

  ptr[0] = (KMALLOC_CANARY << 8) | l2;
  return &ptr[1];
}

void kfree(void *p) {
  p = (void*) ((uintptr_t)p - sizeof(uintptr_t));
  uintptr_t *ptr = (uintptr_t*)p;

  unsigned l2 = ptr[0] & 0xFF;
  unsigned canary = ptr[0] >> 8;

  assert(canary == KMALLOC_CANARY && "Heap corruption!");

  if (l2 <= MAX_CACHESZ_LOG2)
    slab_cache_free(&caches[l2-MIN_CACHESZ_LOG2], p);
  else
    vmspace_free(&kernel_vmspace, (1U << l2), (uintptr_t)p, 1);
}

static int kmalloc_init() {
  /* FIXME: Make vmspace_init deal with addresses that aren't initially
     maximally aligned so we can give it 0xC0400000 as the starting
     address (first address after first 4MB identity map (x86)) */
  if (vmspace_init(&kernel_vmspace,
                   MMAP_KERNEL_VMSPACE_START,
                   MMAP_KERNEL_VMSPACE_END-MMAP_KERNEL_VMSPACE_START) == -1) {
    assert(0 && "kernel_vmspace init failed!");
    return -1;
  }

  int r = 0;
  for (unsigned i = 0; i <= MAX_CACHESZ_LOG2-MIN_CACHESZ_LOG2; ++i) {
    r |= slab_cache_create(&caches[i], &kernel_vmspace, 1U<<(i+MIN_CACHESZ_LOG2), NULL);
  }
  assert(r == 0  && "slab cache creation failed!");

  return r;
}

static prereq_t prereqs[] = { {"x86/free_memory",NULL}, {"hosted/free_memory",NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "kmalloc",
  .required = NULL,
  .load_after = prereqs,
  .init = &kmalloc_init,
  .fini = NULL
};
