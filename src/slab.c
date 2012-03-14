#include "hal.h"
#include "slab.h"

typedef struct slab_footer {
  struct slab_footer *next;
} slab_footer_t;


#define SLAB_ADDR_MASK ~(SLAB_SIZE-1)
#define FOOTER_FOR_PTR(x) (void*)(((uintptr_t) x & SLAB_ADDR_MASK) + SLAB_SIZE - sizeof(slab_footer_t))
#define START_FOR_FOOTER(f) ((uintptr_t)f & SLAB_ADDR_MASK)

static void destroy(slab_footer_t *f);
static slab_footer_t *create(slab_cache_t *c);
static void mark_used(slab_cache_t *c, slab_footer_t *f, void *obj);
static void mark_unused(slab_cache_t *c, slab_footer_t *f, void *obj);
static int all_unused(slab_cache_t *c, slab_footer_t *f);

int slab_cache_create(slab_cache_t *c, vmspace_t *vms, unsigned size, void *init) {
  c->size = size;
  c->init = init;
  c->first = NULL;
  c->empty = NULL;
  c->vmspace = vms;
  return 0;
}

int slab_cache_destroy(slab_cache_t *c) {
  slab_footer_t *s = c->first;
  while (s) {
    slab_footer_t *s_ = s->next;
    destroy(s);
    s = s_;
  }
  c->first = NULL;
  return 0;
}

void *slab_cache_alloc(slab_cache_t *c) {
  void *obj;
  if (c->empty) {

    obj = c->empty;
    slab_footer_t *f = FOOTER_FOR_PTR(obj);
    mark_used(c, f, obj);

    while (f) {
      c->empty = find_empty_obj(c, f);
      if (c->empty) break;
      f = f->next;
    }

  } else {

    /* No empty pointer - must create a new slab. */
    slab_footer_t *f = c->first;
    while (f->next) {
      f = f->next;
    }
    f->next = create(c);
    
    void *obj = find_empty_obj(c, f);
    mark_used(c, f, obj);

    c->empty = find_empty_obj(c, f);

  }
  memcpy(obj, c->init, c->size);
  return obj;
}

void slab_cache_free(slab_cache_t *c, void *obj) {
  slab_footer_t *f = FOOTER_FOR_PTR(obj);

  mark_unused(c, f, obj);
  if (!c->empty || c->empty > obj)
    c->empty = obj;

  while (!f->next && all_unused(c, f)) {
    slab_footer_t *f2 = c->first;
    while (f2->next != f)
      f2 = f2->next;

    f2->next = NULL;
    destroy(f);

    /* Work our way back down the chain, freeing memory where
       possible. */
    f = f2;
  }
}

static void destroy(slab_cache_t *c, slab_footer_t *f) {
  vmspace_free(c->vms, SLAB_SIZE, START_FOR_FOOTER(f), /*free_phys=*/1);
}

static inline unsigned bitmap_num(int obj_sz) {
  unsigned avail = SLAB_SIZE - sizeof(slab_footer_t);
  return avail / obj_sz;
}

static inline unsigned bitmap_sz(int obj_sz) {
  return bitmap_num(obj_sz) / 8;
}

static inline unsigned bitmap_offs(slab_footer_t *f, void *obj, int obj_sz) {
  return ( (uintptr_t)obj - START_FOR_FOOTER(f) ) / obj_sz;
}

static slab_footer_t *create(slab_cache_t *c) {
  uintptr_t addr = vmspace_alloc(c->vms, SLAB_SIZE, /*alloc_phys=*/PAGE_WRITE);

  slab_footer_t *f = FOOTER_FOR_PTR(addr);
  f->next = NULL;
  
  /* Initialise the used/free bitmap. */
  uintptr_t bm = (uintptr_t)f - bitmap_sz(c->size);
  memset((uint8_t*)bm, 0, bitmap_sz(c->size));
}

static void mark_used(slab_cache_t *c, slab_footer_t *f, void *obj) {
  unsigned offs = bitmap_offs(f, obj, c->size);
  unsigned sz = bitmap_sz(c->size);

  unsigned byte = offs >> 3;
  unsigned bit = offs & 7;
  uint8_t *ptr = (uint8_t*)f - sz + byte;
  *ptr |= 1 << bit;
}

static void mark_unused(slab_cache_t *c, slab_footer_t *f, void *obj) {
  unsigned offs = bitmap_offs(f, obj, c->size);
  unsigned sz = bitmap_sz(c->size);

  unsigned byte = offs >> 3;
  unsigned bit = offs & 7;
  uint8_t *ptr = (uint8_t*)f - sz + byte;
  *ptr &= ~(1 << bit);
}

static int all_unused(slab_cache_t *c, slab_footer_t *f) {
  unsigned sz = bitmap_sz(c->size);
  uint8_t *p = (uint8_t*)f - sz;

  /* FIXME: Use something fast like memcmp? */
  for (unsigned i = 0; i < sz; ++i)
    if (p != 0) return 0;
  return 1;
}
