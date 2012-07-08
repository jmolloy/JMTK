#include "assert.h"
#include "hal.h"
#include "slab.h"
#include "string.h"

typedef struct slab_footer {
  struct slab_footer *next;
} slab_footer_t;

#define SLAB_ADDR_MASK ~(SLAB_SIZE-1)
#define FOOTER_FOR_PTR(x) (void*)(((uintptr_t) x & SLAB_ADDR_MASK) + SLAB_SIZE - sizeof(slab_footer_t))
#define START_FOR_FOOTER(f) ((uintptr_t)f & SLAB_ADDR_MASK)

/* Internal functions */
/* Destroy a slab, given its footer. */
static void destroy(slab_cache_t *c, slab_footer_t *f);
/* Create a new slab, in the given cache. */
static slab_footer_t *create(slab_cache_t *c);
/* Mark a slab entry as used - obj is a pointer relative to the start of the slab. */
static void mark_used(slab_cache_t *c, slab_footer_t *f, void *obj);
/* Mark a slab entry as unused - obj is a pointer relative to the start of the slab. */
static void mark_unused(slab_cache_t *c, slab_footer_t *f, void *obj);
/* Predicate: are all the entries in the given slab unused? */
static int all_unused(slab_cache_t *c, slab_footer_t *f);
/* Return the address of an empty object in the given slab, or NULL if all full. */
static void *find_empty_obj(slab_cache_t *c, slab_footer_t *f);

int slab_cache_create(slab_cache_t *c, vmspace_t *vms, unsigned size, void *init) {
  c->size = size;
  c->init = init;
  c->first = NULL;
  c->empty = NULL;
  c->vms = vms;
  spinlock_init(&c->lock);
  return 0;
}

int slab_cache_destroy(slab_cache_t *c) {
  slab_footer_t *s = c->first;
  while (s) {
    slab_footer_t *s_ = s->next;
    destroy(c, s);
    s = s_;
  }
  c->first = NULL;
  return 0;
}

void *slab_cache_alloc(slab_cache_t *c) {
  spinlock_acquire(&c->lock);

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
    c->first = create(c);
    c->first->next = f;
    
    obj = (void*)START_FOR_FOOTER(c->first);
    mark_used(c, c->first, obj);

    c->empty = find_empty_obj(c, c->first);

  }
  if (c->init)
    memcpy(obj, c->init, c->size);

  spinlock_release(&c->lock);
  return obj;
}

void slab_cache_free(slab_cache_t *c, void *obj) {
  spinlock_acquire(&c->lock);
  assert(c->first && "Trying to free from an empty cache!");
  
  slab_footer_t *f = FOOTER_FOR_PTR(obj);

  mark_unused(c, f, obj);

  if (!c->empty || c->empty > obj)
    c->empty = obj;

  if (!f->next && all_unused(c, f)) {
    slab_footer_t *f2 = c->first;
    if (f2 == f) {
      c->first = NULL;
    } else {
      while (f2->next != f)
        f2 = f2->next;
      f2->next = NULL;
    }
    if (FOOTER_FOR_PTR(c->empty) == f)
      c->empty = NULL;
    destroy(c, f);
  }
  spinlock_release(&c->lock);
}

static void destroy(slab_cache_t *c, slab_footer_t *f) {
  vmspace_free(c->vms, SLAB_SIZE, START_FOR_FOOTER(f), /*free_phys=*/1);
}

/* Return the number of entries in a bitmap for an object of 'obj_sz'. */
static inline unsigned bitmap_num(int obj_sz) {
  unsigned avail = SLAB_SIZE - sizeof(slab_footer_t);
  return avail / obj_sz;
}

/* Return the size in bytes of a bitmap for an object of 'obj_sz'. */
static inline unsigned bitmap_sz(int obj_sz) {
  return bitmap_num(obj_sz) / 8 + 1;
}

/* Return the bitmap entry index that represents 'obj'. */
static inline unsigned bitmap_idx(slab_footer_t *f, void *obj, int obj_sz) {
  return ( (uintptr_t)obj - START_FOR_FOOTER(f) ) / obj_sz;
}

static slab_footer_t *create(slab_cache_t *c) {
  uintptr_t addr = vmspace_alloc(c->vms, SLAB_SIZE, /*alloc_phys=*/PAGE_WRITE);

  slab_footer_t *f = FOOTER_FOR_PTR(addr);
  f->next = NULL;
  
  /* Initialise the used/free bitmap. */
  uintptr_t bm = (uintptr_t)f - bitmap_sz(c->size);
  memset((uint8_t*)bm, 0, bitmap_sz(c->size));

  return f;
}

static void mark_used(slab_cache_t *c, slab_footer_t *f, void *obj) {
  unsigned idx = bitmap_idx(f, obj, c->size);
  unsigned sz = bitmap_sz(c->size);

  unsigned byte = idx >> 3;
  unsigned bit = idx & 7;
  uint8_t *ptr = (uint8_t*)f - sz + byte;
  *ptr |= 1 << bit;
}

static void mark_unused(slab_cache_t *c, slab_footer_t *f, void *obj) {
  unsigned idx = bitmap_idx(f, obj, c->size);
  unsigned sz = bitmap_sz(c->size);

  unsigned byte = idx >> 3;
  unsigned bit = idx & 7;
  uint8_t *ptr = (uint8_t*)f - sz + byte;
  *ptr &= ~(1 << bit);
}

static int all_unused(slab_cache_t *c, slab_footer_t *f) {
  unsigned sz = bitmap_sz(c->size);
  uint8_t *p = (uint8_t*)f - sz;

  /* FIXME: Use something fast like memcmp? */
  for (unsigned i = 0; i < sz; ++i)
    if (*p++ != 0) return 0;
  return 1;
}

static int lsb_clear(uint8_t byte) {
  int i = 0;
  while ((byte & 1) == 1) {
    ++i;
    byte >>= 1;
  }
  return i;
}

static void *find_empty_obj(slab_cache_t *c, slab_footer_t *f) {
  unsigned sz = bitmap_sz(c->size);
  uint8_t *p = (uint8_t*)f - sz;

  for (unsigned i = 0; i < sz; ++i) {
    if (*p != 0xFF) {
      unsigned idx = i * 8 + lsb_clear(*p);
      return (idx >= bitmap_num(c->size)) ? NULL : (void*)(START_FOR_FOOTER(f) + c->size*idx);
    }
    ++p;
  }
  return NULL;
}
