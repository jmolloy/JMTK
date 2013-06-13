#include "assert.h"
#include "hal.h"
#include "slab.h"
#include "string.h"

/**#4
   A slab is divided into three main parts - the main content, the allocation bitmap and
   the footer which consists of a pointer to the footer of the next slab (linked list).

   .. image:: ../../../doc/slab-layout.svg
       :class: floated

   To simplify our allocator, we can make the assumption that the parent allocator (``vmspace``)
   only returns chunks of memory that are naturally aligned. So if we ask for an 8KB chunk of
   memory, the resulting pointer will be 8KB-aligned.

   We can use this to easily calculate the start of a slab from any pointer inside it. If you
   have a pointer to a slab footer or one of the objects inside, we can just perform a simple
   mask and get the start of the slab. This then lets us calculate the address of the bitmap
   and footer. { */

typedef struct slab_footer {
  struct slab_footer *next;
} slab_footer_t;

/* Mask for finding the start of a slab. */
#define SLAB_ADDR_MASK ~(SLAB_SIZE-1)
/* Given an object size, calculate the size of the bitmap required. This will
   be an overestimate. */
#define BITMAP_SIZE(obj_sz) ((SLAB_SIZE / obj_sz) / 8 + 1)

/* Given an address inside a slab, find the slab's start address. */
#define START_FOR_PTR(f) ((uintptr_t)f & SLAB_ADDR_MASK)
/* Given an address inside a slab, find the slab's footer. */
#define FOOTER_FOR_PTR(x) ((void*)((START_FOR_PTR(x) +                  \
                                    SLAB_SIZE - sizeof(slab_footer_t))))
/* Given an address inside a slab, find the slab's bitmap start address */
#define BITMAP_FOR_PTR(x, obj_sz) ((uint8_t*)FOOTER_FOR_PTR(x) -       \
                                   BITMAP_SIZE(obj_sz))
/* Return the bitmap entry index for 'obj'. */
#define BITMAP_IDX(obj, obj_sz) (( (uintptr_t)obj -                     \
                                   START_FOR_PTR(obj) ) / obj_sz )

/** The number of objects in a slab is actually slightly less straightforward
    than might be expected.

    We can calculate the number of objects that will fit in a slab (``SLAB_SIZE
    / object_size``), and that tells us how large our bitmap needs to be.

    But now that we have a bitmap of nonzero size, fewer objects will fit in the
    remaining space. We could spend effort to calculate the maximum possible
    number of objects, or we could take the easy way out and simply not bother.
    This means that potentially we are wasting some space, but we might also
    be gaining execution time (as iterating to find the best size is time
    consuming!) { */
static unsigned num_objs_per_slab(unsigned obj_sz) {
  int n = SLAB_SIZE / obj_sz;
  int overhead = BITMAP_SIZE(obj_sz) + sizeof(slab_footer_t);
  return n - overhead / obj_sz - 1;
}

/** Now we get on to our bitmap manipulation helper functions. We have one
    to change the state of a bit (``mark``) and another to check if all
    bits in the bitmap are clear. { */
static void mark(slab_cache_t *c, void *obj, bool used) {
  unsigned idx = BITMAP_IDX(obj, c->size);

  unsigned byte = idx >> 3;
  unsigned bit = idx & 7;
  uint8_t *ptr = BITMAP_FOR_PTR(obj, c->size) + byte;
  if (used)
    *ptr |= 1 << bit;
  else
    *ptr &= ~(1 << bit);
}

static bool all_unused(slab_cache_t *c, slab_footer_t *f) {
  uint8_t *p = BITMAP_FOR_PTR(f, c->size);

  for (unsigned i = 0; i < BITMAP_SIZE(c->size); ++i, ++p) {
    if (*p != 0) {
      unsigned nobjs = num_objs_per_slab(c->size);
      /* Something is nonzero. If we're more than 8 bits
         from the end of the bitmap, then we know for
         definite that there is an unused object. */
      if ((i+1) * 8 < nobjs)
        return false;

      /* Otherwise, we need to look a bit more carefully. */
      uint8_t val = *p;
      for (unsigned j = i * 8; j < nobjs; j += 1) {
        if (val & 1) return false;
        val >>= 1;
      }
    }
  }
  return true;
}

/** The final helper function attempts to find an empty object.
    This is just searching the bitmap of each slab in turn, looking for a clear bit. { */
static int lsb_clear(uint8_t byte) {
  int i = 0;
  while ((byte & 1) == 1) {
    ++i;
    byte >>= 1;
  }
  return i;
}

static void *find_empty_obj(slab_cache_t *c) {
  slab_footer_t *f = c->first;
  while (f) {
    uint8_t *p = BITMAP_FOR_PTR(f, c->size);

    for (unsigned i = 0; i < BITMAP_SIZE(c->size); ++i, ++p) {
      /* Are all bits set? If so, there's definately nothing
         empty here. */
      if (*p != 0xFF) {
        unsigned idx = i * 8 + lsb_clear(*p);
        if (idx >= num_objs_per_slab(c->size))
          /* No free objects in this slab :( */
          break;
        return (void*)(START_FOR_PTR(f) + c->size * idx);
      }
    }

    f = f->next;
  }
  return NULL;
}

/** Then we get to the API functions. Cache creation and destruction is obvious and
    uninteresting. { */
int slab_cache_create(slab_cache_t *c, vmspace_t *vms, unsigned size, void *init) {
  c->size = size;
  c->init = init;
  c->first = NULL;
  c->vms = vms;
  spinlock_init(&c->lock);
  return 0;
}

int slab_cache_destroy(slab_cache_t *c) {
  slab_footer_t *s = c->first;
  while (s) {
    slab_footer_t *s_ = s->next;
    vmspace_free(c->vms, SLAB_SIZE, START_FOR_PTR(s), /*free_phys=*/1);
    s = s_;
  }
  c->first = NULL;
  return 0;
}

/** Ah, allocation. This is slightly more fun. { */
void *slab_cache_alloc(slab_cache_t *c) {
  spinlock_acquire(&c->lock);

  /** We need to allocate a new object. There can be two cases here - either
      all of our slabs are full, in which case we need to get a new slab,
      or there is at least one free object in at least one slab.

      Creating a new slab simply involves calling our ``vmspace`` allocator
      and adding the result to the slab list, as well as initializing
      its allocation bitmap. { */
  void *obj;
  if ((obj = find_empty_obj(c)) == NULL) {
    slab_footer_t *f = c->first;

    uintptr_t addr = vmspace_alloc(c->vms, SLAB_SIZE, /*alloc_phys=*/PAGE_WRITE);
  
    /* Initialise the used/free bitmap. */
    memset((uint8_t*)BITMAP_FOR_PTR(addr, c->size), 0, BITMAP_SIZE(c->size));

    c->first = FOOTER_FOR_PTR(addr);
    c->first->next = f;
    
    obj = (void*)START_FOR_PTR(c->first);
  }
  /** If the user specified an initial state for an object in the cache, copy it
      over now and mark the object as allocated. { */
  if (c->init)
    memcpy(obj, c->init, c->size);
  mark(c, obj, true);

  spinlock_release(&c->lock);
  return obj;
}

/** Freeing is essentially the same in reverse. We need to mark
    the object as unused in its slab's bitmap, then we see if we
    can free the slab completely.

    If all objects in the slab are now unused, we can unlink it from
    the slab list and send the memory back to the ``vmspace`` allocator. { */

void slab_cache_free(slab_cache_t *c, void *obj) {
  spinlock_acquire(&c->lock);
  assert(c->first && "Trying to free from an empty cache!");
  
  slab_footer_t *f = FOOTER_FOR_PTR(obj);

  mark(c, obj, false);

  if (all_unused(c, f)) {
    slab_footer_t *f2 = c->first;
    if (f2 == f) {
      c->first = NULL;
    } else {
      while (f2->next != f)
        f2 = f2->next;
      f2->next = f->next;
    }
    vmspace_free(c->vms, SLAB_SIZE, START_FOR_PTR(f), /*free_phys=*/1);
  }
  spinlock_release(&c->lock);
}

/** And that's all there is to a (very simple, unoptimised) slab allocator! */
