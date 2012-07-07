#include "hal.h"
#include "assert.h"
#include "adt/vector.h"
#include "string.h"
#include "kmalloc.h"

vector_t vector_new(unsigned itemsz, unsigned nitems) {
  vector_t v;
  v.itemsz = itemsz;
  v.nitems = 0;
  v.data = 0;
  v.sz = 0;

  vector_reserve(&v, nitems);
  return v;
}

vector_t vector_clone(vector_t other) {
  vector_t v;
  v.itemsz = other.itemsz;
  v.nitems = other.nitems;
  v.sz = other.sz;
  v.data = kmalloc(v.sz * v.itemsz);
  memcpy(v.data, other.data, v.sz * v.itemsz);

  return v;
}

void vector_drop(vector_t *vec) {
  vec->data = 0;
  vec->sz = 0;
  vec->nitems = 0;
}

void vector_destroy(vector_t *vec) {
  if (vec->data)
    kfree(vec->data);
  vec->data = 0;
  vec->sz = 0;
  vec->nitems = 0;
}

void *vector_get(vector_t *vec, unsigned n) {
  assert(n < vec->nitems && "Indexed off the end of a vector!");

  uint8_t *bytes = (uint8_t*)vec->data;
  return (void*) (&bytes[n * vec->itemsz]);
}

unsigned vector_length(vector_t *vec) {
  return vec->nitems;
}

void vector_add(vector_t *vec, void *item) {
  vector_add_multiple(vec, item, 1);
}

void vector_add_multiple(vector_t *vec, void *items, unsigned nitems) {
  vector_reserve(vec, vec->nitems + nitems);

  uint8_t *bytes = (uint8_t*)vec->data;
  memcpy(&bytes[vec->nitems * vec->itemsz], items, vec->itemsz * nitems);

  vec->nitems += nitems;
}

void vector_reserve(vector_t *vec, unsigned nitems) {
  if (vec->sz >= nitems) return;

  void *newdata = kmalloc(nitems * vec->itemsz);
  if (vec->data) {
    memcpy(newdata, vec->data, vec->sz * vec->itemsz);
  }
  vec->data = newdata;
  vec->sz = nitems;
}

void *vector_get_data(vector_t *vec) {
  return vec->data;
}

void vector_erase(vector_t *vec, unsigned i) {
  assert(i < vec->nitems);

  uint8_t *bytes = (uint8_t*) vec->data;
  memmove(&bytes[i * vec->itemsz],
          &bytes[(i+1) * vec->itemsz],
          (vec->sz - i) * vec->itemsz);
  -- vec->nitems;
}
