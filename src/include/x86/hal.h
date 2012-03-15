#ifndef X86_HAL_H
#define X86_HAL_H

typedef struct address_space {
  uint32_t *directory;
  /* FIXME: Put a lock here. */
} address_space_t;

static inline void abort() {
  for(;;);
}

#include "x86/regs.h"

#define abort() (void)0

#endif
