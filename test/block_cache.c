#if 0
exit `$1 $2 | ./test/FileCheck $0`
#endif

#include "hal.h"
#include "assert.h"
#include "string.h"
#include "vmspace.h"
#include "stdio.h"

#include "block_cache.h"

int dread(block_device_t *obj, uint64_t offset, void *buf, uint64_t len) {
  uint32_t *ibuf = (uint32_t*) buf;
  for (unsigned i = 0; i < len/4; ++i) {
    ibuf[i] = offset + i*4;
  }
  return len;
}

int dwrite(block_device_t *obj, uint64_t offset, void *buf, uint64_t len) {
  return -1;
}

void dflush(block_device_t *obj) {
}

uint64_t dlength(block_device_t *obj) {
  return 0x100000;
}

void ddescribe(block_device_t *obj, char *buf, unsigned bufsz) {
  strcpy(buf, "Mock");
}

block_device_t dev = {
  .read = &dread,
  .write = &dwrite,
  .flush = &dflush,
  .length = &dlength,
  .describe = &ddescribe
};

int test() {
  // Create a new block cache group.
  disk_cache_group_t *grp = disk_cache_group_new();

  // And several new block caches.
  disk_cache_t *cache1 = disk_cache_new(grp, &dev);
  disk_cache_t *cache2 = disk_cache_new(grp, &dev);

  // Create some scratch vmspace.
  void *scratch1 = (void*)vmspace_alloc(&kernel_vmspace, 0x1000, 0);
  void *scratch2 = (void*)vmspace_alloc(&kernel_vmspace, 0x1000, 0);
  void *scratch3 = (void*)vmspace_alloc(&kernel_vmspace, 0x1000, 0);

  // Test caching of data.
  // CHECK: b1 = 1
  kprintf("b1 = %d\n", disk_cache_get(cache1, 0x0, scratch1));
  // CHECK: b2 = 1
  kprintf("b2 = %d\n", disk_cache_get(cache2, 0x0, scratch2));
  // CHECK: b3 = 1
  kprintf("b3 = %d\n", disk_cache_get(cache1, 0x1000, scratch3));

  // CHECK: c1[0] = 0x0 c1[1] = 0x4
  // CHECK: c2[0] = 0x0 c2[1] = 0x4
  // CHECK: c3[0] = 0x1000 c3[1] = 0x1004
  kprintf("c1[0] = %#x c1[1] = %#x\n",
          ((uint32_t*)scratch1)[0], ((uint32_t*)scratch1)[1]);
  kprintf("c2[0] = %#x c2[1] = %#x\n",
          ((uint32_t*)scratch2)[0], ((uint32_t*)scratch2)[1]);
  kprintf("c3[0] = %#x c3[1] = %#x\n",
          ((uint32_t*)scratch3)[0], ((uint32_t*)scratch3)[1]);

  // Attempt to evict a page. It should fail because all allocated
  // pages have handles.
  // CHECK: evict = 0
  kprintf("evict = %d\n", disk_cache_group_evict(grp, 0x1000));

  // Now release one handle, and check it is still cached (and that there
  // are no handles left).
  unmap((uintptr_t)scratch2, 1);
  disk_cache_release(cache2, 0x0);
  // CHECK: released: iscached 1 n_handles 0
  kprintf("released: iscached %d n_handles %d\n",
          disk_cache_is_cached(cache2, 0x0),
          disk_cache_get_n_handles(cache2, 0x0));

  // Try eviction again. It should succeed.
  // CHECK: evict = 1
  kprintf("evict = %d\n", disk_cache_group_evict(grp, 0x1000));

  // And now the address should not be cached.
  // CHECK: released: iscached 0 n_handles 0
  kprintf("released: iscached %d n_handles %d\n",
          disk_cache_is_cached(cache2, 0x0),
          disk_cache_get_n_handles(cache2, 0x0));

  // Get another handle to one address, and check the #handles increases
  // and they map to the same phys address.
  // CHECK: b4 = 1
  kprintf("b4 = %d\n", disk_cache_get(cache1, 0x1000, scratch2));

  // CHECK: nhandles = 2
  kprintf("nhandles = %d\n", disk_cache_get_n_handles(cache1, 0x1000));

  // CHECK: m1 = [[addr:0x[a-f0-9]*]]
  // CHECK: m2 = [[addr]]
  unsigned flags;
  kprintf("m1 = %#x\nm2 = %#x\n",
          (uint32_t)get_mapping((uintptr_t)scratch3, &flags),
          (uint32_t)get_mapping((uintptr_t)scratch2, &flags));
  
  return 0;
}

static prereq_t r[] = { {"vfs", NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "block-cache-test",
  .required = r,
  .load_after = NULL,
  .init = &test,
  .fini = NULL
};
module_t *test_module = &x;
