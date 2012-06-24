#ifndef BITMAP_H
#define BITMAP_H

/* This ADT exposes a statically sized bitmap structure. Initialisation of the type
   calls the given memory allocator to allocate memory. */

#include "stdint.h"

/* A bitmap type. */
typedef struct xbitmap {
  uint8_t *data;
  int64_t max_extent;
} xbitmap_t;


void xbitmap_init(xbitmap_t *xb, uint8_t *storage, int64_t max_extent);

/* Sets a bit at index idx. */
void xbitmap_set(xbitmap_t *xb, unsigned idx);

/* Clears a bit at index idx. */
void xbitmap_clear(xbitmap_t *xb, unsigned idx);

/* Predicate: returns nonzero if the bit at index idx is set. */
int xbitmap_isset(xbitmap_t *xb, unsigned idx);

/* Predicate: returns nonzero if the bit at index idx is clear. */
int xbitmap_isclear(xbitmap_t *xb, unsigned idx);

/* Return the index of the first bit that is set, or -1 if no bits are
   set at all. */
int64_t xbitmap_first_set(xbitmap_t *xb);

#endif
