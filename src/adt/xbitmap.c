#include "adt/xbitmap.h"
#include "hal.h"
#include "string.h"

#define NEXT_BLK(x) (uint8_t**)((uintptr_t)x + xb->blocksz - sizeof(void*))

static uint8_t *newblock(xbitmap_t *xb) {
  uint8_t *x = (uint8_t*)xb->alloc(xb->blocksz, xb->alloc_p);
  memset(x, 0, xb->blocksz);
  return x;
}
  
static uint8_t *findbyte(xbitmap_t *xb, unsigned byte, int extend) {
  uint8_t *block = xb->firstblock;
  uint8_t **blockaddr;

  if (!block) {
    if (!extend) return NULL;
    xb->firstblock = newblock(xb);
    return findbyte(xb, byte, extend);
  }

  while (block) {
    if (byte >= xb->blocksz-sizeof(void*)) {
      byte -= xb->blocksz-sizeof(void*);
      blockaddr = NEXT_BLK(block);
      block = *blockaddr;

      if (!block) {
        if (!extend) return NULL;
        block = newblock(xb);
        *blockaddr = block;
      }
      
    } else {
      return &block[byte];
    }
  }
  panic("xbitmap.c:findbyte: algorithmic error!");
}

static int lsb_set(uint8_t byte) {
  int i = 0;
  while ((byte & 1) == 0) {
    ++i;
    byte >>= 1;
  }
  return i;
}

void xbitmap_init(xbitmap_t *xb, int blocksz, alloc_fn_t alloc, free_fn_t free, void *alloc_p) {
  xb->blocksz = blocksz;
  xb->alloc = alloc;
  xb->free = free;
  xb->alloc_p = alloc_p;
  xb->firstblock = NULL;
  xb->extent = 0;
}

void xbitmap_set(xbitmap_t *xb, unsigned idx) {
  uint8_t *byte = findbyte(xb, idx/8, 1);
  *byte |= (1 << (idx%8));

  if (idx > xb->extent) xb->extent = idx;
}

void xbitmap_clear(xbitmap_t *xb, unsigned idx) {
  uint8_t *byte = findbyte(xb, idx/8, 1);
  *byte &= ~(1 << (idx%8));

  if (idx > xb->extent) xb->extent = idx;
}

int xbitmap_isset(xbitmap_t *xb, unsigned idx) {
  uint8_t *byte = findbyte(xb, idx/8, 0);
  return byte && *byte & (1 << (idx%8));
}
int xbitmap_isclear(xbitmap_t *xb, unsigned idx) {
  return !xbitmap_isset(xb, idx);
}

int xbitmap_first_set(xbitmap_t *xb) {
  uint8_t *block = xb->firstblock;
  unsigned i = 0;
  while (block && i < xb->extent/8) {
    for(unsigned thisblock_i = 0; thisblock_i < xb->blocksz-sizeof(void*); ++thisblock_i) {
      if (block[thisblock_i] != 0)
        return (i+thisblock_i) * 8 + lsb_set(block[thisblock_i]);
    }
    i += xb->blocksz - sizeof(void*);
    block = (uint8_t*) ((uintptr_t)block + xb->blocksz - sizeof(void*));
  }
  return -1;
}
