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

     **Allocation**: *lg(n)*
     **Free**: *lg(n)*

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

/**
   Users of the buddy allocator will need to provide storage for the bitmaps
   it uses.

   We provide a function to inform the user how large this storage needs to be. This depends on the range of addresses being covered, and equates to the sum how many bits are needed to cover the address range when subdivided into different sizes.

   We maintain bitmaps for sizes ranging from 2:sup:`MIN_BUDDY_SZ_LOG2` to 2:sup:`MAX_BUDDY_SZ_LOG2`. { */

size_t buddy_calc_overhead(range_t r) {
  size_t accum = 0;
  for (unsigned i = MIN_BUDDY_SZ_LOG2; i <= MAX_BUDDY_SZ_LOG2; ++i)
    /* Add one here to be conservative in case the division by 8 had
       a remainder. */
    accum += (r.extent >> i) / 8 + 1;
  return accum;
}

/**
   Next we can initialise a buddy allocator. We use the ``overhead_storage``
   argument to store our bitmaps in, and allow the user to choose if the
   memory should start as being free or allocated.

   Note that I have defined a simple bitmap ADT, but am not going to show the
   implementation - that is left as an exercise to the reader! { */

int buddy_init(buddy_t *bd, uint8_t *overhead_storage,
               range_t r, int start_freed) {
  bd->start = r.start;
  bd->size  = r.extent;

  for (unsigned i = 0; i < NUM_BUDDY_BUCKETS; ++i) {
    unsigned nbits = bd->size >> (MIN_BUDDY_SZ_LOG2 + i);
    bitmap_init(&bd->orders[i], overhead_storage, nbits);
    overhead_storage += nbits / 8 + 1;
  }

  if (start_freed != 0)
    buddy_free_range(bd, r);

  return 0;
}

/**
   Now we get to the meat and bones of the buddy allocator - allocation! { */

uint64_t buddy_alloc(buddy_t *bd, unsigned sz) {

  /** Firstly we find the smallest power of 2 that will hold the allocation request, and take the log base 2 of it. { */
  unsigned log_sz = log2_roundup(sz);
  if (log_sz > MAX_BUDDY_SZ_LOG2)
    panic("buddy_alloc had request that was too large to handle!");

  unsigned orig_log_sz = log_sz;

  /** Then we try and find a free block of this size. This involves searching in the right bitmap for an set bit. If there are no set bits, we increase the size of the block we're searching for. { */

  /* Search for a free block - we may have to increase the size of the
     block to find a free one. */
  int64_t idx;
  while (log_sz <= MAX_BUDDY_SZ_LOG2) {
    idx = bitmap_first_set(&bd->orders[log_sz - MIN_BUDDY_SZ_LOG2]);
    if (idx != -1)
      /* Block found! */
      break;
    ++log_sz;
  }

  if (idx == -1)
    /* No free blocks :( */
    return ~0ULL;

  /** Now, if we couldn't get a block of the size we wanted, we'll have to
      split it down to the right size. { */

  /* We may have to split blocks to get back to a block of
     the minimum size. */
  for (; log_sz != orig_log_sz; --log_sz) {
    int order_idx = log_sz - MIN_BUDDY_SZ_LOG2;

    /* We're splitting a block, so deallocate it first... */
    bitmap_clear(&bd->orders[order_idx], idx);

    /* Then set both its children as free in the next order. */
    idx = INC_ORDER(idx);
    bitmap_set(&bd->orders[order_idx-1], idx);
    bitmap_set(&bd->orders[order_idx-1], idx+1);
  }

  /** By this point we have a block that is free. We should now mark it as allocated then calculate the address that actually equates to. { */

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

/** Freeing is actually easier, as we never have to worry about
    splitting blocks.

    Note that this function can only free addresses and sizes that are
    correctly aligned, so it's only really safe to call this with addresses
    returned from buddy_alloc(). 

    We simply mark the incoming block as free, then while we are not
    at the top level of the tree, see if the buddy is also free. If so,
    we mark them both as unavailable and move up the tree one level. { */
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

    /* FIXME: Ensure max(this, buddy) wouldn't go over the max extent
       of the region. */

    /* Mark them both non free. */
    bitmap_clear(&bd->orders[order_idx], idx);
    bitmap_clear(&bd->orders[order_idx], BUDDY(idx));

    /* Move up an order. */
    idx = DEC_ORDER(idx);
    ++log_sz;
  }

}

/** Finally we have a helper function which will take a range of memory and 
    mark it all as free.

    To do this, we need to chunk it up into correctly aligned blocks of 
    maximal size. { */

void buddy_free_range(buddy_t *bd, range_t range) {
  uintptr_t min_sz = 1 << MIN_BUDDY_SZ_LOG2;

  /** Firstly, we use a helper function to check if the range's start address
      is aligned to a multiple of the smallest block size. If not, we adjust
      it so that it is. { */

  /* Ensure the range start address is at least aligned to
     MIN_BUDDY_SZ_LOG2. */
  if (aligned_for(range.start, MIN_BUDDY_SZ_LOG2) == 0) {
    if (range.extent < min_sz)
      return;

    uint64_t old_start = range.start;
    range.start &= ~0ULL << MIN_BUDDY_SZ_LOG2;
    range.start += min_sz;
    range.extent -= range.start - old_start;
  }

  /** Now, we iteratively work through the range trying to allocate the largest
      block we can. { */

  while (range.extent >= min_sz &&
         aligned_for(range.start, MIN_BUDDY_SZ_LOG2)) {
    
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
