#include "hal.h"
#include "stdio.h"
#include "assert.h"
#include "adt/hashtable.h"
#include "kmalloc.h"
#include "string.h"

typedef struct ht_bucket {
  void *key;
  void *data;
  struct ht_bucket *next;
} ht_bucket_t;

static uintptr_t hash(hashtable_t *ht, void *key) {
  uintptr_t k = (uintptr_t)key;
  return k % ht->nbuckets;
}

hashtable_t hashtable_new(unsigned nbuckets) {
  hashtable_t ht;
  ht.buckets = kmalloc(sizeof(ht_bucket_t) * nbuckets);
  memset(ht.buckets, 0, sizeof(ht_bucket_t) * nbuckets);
  ht.nbuckets = nbuckets;

  return ht;
}

void *hashtable_get(hashtable_t *ht, void *key) {
  ht_bucket_t *bucket = ht->buckets[hash(ht, key)];
  while (bucket) {
    if (bucket->key == key)
      return bucket->data;
    bucket = bucket->next;
  }
  return NULL;
}

void hashtable_set(hashtable_t *ht, void *key, void *data) {
  /* FIXME: implement dynamic resizing? */
  uintptr_t h = hash(ht, key);
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
