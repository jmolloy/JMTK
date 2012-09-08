#include "hal.h"
#include "stdio.h"
#include "assert.h"
#include "adt/hashtable.h"
#include "kmalloc.h"
#include "string.h"

typedef struct ht_bucket {
  uint64_t key;
  uint64_t data;
  struct ht_bucket *next;
} ht_bucket_t;

static uint64_t hash(hashtable_t *ht, uint64_t key) {
  return key % ht->nbuckets;
}

hashtable_t hashtable_new(unsigned nbuckets) {
  hashtable_t ht;
  ht.buckets = kmalloc(sizeof(ht_bucket_t) * nbuckets);
  memset(ht.buckets, 0, sizeof(ht_bucket_t) * nbuckets);
  ht.nbuckets = nbuckets;

  return ht;
}

void *hashtable_get(hashtable_t *ht, void *key) {
  return (void*)(uintptr_t)hashtable_get64(ht, (uint64_t)(uintptr_t)key);
}

uint64_t hashtable_get64(hashtable_t *ht, uint64_t key) {
  ht_bucket_t *bucket = ht->buckets[hash(ht, key)];
  while (bucket) {
    if (bucket->key == key)
      return bucket->data;
    bucket = bucket->next;
  }
  return 0;
}

void hashtable_set(hashtable_t *ht, void *key, void *data) {
  hashtable_set64(ht, (uint64_t)(uintptr_t)key, (uint64_t)(uintptr_t)data);
}

void hashtable_set64(hashtable_t *ht, uint64_t key, uint64_t data) {
  /* FIXME: implement dynamic resizing? */
  uint64_t h = hash(ht, key);
  ht_bucket_t *bucket = ht->buckets[h];

  while (bucket) {
    if (bucket->key == key) {
      bucket->data = data;
      return;
    }
    bucket = bucket->next;
  }

  bucket = kmalloc(sizeof(ht_bucket_t));
  bucket->next = ht->buckets[h];
  bucket->key = key;
  bucket->data = data;

  ht->buckets[h] = bucket;
}

void hashtable_destroy(hashtable_t *ht) {
  for (unsigned i = 0; i < ht->nbuckets; ++i) {
    ht_bucket_t *bucket = ht->buckets[i];
    while (bucket) {
      ht_bucket_t *b = bucket->next;
      kfree(bucket);
      bucket = b;
    }
  }
  kfree(ht->buckets);
  ht->buckets = NULL;
  ht->nbuckets = 0;
}
