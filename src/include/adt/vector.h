#ifndef VECTOR_H
#define VECTOR_H

typedef struct vector {
  void *data;
  unsigned itemsz;
  unsigned nitems;
  unsigned sz;
} vector_t;

vector_t vector_new(unsigned itemsz, unsigned nitems);
vector_t vector_clone(vector_t other);
void vector_drop(vector_t *vec);
void vector_destroy(vector_t *vec);

void *vector_get(vector_t *vec, unsigned n);
unsigned vector_length(vector_t *vec);

void vector_add(vector_t *vec, void *item);
void vector_add_multiple(vector_t *vec, void *items, unsigned nitems);

void vector_reserve(vector_t *vec, unsigned nitems);

void *vector_get_data(vector_t *vec);

void vector_erase(vector_t *vec, unsigned i);

#endif
