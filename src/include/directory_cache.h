#ifndef DIRECTORY_CACHE_H
#define DIRECTORY_CACHE_H

#include "adt/vector.h"
#include "vfs.h"

/* FIXME: implement read buffers. */
typedef struct directory_cache {
  vector_t write_buf;
} directory_cache_t;

directory_cache_t *directory_cache_new(vector_t entries);
void directory_cache_add(directory_cache_t *cache, dirent_t *entry);
inode_t *directory_cache_get(directory_cache_t *cache, const char *path);
vector_t directory_cache_get_all(directory_cache_t *cache);
void directory_cache_destroy();

#endif
