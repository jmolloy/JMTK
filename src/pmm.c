#include "hal.h"
#include "assert.h"
#include "stdio.h"
#include "mmap.h"
#include "adt/buddy.h"

#define dbg(args...) kprintf("pmm: " args)

#define MIN(x, y) ( (x < y) ? x : y )
#define MAX(x, y) ( (x > y) ? x : y )

static spinlock_t lock = SPINLOCK_RELEASED;
static buddy_t allocators[3];

static range_t split_range(range_t *r, uint64_t loc) {
  range_t ret;

  if (r->start >= loc) {
    ret.extent = 0;
  } else if (r->start + r->extent <= loc) {
    ret = *r;
    r->extent = 0;
  } else {
    ret.start = r->start;
    ret.extent = loc - r->start;

    r->start = loc;
    r->extent = r->extent - ret.extent;
  }

  return ret;
}

uint64_t alloc_page(int req) {
  return alloc_pages(req, 1);
}

uint64_t alloc_pages(int req, size_t num) {
  spinlock_acquire(&lock);

  uint64_t val = buddy_alloc(&allocators[req], num * get_page_size());

  if (val == ~0ULL && req == PAGE_REQ_NONE)
    val = buddy_alloc(&allocators[PAGE_REQ_UNDER4GB], num * get_page_size());

  spinlock_release(&lock);
  return val;
}

int free_page(uint64_t page) {
  return free_pages(page, 1);
}

int free_pages(uint64_t pages, size_t num) {
  spinlock_acquire(&lock);

  int req = PAGE_REQ_NONE;
  if (pages < 0x100000)
    req = PAGE_REQ_UNDER1MB;
  else if (pages < 0x100000000ULL)
    req = PAGE_REQ_UNDER4GB;
  
  buddy_free(&allocators[req], pages, num * get_page_size());

  spinlock_release(&lock);
  return 0;
}

int init_physical_memory(range_t *ranges, unsigned nranges, uint64_t max_extent) {
  range_t rs[3];
  rs[PAGE_REQ_UNDER1MB].start = 0x0;
  rs[PAGE_REQ_UNDER1MB].extent = MAX(MIN(max_extent, 0x100000), 0);

  rs[PAGE_REQ_UNDER4GB].start = 0x100000;
  rs[PAGE_REQ_UNDER4GB].extent = MAX(MIN(max_extent, 0x100000000ULL) -
                                     0x100000, 0);

  rs[PAGE_REQ_NONE].start = 0x100000000ULL;
  rs[PAGE_REQ_NONE].extent = (max_extent > 0x100000000ULL) ?
    max_extent - 0x100000000ULL : 0;

  size_t overheads[3];
  overheads[0] = buddy_calc_overhead(rs[0]);
  overheads[1] = buddy_calc_overhead(rs[1]);
  overheads[2] = buddy_calc_overhead(rs[2]);

  size_t bitmap_sz = overheads[0] + overheads[1] + overheads[2];
  
  /* Try and find a range large enough to hold the bitmaps */
  range_t *range = NULL;
  for (unsigned i = 0; i < nranges; ++i) {
    if (ranges[i].extent >= bitmap_sz) {
      range = &ranges[i];
      break;
    }
  }
  assert(range != NULL && "Unable to find a memory range large enough!");

  size_t bitmap_sz_pages = bitmap_sz >> get_page_shift();
  if (bitmap_sz & get_page_mask())
    ++bitmap_sz_pages;

  int ok = map(MMAP_PMM_BITMAP, range->start, bitmap_sz_pages, PAGE_WRITE);
  assert(ok == 0 && "map() failed in init_physical_memory!");

  range->start += bitmap_sz;
  range->extent -= bitmap_sz;

  ok  = buddy_init(&allocators[PAGE_REQ_UNDER1MB],
                   (uint8_t*) MMAP_PMM_BITMAP,
                   rs[PAGE_REQ_UNDER1MB], 0);
  ok |= buddy_init(&allocators[PAGE_REQ_UNDER4GB],
                   (uint8_t*) (MMAP_PMM_BITMAP + overheads[0]),
                   rs[PAGE_REQ_UNDER4GB], 0);
  ok |= buddy_init(&allocators[PAGE_REQ_NONE],
                   (uint8_t*) (MMAP_PMM_BITMAP + overheads[0] + overheads[1]),
                   rs[PAGE_REQ_NONE], 0);
  if (ok != 0) {
    dbg("buddy_init failed!\n");
    return 1;
  }

  for (unsigned i = 0; i < nranges; ++i) {
    if (ranges[i].extent == 0)
      continue;

    range_t r = split_range(&ranges[i], 0x100000);
    if (r.extent > 0)
      buddy_free_range(&allocators[PAGE_REQ_UNDER1MB], r);

    r = split_range(&ranges[i], 0x100000000ULL);
    if (r.extent > 0)
      buddy_free_range(&allocators[PAGE_REQ_UNDER4GB], r);

    if (range->extent > 0)
      buddy_free_range(&allocators[PAGE_REQ_NONE], ranges[i]);
  }

  return 0;
}
