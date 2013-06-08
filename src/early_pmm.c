#include "hal.h"
#include "assert.h"
#include "stdio.h"
#include "string.h"

/**
   Early physical memory allocation
   ================================

   Physical and virtual memory managers are symbiotic. Virtual memory managers
   need physical pages to use as page tables. Physical memory managers don't
   know how much memory they'll need at compile time (it depends on how much
   memory is available in the system!) so need to map memory at runtime.

   This chapter will be very short. We'll be implementing a trivial physical
   memory manager that doesn't have the ability to free pages (so can be
   *very* simple). The idea being that we'll use this "early" physical
   memory allocator until the virtual memory manager is up and running,
   at which point we'll switch over to a full-fledged PMM.

*/

#ifdef DEBUG_early_pmm
# define dbg(args...) kprintf("early_pmm: " args)
#else
# define dbg(args...)
#endif

/**
   The idea is that the system gives us a set of "ranges" of free memory,
   and we'll try and modify them in place, being as simple as possible. { */

range_t  early_ranges[64];
unsigned early_nranges;
/* The largest valid memory address. */
uint64_t early_max_extent;

/** In initialising the early PMM, we'll just copy the ranges the user gave us.
    Our aim is to be simple, not flexible. So if we can't handle something,
    we'll just panic. { */
int init_physical_memory_early(range_t *ranges, unsigned nranges,
                               uint64_t max_extent) {
  assert(pmm_init_stage == PMM_INIT_START &&
         "init_physical_memory_early() called twice!");
  assert(nranges < 64 && "Too many ranges!");
  memcpy(early_ranges, ranges, nranges * sizeof(range_t));
  early_nranges = nranges;
  early_max_extent = max_extent;

  pmm_init_stage = PMM_INIT_EARLY;
  return 0;
}

/** In allocating a page, we do have to be a little cleverer. Pages under
    1MB in address are useful for DMA engines and shouldn't be used by us
    unless we really have to. Similarly, addresses which require more than
    32 bits to represent may not be usable early in our boot process, so we'll
    try to return a page between 1MB and 4GB in address. { */
uint64_t early_alloc_page() {
  assert(pmm_init_stage == PMM_INIT_EARLY);
  for (unsigned i = 0; i < early_nranges; ++i) {
    if (early_ranges[i].extent <= 0x1000 || early_ranges[i].start >= 0x100000000ULL)
      /* Discard any pages over 4GB physical or less than 4K in size. */
      continue;
    /* Ignore any range under 1MB */
    if (early_ranges[i].start < 0x100000)
      continue;
    
    uint32_t ret = (uint32_t)early_ranges[i].start;
    early_ranges[i].start += 0x1000;
    early_ranges[i].extent -= 0x1000;

    dbg("early_alloc_page() -> %x\n", ret);
    return ret;
  }
  panic("early_alloc_page couldn't find any pages to use!");
}
