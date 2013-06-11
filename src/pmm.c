/**#1
   Physical memory management
   ==========================

   In a kernel, you have multiple memory managers. You have a MM for managing use of the physical address space, a manager for use of the virtual address space, and a manager for controlling the mappings between virtual and physical.

   This chapter is going to focus on the first - physical memory management.
   
   Physical Memory management basics
   ---------------------------------

   Physical memory managers usually work on a courser granularity than one byte. The usual granularity is the page size, which is a minimum of 4096 bytes (2:sup:`12`).

   So the input to the PMM is a set of pages that are free. Its API is simple - a user can request a free page, and can return a page as no longer used.

   Actually it's slightly more complex than that; there are certain components that require only physical memory in certain locations. An example is 16-bit i386 emulation (perhaps for a VBE driver) - this requires memory below the 1MB boundary. Many DMA engines only support memory below 4GB. So we have three categories of physical memory - below 1MB, 1MB..4GB and > 4GB.

   Common algorithms
   -----------------

   There are two common routes for implementing a PMM. I'm going to discuss both of them, then explain the reason we're not going to use either :)

   Note during these that the ideal is *O(1)* for allocate and free, and *O(n)* for space requirement.

   Bitmaps
   ~~~~~~~

   The idea is that you create a bitmap (array of 1-bit values) for the entirety of your usable physical space. One value represents that the page is free (available for use), the other represents that it is unavailable.

   **Time complexity**
     A bitmap search is worst-case linear with respect to size - *O(n)*. Taking the naive approach of searching from the beginning of the bitmap each time and allocating the first encountered free page, it will take the worst-case time as the average time.

     **Allocate**: *O(n)*.
     **Free**: *O(1)*

   **Space complexity**
     A bitmap must be created that spans from the lowest possible address to the highest possible address. This means that there will potentially be lots of dead space, as memory maps rarely bunch up all available RAM together in the low end of the address space. There are often large holes for which we're allocating bitmap space that is unused. In the worst case that we have available RAM at 0xFFFFF000, we'd need to allocate a 4MB bitmap. Not only is this large, it is uncorrelated to the amount of physical RAM in the system and can hurt cache performance. **O(n)**

   Stack
   ~~~~~

   The idea here is to keep a push-down stack of all known free pages. On allocate you pops a page off the stack, on free you push the page onto the stack.

   **Time complexity**
     A stack has constant time complexity in all operations.

     **Allocate**: *O(1)*
     **Free**: *O(1)*

   **Space complexity**
     The stack only takes up as much space as is required to store its contents, so altough it is **O(n)** the same as the bitmap, the *n* is smaller as it is related to the amount of available memory rather than the position of that memory in the address space.

   **Drawbacks**
     The stack based approach is clearly ideal in allocation, free and space complexity, so what are some negatives?

     A stack suffers from fragmentation. The order in which pages are returned from the allocator is the inverse order in which they were freed. Generally this doesn't matter, but there are a couple of situations where it does:

        * Old DMA (direct memory access) devices may require a contiguous region of physical memory to communicate with the processor. Newer DMA controllers have "scatter-gather" semantics that allow them to take a noncontiguous list of pages to write to, but old ones may not.
        * Coalescing into larger pages. We've mentioned that the basic page size is 4KB, but many CPUs support larger page sizes which, if used, are more efficient (fewer TLB entries). If it is impossible to get contiguous memory spanning multiple pages from the physical allocator, it is impossible to use a larger page size.

     It may end up using more space - to store the fact that page *n* is free, it requires 32 bits whereas the bitmap requires just one bit.

 */

#include "hal.h"
#include "assert.h"
#include "stdio.h"
#include "mmap.h"
#include "adt/buddy.h"
#include "string.h"

#ifdef DEBUG_pmm
# define dbg(args...) kprintf("pmm: " args)
#else
# define dbg(args...)
#endif

#define MIN(x, y) ( (x < y) ? x : y )
#define MAX(x, y) ( (x > y) ? x : y )

/**#3
*/

extern range_t  early_ranges[64];
extern unsigned early_nranges;
extern uint64_t early_max_extent;

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
  dbg("alloc_pages: get lock\n");
  spinlock_acquire(&lock);
  dbg("alloc_pages: got lock\n");
  uint64_t val = buddy_alloc(&allocators[req], num * get_page_size());
  dbg("alloc_pages2: returning %x\n", val & 0xFFFFFFFF);
  if (val == ~0ULL && req == PAGE_REQ_NONE)
    val = buddy_alloc(&allocators[PAGE_REQ_UNDER4GB], num * get_page_size());
  dbg("alloc_pages: returning %x\n", val & 0xFFFFFFFF);
  
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

int init_physical_memory() {
  assert(pmm_init_stage == PMM_INIT_EARLY &&
         "init_physical_memory_early must be called first!");

  range_t rs[3];
  rs[PAGE_REQ_UNDER1MB].start = 0x0;
  rs[PAGE_REQ_UNDER1MB].extent = MAX(MIN(early_max_extent, 0x100000), 0);

  rs[PAGE_REQ_UNDER4GB].start = 0x100000;
  rs[PAGE_REQ_UNDER4GB].extent = MAX(MIN(early_max_extent, 0x100000000ULL) -
                                     0x100000, 0);

  rs[PAGE_REQ_NONE].start = 0x100000000ULL;
  rs[PAGE_REQ_NONE].extent = (early_max_extent > 0x100000000ULL) ?
    early_max_extent - 0x100000000ULL : 0;

  size_t overheads[3];
  overheads[0] = buddy_calc_overhead(rs[PAGE_REQ_UNDER1MB]);
  overheads[1] = buddy_calc_overhead(rs[PAGE_REQ_UNDER4GB]);
  overheads[2] = buddy_calc_overhead(rs[PAGE_REQ_NONE]);

  size_t bitmap_sz = overheads[0] + overheads[1] + overheads[2];
  size_t bitmap_sz_pages =
    round_to_page_size(bitmap_sz) >> get_page_shift();

  for (unsigned i = 0; i < bitmap_sz_pages; ++i)
    assert(map(MMAP_PMM_BITMAP + i * get_page_size(),
               early_alloc_page(), 1, PAGE_WRITE) == 0);

  int ok;
  ok  = buddy_init(&allocators[PAGE_REQ_UNDER1MB],
                   (uint8_t*) MMAP_PMM_BITMAP,
                   rs[PAGE_REQ_UNDER1MB], 0);
  ok |= buddy_init(&allocators[PAGE_REQ_UNDER4GB],
                   (uint8_t*) (MMAP_PMM_BITMAP + overheads[0]),
                   rs[PAGE_REQ_UNDER4GB], 0);
  ok |= buddy_init(&allocators[PAGE_REQ_NONE],
                   (uint8_t*) (MMAP_PMM_BITMAP + overheads[0] +
                               overheads[1]),
                   rs[PAGE_REQ_NONE], 0);
  if (ok != 0) {
    dbg("buddy_init failed!\n");
    return 1;
  }

  for (unsigned i = 0; i < early_nranges; ++i) {
    if (early_ranges[i].extent == 0)
      continue;

    range_t r = split_range(&early_ranges[i], 0x100000);
    if (r.extent > 0)
      buddy_free_range(&allocators[PAGE_REQ_UNDER1MB], r);

    r = split_range(&early_ranges[i], 0x100000000ULL);
    if (r.extent > 0)
      buddy_free_range(&allocators[PAGE_REQ_UNDER4GB], r);

    if (early_ranges[i].extent > 0)
      buddy_free_range(&allocators[PAGE_REQ_NONE], early_ranges[i]);
  }

  pmm_init_stage = PMM_INIT_FULL;

  return 0;
}
