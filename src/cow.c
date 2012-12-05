#include "assert.h"
#include "hal.h"
#include "mmap.h"
#include "string.h"
#include "stdio.h"
static uint32_t *cow_refcnt_array = (uint32_t*) MMAP_COW_REFCNTS;

static void init_page(uint64_t p) {
  uintptr_t backing_page =
    (uintptr_t)(&cow_refcnt_array[p >> get_page_shift()]) & ~get_page_mask();

  if (!is_mapped(backing_page)) {
    uint64_t page = alloc_page(PAGE_REQ_NONE);
    assert(page != ~0ULL && "alloc_page failed!");
    int ret = map(backing_page, page, 1, PAGE_WRITE);
    assert(ret != -1 && "map failed!");

    memset((void*)backing_page, 0, get_page_size());
  }
}

int init_cow_refcnts(range_t *ranges, unsigned nranges) {
  for (unsigned i = 0; i < nranges; ++i) {
    for (uint64_t j = 0; j < ranges[i].extent; j += get_page_size()) {
      init_page(ranges[i].start + j);
    }
  }
  return 0;
}

void cow_refcnt_inc(uint64_t p) {
  ++cow_refcnt_array[p >> get_page_shift()];
}

void cow_refcnt_dec(uint64_t p) {
  --cow_refcnt_array[p >> get_page_shift()];
}

unsigned cow_refcnt(uint64_t p) {
  return cow_refcnt_array[p >> get_page_shift()];
}
