#include "assert.h"
#include "hal.h"
#include "vmspace.h"

int vmspace_init(vmspace_t *vms, uintptr_t addr, uintptr_t sz) {
  /* FIXME: Assert starts and finishes on a page boundary! */
  range_t r;
  r.start = addr;
  r.extent = sz;

  vms->start = addr;
  vms->size = sz;
  spinlock_init(&vms->lock);

  size_t overhead = round_to_page_size(buddy_calc_overhead(r));
  size_t npages = overhead >> get_page_shift();
  uintptr_t start = r.start + r.extent - overhead;

  int ok = map(start,
               alloc_pages(PAGE_REQ_NONE, npages), npages,
               PAGE_WRITE);
  assert(ok == 0 && "map() failed in vmspace_init!");

  r.extent -= overhead;

  buddy_init(&vms->allocator, (uint8_t*)start, r, /*start_freed=*/0);

  buddy_free_range(&vms->allocator, r);

  return 0;
}

uintptr_t vmspace_alloc(vmspace_t *vms, unsigned sz, int alloc_phys) {
  /* FIXME: Assert sz is page aligned. */
  spinlock_acquire(&vms->lock);

  uint64_t addr = buddy_alloc(&vms->allocator, sz);

  if (alloc_phys && addr != ~0ULL) {
    size_t npages = sz >> get_page_shift();
    int ok = map(addr, alloc_pages(PAGE_REQ_NONE, npages), npages, alloc_phys);
    assert(ok == 0 && "vmspace_alloc: map failed!");
  }

  spinlock_release(&vms->lock);
  return addr;
}

void vmspace_free(vmspace_t *vms, unsigned sz, uintptr_t addr, int free_phys) {
  spinlock_acquire(&vms->lock);

  if (free_phys) {
    unsigned pgsz = get_page_size();
    for (unsigned i = 0; i < sz; i += pgsz) {
      uint64_t p = get_mapping(addr + i, NULL);
      assert(p != ~0ULL &&
             "vmspace_free asked to free_phys but mapping did not exist!");
      free_page(p);
      unmap(addr + i, 1);
    }
  }

  buddy_free(&vms->allocator, addr, sz);

  spinlock_release(&vms->lock);
}
