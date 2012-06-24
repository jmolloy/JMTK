#include "adt/xbitmap.h"
#include "hal.h"
#include "string.h"
#include "stdio.h"
#include "assert.h"

/* Returns the index of the least significant bit that is set in byte.
   If byte == 0, the behaviour is undefined. */
static int lsb_set(uint8_t byte) {
  int i = 0;
  /* FIXME: Surely something in Hacker's delight could speed this up? */
  while ((byte & 1) == 0) {
    ++i;
    byte >>= 1;
  }
  return i;
}

static int lsb_set_ptr(uintptr_t byte) {
  int i = 0;
  /* FIXME: Surely something in Hacker's delight could speed this up? */
  while ((byte & 1) == 0) {
    ++i;
    byte >>= 1;
  }
  return i;
}

void xbitmap_init(xbitmap_t *xb, uint8_t *storage, int64_t max_extent) {
  xb->max_extent = max_extent;
  xb->data = storage;

  memset(xb->data, 0, max_extent / 8 + 1);
}

void xbitmap_set(xbitmap_t *xb, unsigned idx) {
  xb->data[idx/8] |= (1 << (idx%8));
  assert(xbitmap_isset(xb, idx));
}

void xbitmap_clear(xbitmap_t *xb, unsigned idx) {
  xb->data[idx/8] &= ~(1 << (idx%8));
}

int xbitmap_isset(xbitmap_t *xb, unsigned idx) {
  return (xb->data[idx/8] & (1 << (idx%8))) ? 1 : 0;
}
int xbitmap_isclear(xbitmap_t *xb, unsigned idx) {
  return !xbitmap_isset(xb, idx);
}

int64_t xbitmap_first_set(xbitmap_t *xb) {
  uintptr_t *d = (uintptr_t*)xb->data;

  for (uint64_t i = 0; i < xb->max_extent / 8 + 1; i ++) {
    if (xb->data[i] == 0) continue;

    int64_t idx = i * 8 + lsb_set(xb->data[i]);
    return (idx > xb->max_extent) ? -1 : idx;
  }
  return -1;
}
