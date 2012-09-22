#ifndef X86_MMAP_H
#define X86_MMAP_H

#define MMAP_KERNEL_START 0xC0000000

#define MMAP_COW_REFCNTS  0xCC000000 /* At least 64MB of address space for 36-bit
                                        physical addresses */
#define MMAP_KERNEL_VMSPACE_START \
                          0xD0000000
#define MMAP_KERNEL_VMSPACE_END \
                          0xFEFF0000

#define MMAP_KERNEL_TMP1  0xFEFFE000
#define MMAP_KERNEL_TMP2  0xFEFFF000
#define MMAP_PMM_BITMAP   0xFF000000
#define MMAP_PMM_BITMAP_END 0xFFB00000

#define MMAP_PAGE_DIR     0xFFBFF000
#define MMAP_PAGE_TABLES  0xFFC00000

#define IS_KERNEL_ADDR(x) ((void*)(x) >= (void*)MMAP_KERNEL_START)

#endif
