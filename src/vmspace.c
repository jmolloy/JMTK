#include "vmspace.h"
#include "hal.h"

#define BUDDY(x, log_sz) (x ^ (1U<<log_sz))

static unsigned clz(uint32_t n) {
  /* I really hope this naive algorithm is converted to bsr/clz... */
  unsigned r = 0;
  while ((n & (1U<<31)) == 0) {
    ++r;
    n <<= 1;
  }
  return r;
}

static unsigned log2_roundup(uint32_t n) {
  /* Calculate the floor of log2(n) */
  unsigned l2 = 32 - clz(n);
  
  /* If n == 2^log2(n), floor(n) == n so we can return l2. */
  if (n == 1U<<l2)
    return l2;
  /* else floor(n) != n, so return l2+1 to round up. */
  return l2+1;
}

static int vmspace_init(vmspace_t *vms, uintptr_t addr, uintptr_t sz) {
  vms->start = addr;
  vms->size = sz;

  for (unsigned i = 0; i < MAX_BUDDY_SZ_LOG2-MIN_BUDDY_SZ_LOG2; ++i)
    xbitmap_init(vms->orders[i], 0, NULL, NULL, NULL);

  unsigned i = MAX_BUDDY_SZ_LOG2;
  uintptr_t idx = 0;
  while (sz > 0 && i >= MIN_BUDDY_SZ_LOG2) {
    unsigned _sz = 1U << i;
    if (sz >= _sz) {
      xbitmap_set(vms->orders[i], idx++);
      sz -= _sz;
    } else {
      --i;
      idx <<= 1;
    }
  }
  if (sz != 0 && i == MIN_BUDDY_SZ_LOG2)
    panic("vmspace_init: algorithmic error!");

  return 0;
}

uintptr_t vmspace_alloc(vmspace_t *vms, unsigned sz, int alloc_phys) {
  unsigned log_sz = log2_roundup(sz);
  if (log_sz > MAX_BUDDY_SZ_LOG2)
    panic("vmspace_alloc had request that was too large to handle!");

  unsigned orig_log_sz = log_sz;

  /* Search for a free block - we may have to increase the size of the
     block to find a free one. */
  unsigned idx;
  while (log_sz <= MAX_BUDDY_SZ_LOG2) {
    int order_idx = log_sz - MIN_BUDDY_SZ_LOG2;
    
    idx = xbitmap_first_set(vms->orders[order_idx]);
    if (idx != ~0U)
      break;
    ++log_sz;
  }

  /* We may have to split blocks to get back to a block of the minimum size. */
  for (; log_sz != orig_log_sz; --log_sz) {
    int order_idx = log_sz - MIN_BUDDY_SZ_LOG2;

    /* We're splitting a block, so deallocate it first... */
    xbitmap_clear(vms->orders[order_idx], idx);

    /* Then set both its children as free in the next order. */
    idx <<= 1;
    xbitmap_set(vms->orders[order_idx-1], idx);
    xbitmap_set(vms->orders[order_idx-1], idx+1);
  }

  /* Mark the block as not free. */
  int order_idx = log_sz - MIN_BUDDY_SZ_LOG2;
  xbitmap_clear(vms->orders[order_idx], idx);

  uintptr_t addr = vms->start + (idx << log_sz);

  if (alloc_phys) {
    unsigned pgsz = get_page_size();
    for (unsigned i = 0; i < sz; i += pgsz) {
      uint64_t p = alloc_page(PAGE_REQ_NONE);
      if (p == ~0ULL)
        panic("vmspace_alloc: alloc_page failed!");
      if (map(addr + i, p, 1, alloc_phys) == -1)
        panic("vmspace_alloc: map failed!");
    }
  }

  return addr;
}

void vmspace_free(vmspace_t *vms, unsigned sz, uintptr_t addr, int free_phys) {

  if (free_phys) {
    unsigned pgsz = get_page_size();
    for (unsigned i = 0; i < sz; i += pgsz) {
      uint64_t p = get_mapping(addr + i, NULL);
      if (p == ~0ULL)
        panic("vmspace_free asked to free_phys but mapping did not exist!");
      free_page(p);
      unmap(addr + i, 1);
    }
  }

  uintptr_t offs = addr - vms->start;
  unsigned log_sz = log2_roundup(sz);
  unsigned idx = offs >> log_sz;

  while (log_sz >= MIN_BUDDY_SZ_LOG2) {
    int order_idx = log_sz - MIN_BUDDY_SZ_LOG2;

    /* Mark this node free. */
    xbitmap_set(vms->orders[order_idx], idx);
    /* Is this node's buddy also free? */
    if (xbitmap_isset(vms->orders[order_idx], BUDDY(idx, log_sz)) == 0)
      /* no :( */
      break;

    /* Mark them both non free. */
    xbitmap_clear(vms->orders[order_idx], idx);
    xbitmap_clear(vms->orders[order_idx], BUDDY(idx, log_sz));

    /* Move up an order. */
    idx >>= 1;
    ++log_sz;
  }

}
