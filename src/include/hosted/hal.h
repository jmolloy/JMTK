#ifndef HOSTED_HAL_H
#define HOSTED_HAL_H

typedef struct address_space {
  uint32_t a[1<<20];
} address_space_t;

#endif
