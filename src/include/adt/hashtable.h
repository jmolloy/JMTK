#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "types.h"

typedef struct hashtable {
  struct ht_bucket **buckets;
  unsigned nbuckets;
} hashtable_t;

hashtable_t hashtable_new(unsigned nbuckets);
void *hashtable_get(hashtable_t *ht, void *key);
uint64_t hashtable_get64(hashtable_t *ht, uint64_t key);
void hashtable_set(hashtable_t *ht, void *key, void *data);
void hashtable_set64(hashtable_t *ht, uint64_t key, uint64_t data);
void hashtable_destroy(hashtable_t *ht);

#endif
