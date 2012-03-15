#include "adt/xbitmap.h"
#include "hal.h"
#include "string.h"
#include "assert.h"

/* Given a pointer to a block, return the address of the next block as a
   uint8_t** */
#define NEXT_BLK(x) (uint8_t**)((uintptr_t)x + xb->blocksz - sizeof(void*))

/* Create a new block and return a pointer to it. */
static uint8_t *newblock(xbitmap_t *xb) {
  uint8_t *x = (uint8_t*)xb->alloc(xb->blocksz, xb->alloc_p);
  memset(x, 0, xb->blocksz);
  return x;
}

/* Return a pointer to the n'th byte in the bitmap. If the bitmap is smaller
   than 'byte' bytes, and extend is nonzero, extend the bitmap. Else return
   NULL. */
static uint8_t *findbyte(xbitmap_t *xb, unsigned byte, int extend) {
  uint8_t *block = xb->firstblock;

  /* Early-exit if no memory has yet been allocated. */
  if (!block) {
    if (!extend) return NULL;
    xb->firstblock = block = newblock(xb);
  }

  while (block) {
    /* Number of bytes in this block */
    unsigned nb = xb->blocksz-sizeof(void*);
    
    /* If the byte index is past the end of this block */
    if (byte >= nb) {
      byte -= nb;
      uint8_t **blockaddr = NEXT_BLK(block);
      block = *blockaddr;

      /* We reached the last block - allocate a new one. */
      if (!block) {
        if (!extend) return NULL;
        block = newblock(xb);
        *blockaddr = block;
      }
      
    } else {
      /* Byte index is within this block. */
      return &block[byte];
    }
  }
  assert(0 && "algorithmic error!");
}

/* Returns the index of the least significant bit that is set in byte.
   If byte == 0, the behaviour is undefined. */
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

  /* The number of bytes available per block for storage. */
  unsigned nb = xb->blocksz - sizeof(void*);
  
  unsigned i = 0;
  while (block && i < xb->extent/8) {
    for(unsigned thisblock_i = 0; thisblock_i < nb; ++thisblock_i) {
      if (block[thisblock_i] != 0) {
        int idx = (i+thisblock_i) * 8 + lsb_set(block[thisblock_i]);
        return (idx > (int)xb->extent) ? -1 : idx;
      }
    }
    i += nb;
    block = * NEXT_BLK(block);
  }
  return -1;
}
