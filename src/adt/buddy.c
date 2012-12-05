/**#2
   Buddy allocation
   ~~~~~~~~~~~~~~~~

   This is a more versatile allocator that can be used for multiple purposes, not just physical allocation (which has a subset of the requirements of a general-purpose allocator).

   The idea is that the address space is represented by a binary tree. Each node in the tree can be split into two "buddies" - buddies are siblings in the tree.

   The root tree node covers all of the available space. Its children cover half each, and their children half of that and so on.

   The tree is kept as compacted as possible at all times, so initially there is just the one root node. On an allocation request, the tree is divided enough so that the minimum node size that will cover the allocation request is reached.

   On a free request, the node indicated is marked free, then iteratively up the tree back to the root its buddy is checked if it too is free. If so, both buddies are destroyed and their parent marked as free instead, collapsing the tree back up again.

   **Time complexity**
     Allocation in the buddy world requires potentially *log:sub:`2`(n)* tree node traversals, the same with freeing. The leading constant however is very fast.

     **Allocation**: *log(n)*
     **Free**: *log(n)*

   **Space complexity**
     The buddy allocator requires several bitmaps to operate - these are dependent upon the address space size it operates over, so it is similar in requirement to the bitmap allocator mentioned previously - *O(n)*.

   Implementation
   --------------

   Our physical memory manager will be based upon a buddy allocator. Although the stack allocator has better theoretical allocate and free performance (and memory usage), it only allows allocation/freeing of one page at a time, and does not have any way to request multiple adjacent pages, which we may want to do for a DMA buffer or to use larger-sized pages (such as 1MB or 2MB pages).

*/

#include "assert.h"
#include "hal.h"
#include "math.h"
#include "adt/buddy.h"

/**
   Firstly, let's define some helper macros that manipulate tree node indices.

   To find the buddy of a node 'x', it is the adjacent node. So that's simply ``x xor 1``.

   Given a node in one depth level (order) of the tree, you can find its first child by multiplying it by two, and its parent by dividing by two. { */

#define BUDDY(x)     (x ^ 1)
#define INC_ORDER(x) (x << 1)
#define DEC_ORDER(x) (x >> 1)



size_t buddy_calc_overhead(range_t r) {
  size_t accum = 0;
  for (unsigned i = MIN_BUDDY_SZ_LOG2; i <= MAX_BUDDY_SZ_LOG2; ++i)
    accum += (r.extent >> i) / 8 + 1;
  return accum;
}

int buddy_init(buddy_t *bd, uint8_t *overhead_storage,
               range_t r, int start_freed) {
  bd->start = r.start;
  bd->size  = r.extent;

  for (unsigned i = 0; i < NUM_BUDDY_BUCKETS; ++i) {
    unsigned idx = bd->size >> (MIN_BUDDY_SZ_LOG2 + i);
    bitmap_init(&bd->orders[i], overhead_storage, idx);
    overhead_storage += idx / 8 + 1;
  }

  if (start_freed != 0)
    buddy_free_range(bd, r);

  return 0;
}

uint64_t buddy_alloc(buddy_t *bd, unsigned sz) {

  unsigned log_sz = log2_roundup(sz);
  if (log_sz > MAX_BUDDY_SZ_LOG2)
    panic("buddy_alloc had request that was too large to handle!");

  unsigned orig_log_sz = log_sz;

  /* Search for a free block - we may have to increase the size of the
     block to find a free one. */
  int64_t idx;
  while (log_sz <= MAX_BUDDY_SZ_LOG2) {
    int order_idx = log_sz - MIN_BUDDY_SZ_LOG2;
    
    idx = bitmap_first_set(&bd->orders[order_idx]);

    if (idx != -1)
      break;

    ++log_sz;
  }

  if (idx == -1)
    /* No free blocks :( */
    return ~0ULL;

  /* We may have to split blocks to get back to a block of the minimum size. */
  for (; log_sz != orig_log_sz; --log_sz) {
    int order_idx = log_sz - MIN_BUDDY_SZ_LOG2;

    /* We're splitting a block, so deallocate it first... */
    bitmap_clear(&bd->orders[order_idx], idx);

    /* Then set both its children as free in the next order. */
    idx = INC_ORDER(idx);
    bitmap_set(&bd->orders[order_idx-1], idx);
    bitmap_set(&bd->orders[order_idx-1], idx+1);
  }

  /* Mark the block as not free. */
  int order_idx = log_sz - MIN_BUDDY_SZ_LOG2;
  bitmap_clear(&bd->orders[order_idx], idx);

  uint64_t addr = bd->start + ((uint64_t)idx << log_sz);
  return addr;  
}

static int aligned_for(uint64_t addr, uintptr_t lg2) {
  uintptr_t mask = ~( ~0ULL << lg2 );
  return (addr & mask) == 0;
}

void buddy_free_range(buddy_t *bd, range_t range) {
  uintptr_t min_sz = 1 << MIN_BUDDY_SZ_LOG2;

  /* Ensure the range start address is at least aligned to MIN_BUDDY_SZ_LOG2. */
  if (aligned_for(range.start, MIN_BUDDY_SZ_LOG2) == 0) {
    if (range.extent < min_sz)
      return;

    range.start &= ~0ULL << MIN_BUDDY_SZ_LOG2;
    range.start += min_sz;
  }

  while (range.extent >= min_sz && aligned_for(range.start, MIN_BUDDY_SZ_LOG2)) {
    
    for (unsigned i = MAX_BUDDY_SZ_LOG2; i >= MIN_BUDDY_SZ_LOG2; --i) {
      uintptr_t sz = 1 << i;
      uint64_t start = range.start - bd->start;

      if (sz > range.extent || aligned_for(start, i) == 0)
        continue;
      
      range.extent -= sz;
      range.start += sz;
      buddy_free(bd, start + bd->start, sz);
      break;
    }

  }
}

void buddy_free(buddy_t *bd, uint64_t addr, unsigned sz) {
  uint64_t offs = addr - bd->start;
  unsigned log_sz = log2_roundup(sz);
  unsigned idx = offs >> log_sz;

  while (log_sz >= MIN_BUDDY_SZ_LOG2) {
    int order_idx = log_sz - MIN_BUDDY_SZ_LOG2;

    /* Mark this node free. */
    bitmap_set(&bd->orders[order_idx], idx);

    /* Can we coalesce up another level? */
    if (log_sz == MAX_BUDDY_SZ_LOG2)
      break;

    /* Is this node's buddy also free? */
    if (bitmap_isset(&bd->orders[order_idx], BUDDY(idx)) == 0)
      /* no :( */
      break;

    /* Ensure max(this, buddy) wouldn't go over the max extent
       of the region. */
    /* FIXME: ^^ */

    /* Mark them both non free. */
    bitmap_clear(&bd->orders[order_idx], idx);
    bitmap_clear(&bd->orders[order_idx], BUDDY(idx));

    /* Move up an order. */
    idx = DEC_ORDER(idx);
    ++log_sz;
  }

}
