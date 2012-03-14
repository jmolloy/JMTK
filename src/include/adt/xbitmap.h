#ifndef XBITMAP_H
#define XBITMAP_H

#include "stdint.h"

typedef struct xbitmap {
  int use_kmalloc;
  int blocksz;
  uint8_t *firstblock;
  unsigned extent;
} xbitmap_t;

void xbitmap_init(xbitmap_t *xb, int use_kmalloc);
void xbitmap_set(xbitmap_t *xb, unsigned idx);
void xbitmap_clear(xbitmap_t *xb, unsigned idx);
int xbitmap_isset(xbitmap_t *xb, unsigned idx);
int xbitmap_isclear(xbitmap_t *xb, unsigned idx);
int xbitmap_first_set(xbitmap_t *xb);

#endif
