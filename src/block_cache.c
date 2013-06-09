#include "adt/hashtable.h"
#include "assert.h"
#include "block_cache.h"
#include "kmalloc.h"
#include "stdlib.h"
#include "stdio.h"
#include "vmspace.h"

#ifdef DEBUG_block_cache
# define dbg(args...) kprintf("block_cache: " args)
#else
# define dbg(args...)
#endif

typedef struct page {
  disk_cache_t *cache;
  uint64_t phys_addr;
  uint64_t offset;
  unsigned use_count;

  struct page *next, *prev;
} page_t;

/* FIXME: How does this compile? disk_cache_group_t is not defined! */
struct disk_cache_group {
  vector_t caches;
  page_t *mru_page;
  page_t *lru_page;
  mutex_t lock;
};

struct disk_cache {
  disk_cache_group_t *parent;
  block_device_t *dev;
  hashtable_t pages;
};

static disk_cache_group_t *default_group = NULL;

static void touch(disk_cache_group_t *group, page_t *pg) {
  if (pg == group->mru_page)
    return;

  if (pg->prev)
    pg->prev->next = pg->next;
  if (pg->next)
    pg->next->prev = pg->prev;

  if (pg == group->lru_page)
    group->lru_page = pg->prev;

  if (group->mru_page)
    group->mru_page->prev = pg;

  pg->prev = NULL;
  pg->next = group->mru_page;
  group->mru_page = pg;
}

static page_t *evict(disk_cache_group_t *group, page_t *pg) {
  page_t *prev = pg->prev;

  if (pg->prev)
    pg->prev->next = pg->next;
  if (pg->next)
    pg->next->prev = pg->prev;

  if (pg == group->lru_page)
    group->lru_page = pg->prev;
  if (pg == group->mru_page)
    group->mru_page = pg->next;
  
  /* FIXME: writeback to disk! */
  free_page(pg->phys_addr);

  hashtable_set64(&pg->cache->pages, pg->offset >> get_page_shift(), 0);

  /* FIXME: Use a slab cache for this. */
  kfree(pg);

  return prev;
}

disk_cache_group_t *disk_cache_group_new() {
  disk_cache_group_t *group = kmalloc(sizeof(disk_cache_group_t));
  group->caches = vector_new(sizeof(disk_cache_t), 4);
  mutex_init(&group->lock);
  return group;
}

void disk_cache_group_destroy(disk_cache_group_t *group) {
  vector_destroy(&group->caches);
  /* FIXME: Destroy all related caches too? */
  kfree(group);
}

disk_cache_group_t *disk_cache_group_get_default() {
  if (default_group == NULL)
    default_group = disk_cache_group_new();
  return default_group;
}

bool disk_cache_group_evict(disk_cache_group_t *group, uint64_t bytes) {
  uint64_t npages = bytes >> get_page_shift();
  mutex_acquire(&group->lock);

  page_t *pg = group->lru_page;
  while (npages > 0 && pg) {
    if (pg->use_count == 0) {
      pg = evict(group, pg);
      --npages;
    } else {
      pg = pg->prev;
    }
  }

  mutex_release(&group->lock);
  return npages == 0;
}

disk_cache_t *disk_cache_new(disk_cache_group_t *group,
                             block_device_t *dev) {
  disk_cache_t *cache = kmalloc(sizeof(disk_cache_t));
  cache->parent = group;
  cache->dev = dev;
  cache->pages = hashtable_new(1031);
  return cache;
}

void disk_cache_destroy(disk_cache_t *cache) {
  mutex_acquire(&cache->parent->lock);
  
  uintptr_t v = vmspace_alloc(&kernel_vmspace, get_page_size(), 0);

  page_t *pg = cache->parent->mru_page;
  while (pg) {
    dbg("pg = %x (next = %x)\n", pg, pg->next);
    if (pg->cache == cache) {
      dbg("write %x\n", pg);
      map(v, pg->phys_addr, 1, PAGE_WRITE);
      cache->dev->write(cache->dev, pg->offset,
                        (void*)v, get_page_size());
      unmap(v, 1);

      if (pg->next)
        pg->next->prev = pg->prev;
      if (pg->prev)
        pg->prev->next = pg->next;
      page_t *npg = pg->next;
      kfree(pg);
      pg = npg;
    } else {
      pg = pg->next;
    }
  }
  dbg("destroy end");
  mutex_release(&cache->parent->lock);

  hashtable_destroy(&cache->pages);
  kfree(cache);

}

bool disk_cache_get(disk_cache_t *cache, uint64_t addr, void *map_at) {
  dbg("get(%#x)\n", (uint32_t)addr);
  addr >>= get_page_shift();

  mutex_acquire(&cache->parent->lock);

  page_t *pg = (page_t*)(uintptr_t)hashtable_get64(&cache->pages, addr);
  if (!pg) {
    /* FIXME: Use a slab cache for this. */
    pg = kmalloc(sizeof(page_t));
    pg->next = pg->prev = NULL;
    pg->cache = cache;
    pg->offset = addr << get_page_shift();

    hashtable_set64(&cache->pages, addr, (uint64_t)(uintptr_t)pg);
    
    pg->phys_addr = alloc_page(PAGE_REQ_NONE);
    /* FIXME: Invoke cache eviction here. */
    assert(pg->phys_addr != ~0ULL && "No physical pages available!");
    
    pg->use_count = 1;
    map((uintptr_t)map_at, pg->phys_addr, 1, PAGE_WRITE);

    /* Add as the most recently used page. */
    pg->next = cache->parent->mru_page;
    if (cache->parent->mru_page)
      cache->parent->mru_page->prev = pg;
    cache->parent->mru_page = pg;

    /* ... and maybe as the least recently used page? */
    if (cache->parent->lru_page == 0)
      cache->parent->lru_page = pg;
    dbg("mapping addr %x to %x\n", pg->phys_addr, map_at);
    mutex_release(&cache->parent->lock);
    cache->dev->read(cache->dev, addr << get_page_shift(),
                     map_at, get_page_size());
    return true;
  } else {
    ++pg->use_count;
    touch(cache->parent, pg);
    mutex_release(&cache->parent->lock);
    dbg("mapping addr %x to %x\n", pg->phys_addr, map_at);
    map((uintptr_t)map_at, pg->phys_addr, 1, PAGE_WRITE);
    return true;
  }
}

void disk_cache_release(disk_cache_t *cache, uint64_t addr) {
  dbg("release(%#x)\n", (uint32_t)addr);
  addr >>= get_page_shift();

  mutex_acquire(&cache->parent->lock);

  page_t *pg = (page_t*)(uintptr_t)hashtable_get64(&cache->pages, addr);
  assert(pg);
  
  --pg->use_count;

  mutex_release(&cache->parent->lock);
}

bool disk_cache_is_cached(disk_cache_t *cache, uint64_t addr) {
  addr >>= get_page_shift();
  mutex_acquire(&cache->parent->lock);
  page_t *pg = (page_t*)(uintptr_t)hashtable_get64(&cache->pages, addr);
  mutex_release(&cache->parent->lock);
  return pg != NULL;
}

unsigned disk_cache_get_n_handles(disk_cache_t *cache, uint64_t addr) {
  addr >>= get_page_shift();
  mutex_acquire(&cache->parent->lock);
  page_t *pg = (page_t*)(uintptr_t)hashtable_get64(&cache->pages, addr);
  unsigned usecnt = 0;
  if (pg)
    usecnt = pg->use_count;
  mutex_release(&cache->parent->lock);
  return usecnt;
}
