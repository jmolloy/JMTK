#ifndef XBITMAP_H
#define XBITMAP_H

#include "stdint.h"

typedef void* (*alloc_fn_t)(unsigned,void*);
typedef void (*free_fn_t)(void*,void*);

typedef struct xbitmap {
  alloc_fn_t alloc;
  free_fn_t free;
  void *alloc_p;

  int blocksz;
  uint8_t *firstblock;
  unsigned extent;
} xbitmap_t;

void xbitmap_init(xbitmap_t *xb, int blocksz, alloc_fn_t alloc, free_fn_t free, void *alloc_p);
void xbitmap_set(xbitmap_t *xb, unsigned idx);
void xbitmap_clear(xbitmap_t *xb, unsigned idx);
int xbitmap_isset(xbitmap_t *xb, unsigned idx);
int xbitmap_isclear(xbitmap_t *xb, unsigned idx);
int xbitmap_first_set(xbitmap_t *xb);

#endif
