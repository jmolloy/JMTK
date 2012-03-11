#ifndef MMAP_H
#define MMAP_H

#if defined(X64)
#include "x64/mmap.h"
#elif defined(X86)
#include "x86/mmap.h"
#elif defined(HOSTED)
#include "hosted/mmap.h"
#endif

#endif
