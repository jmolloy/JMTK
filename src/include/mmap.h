#ifndef MMAP_H
#define MMAP_H

/** @file mmap.h Memory map definition
 
    This file is a stepping stone to platform specific memory map information. The
    implementation headers in {PLATFORM}/mmap.h must define at least two macros:

    #define MMAP_KERNEL_START <start address of kernel virtual memory>
    #define MMAP_COW_REFCNTS  <area of virtual memory at least 64MB large> */

#if defined(X64)
#include "x64/mmap.h"
#elif defined(X86)
#include "x86/mmap.h"
#elif defined(HOSTED)
#include "hosted/mmap.h"
#endif

#endif
