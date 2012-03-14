#ifndef SLAB_H
#define SLAB_H

#include "vmspace.h"

#define SLAB_SIZE 0x2000

typedef struct slab_cache {
  unsigned size;
  void *init;
  slab_footer_t *first;
  void *empty;
  vmspace_t *vms;

  void (*free_fn)(vmspace_t*,unsigned,uintptr_t,int);
  uintptr_t (*alloc_fn)(vmspace_t*,unsigned,int);

  /* FIXME: Add a lock here. */
} slab_cache_t;

int slab_cache_create(slab_cache_t *c, vmspace_t *vms, unsigned size, void *init);
int slab_cache_destroy(slab_cache_t *c);
void *slab_cache_alloc(slab_cache_t *c);
void slab_cache_free(slab_cache_t *c, void *obj);

#endif
