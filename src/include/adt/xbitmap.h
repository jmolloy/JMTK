#ifndef XBITMAP_H
#define XBITMAP_H

/** @file xbitmap.h An extensible bitmap */

/** @addtogroup xbitmap Extensible bitmap
 
   This ADT exposes a bitmap structure that expands on demand. Initialisation of the 
   type allocates no memory - memory is only allocated when a mutator method is called.
 
   The bitmap has no maximum bound. It also takes an allocator/free function so that it
   can be used before kmalloc is fully up and running.
 
   @{ */

#include "stdint.h"

/** Allocator function type.
    @param sz The requested allocation size. This will be exactly @p blocksz given in the
              adt constructor.
    @param p  Opaque parameter as passed to the constructor. */
typedef void* (*alloc_fn_t)(unsigned sz, void *p);

/** Free function type.
    @param ptr The address to free, as returned by a call to an alloc_fn_t.
    @param p  Opaque parameter as passed to the constructor. */
typedef void (*free_fn_t)(void *ptr, void *p);

/** An extensible bitmap type. */
typedef struct xbitmap {
  alloc_fn_t alloc;
  free_fn_t free;
  void *alloc_p;       /* Opaque value to pass to alloc and free */

  int blocksz;
  uint8_t *firstblock; /* Pointer to the first block, or NULL */
  unsigned extent;     /* The largest index set/cleared so far */
} xbitmap_t;


/** Initialise an xbitmap object.
    @param xb The object to initialise
    @param blocksz The size of data to request from the allocator function, @p alloc
    @param alloc The memory allocator function
    @param free The memory release function
    @param alloc_p An opaque pointer that can be any value and is passed verbatim to the
                   alloc/free functions. */
void xbitmap_init(xbitmap_t *xb, int blocksz, alloc_fn_t alloc, free_fn_t free, void *alloc_p);

/** Sets a bit at index @p idx. If this requires the bitmap be expanded, it will be. */
void xbitmap_set(xbitmap_t *xb, unsigned idx);

/** Clears a bit at index @p idx. If this requires the bitmap be expanded, it will be. */
void xbitmap_clear(xbitmap_t *xb, unsigned idx);

/** Predicate: returns nonzero if the bit at index @p idx is set. */
int xbitmap_isset(xbitmap_t *xb, unsigned idx);

/** Predicate: returns nonzero if the bit at index @p idx is clear. */
int xbitmap_isclear(xbitmap_t *xb, unsigned idx);

/** Return the index of the first bit that is set, or -1 if no bits are set at all. */
int xbitmap_first_set(xbitmap_t *xb);

/** @} */

#endif
