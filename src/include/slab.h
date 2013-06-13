/**#3
   Slab allocator
   ==============

   Now that we have a "large object" allocator, let's focus on the "small object" allocator.

   There are many types of allocator we can use for small objects - a fairly standard allocator is `dlmalloc <http://g.oswego.edu/dl/html/malloc.html>`_ which is reused for many hobby kernels.

   The Solaris kernel guys developed an allocator called the `Slab allocator <http://en.wikipedia.org/wiki/Slab_allocation>`_ which was later adopted into Linux, and was the main kernel allocator until they added the "SLUB" allocator (which is SLAB-based).

   The Slab scheme is fairly elegant and compact, so we're going to implement it (or a variant of it) for our small object allocator.

   Slab basics
   -----------

   A slab allocator sits on top of another memory allocator. It requests large amounts of memory (a **slab**) at a time, then carves those slabs up itself.

   Slabs belong to a **cache**. All allocation requests to a cache return the same size of memory. In order to allocate different sizes of object, different caches must be made (one for each size of object you want to return).

   The original Solaris Slab allocator extended this concept. Not only was each cache dedicated to one *size* of object, but it could also be dedicated to one *type* of object - you'd have a cache for your filesystem handles, one for your thread handles and other high-churn, small objects. The advantage of this is that the cache can return an object that has already been initialised - instead of memsetting or setting struct members to a default value after calling the allocator, the allocator can do it for you more efficiently!

   So you create one or more caches, and a cache maintains a list of slabs it has gained from the underlying large memory allocator. It takes those slabs and carves them up into N objects of the same size. At the end of the slab, it allocates space for a simple bitmap which allows it to track which of the objects in this slab are allocated. If none are allocated, it can return the slab to the large memory allocator.
*/
/**
   Slab implementation
   -------------------

   We'll obviously build our slab allocator on top of the "vmspace" allocator we just defined. The API is simple, and involves creating, allocating from, freeing from and destroying caches. { */
#ifndef SLAB_H
#define SLAB_H

#include "vmspace.h"

/* 8KB slab sizes */
#define SLAB_SIZE 0x2000

typedef struct slab_cache {
  unsigned size;                /* The size of objects this cache returns, in bytes. */
  void *init;                   /* Optional initializer that can be applied to every
                                   returned object. */
  struct slab_footer *first;    /* First slab footer in a linked list - all
                                   of these slabs are full or partially full. */
  vmspace_t *vms;               /* Parent "large object" allocator. */

  spinlock_t lock;
} slab_cache_t;

/* Create a new slab cache. Use 'vms' as the parent "large object" allocator,
   and return objects of 'size' bytes. If 'init' is non-NULL, all objects will
   have the value located at 'init' upon allocation. */
int slab_cache_create(slab_cache_t *c, vmspace_t *vms, unsigned size, void *init);
int slab_cache_destroy(slab_cache_t *c);
void *slab_cache_alloc(slab_cache_t *c);
void slab_cache_free(slab_cache_t *c, void *obj);

#endif
