#include "hal.h"
#include "directory_cache.h"
#include "kmalloc.h"
#include "string.h"
#include "assert.h"

directory_cache_t *directory_cache_new(vector_t entries) {
  directory_cache_t *cache = kmalloc(sizeof(directory_cache_t));
  cache->write_buf = vector_clone(entries);

  return cache;
}

void directory_cache_add(directory_cache_t *cache, dirent_t *entry) {
  vector_add(&cache->write_buf, entry);
}

inode_t *directory_cache_get(directory_cache_t *cache, const char *path) {
  assert(cache);
  for (unsigned i = 0; i < vector_length(&cache->write_buf); ++i) {
    dirent_t *dent = vector_get(&cache->write_buf, i);
    if (!strcmp(dent->name, path)) return dent->ino;
  }
  return NULL;
}

vector_t directory_cache_get_all(directory_cache_t *cache) {
  assert(cache);
  return vector_clone(cache->write_buf);
}

void directory_cache_destroy(directory_cache_t *cache) {
  assert(cache);
  vector_destroy(&cache->write_buf);
  kfree(cache);
}
