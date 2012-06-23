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
    kprintf("split_range: 1\n");
    ret.extent = 0;
  } else if (r->start + r->extent <= loc) {
    kprintf("split_range: 2\n");
    ret = *r;
    r->extent = 0;
  } else {
    kprintf("split_range: 3\n");
    ret.start = r->start;
    ret.extent = loc - r->start;

    r->start = loc;
    r->extent = r->extent - ret.extent;
  }

  return ret;
}

static void *pmm_alloc_internal(unsigned sz, void *p) {
  static uintptr_t virt = MMAP_PMM_BITMAP;
  //  kprintf("pmm_alloc_internal.\n");
  uintptr_t *ptr = (uintptr_t*)p;
  uintptr_t phys = *ptr;
  uintptr_t ret = virt;

  map(virt, phys, 1, PAGE_WRITE);

  virt += get_page_size();
  *ptr += get_page_size();

  return (void*)ret;
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
  /* Calculate the required bitmap size for the buddy allocator -
       2*max / pagesize / 8. */
  uint64_t bitmap_sz = (max_extent >> (12+3)) << 1;
  dbg("bitmap_sz: %x\n", (uint32_t)bitmap_sz);

  /* Try and find a range large enough to hold the bitmaps */
  range_t *range = NULL;
  for (unsigned i = 0; i < nranges; ++i) {
    if (ranges[i].extent >= bitmap_sz) {
      range = &ranges[i];
      break;
    }
  }

  assert(range != NULL && "Unable to find a memory range large enough!");

  static uintptr_t alloc_ptr;
  alloc_ptr = range->start;
  
  range->start += bitmap_sz;
  range->extent -= bitmap_sz;

  range_t rs[3];
  rs[PAGE_REQ_UNDER1MB].start = 0x0;
  rs[PAGE_REQ_UNDER1MB].extent = MAX(MIN(max_extent, 0x100000), 0);
  rs[PAGE_REQ_UNDER4GB].start = 0x100000;
  rs[PAGE_REQ_UNDER4GB].extent = MAX(MIN(max_extent, 0x100000000ULL) -
                                     0x100000, 0);
  rs[PAGE_REQ_NONE].start = 0x100000000ULL;
  rs[PAGE_REQ_NONE].extent = (max_extent > 0x100000000ULL) ? max_extent - 0x100000000ULL : 0;

  int ok = buddy_init(&allocators[PAGE_REQ_UNDER1MB], &pmm_alloc_internal, NULL,
                      &alloc_ptr, rs[PAGE_REQ_UNDER1MB], 0);
  ok |= buddy_init(&allocators[PAGE_REQ_UNDER4GB], &pmm_alloc_internal, NULL,
                   &alloc_ptr, rs[PAGE_REQ_UNDER4GB], 0);
  ok |= buddy_init(&allocators[PAGE_REQ_NONE], &pmm_alloc_internal, NULL,
                   &alloc_ptr, rs[PAGE_REQ_NONE], 0);
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
