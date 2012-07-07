#ifndef HASHTABLE_H
#define HASHTABLE_H

typedef struct hashtable {
  struct ht_bucket **buckets;
  unsigned nbuckets;
} hashtable_t;

hashtable_t hashtable_new(unsigned nbuckets);
void *hashtable_get(hashtable_t *ht, void *key);
void hashtable_set(hashtable_t *ht, void *key, void *data);
void hashtable_destroy(hashtable_t *ht);

#endif
