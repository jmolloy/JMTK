#ifndef DISK_CACHE_H
#define DISK_CACHE_H

#include "adt/vector.h"
#include "hal.h"
#include "types.h"

typedef struct disk_cache disk_cache_t;
typedef struct disk_cache_group disk_cache_group_t;

disk_cache_group_t *disk_cache_group_new();
void disk_cache_group_destroy(disk_cache_group_t *group);
disk_cache_group_t *disk_cache_group_get_default();
bool disk_cache_group_evict(disk_cache_group_t *group, uint64_t bytes);

disk_cache_t *disk_cache_new(disk_cache_group_t *group,
                             block_device_t *dev);
void disk_cache_destroy(disk_cache_t *cache);
bool disk_cache_get(disk_cache_t *cache, uint64_t addr, void *map_at);
void disk_cache_release(disk_cache_t *cache, uint64_t addr);

bool disk_cache_is_cached(disk_cache_t *cache, uint64_t addr);
unsigned disk_cache_get_n_handles(disk_cache_t *cache, uint64_t addr);

#endif
