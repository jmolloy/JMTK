#include "hal.h"
#include "x86/hal.h"
#include "string.h"

bool cow_handle_page_fault(uintptr_t cr2, uintptr_t error_code) {
  unsigned flags;

  uint32_t p = (uint32_t)get_mapping(cr2, &flags);

  if ((error_code & (X86_PRESENT|X86_WRITE)) &&
      p != ~0UL && (flags & PAGE_COW) ) {
    /* Page was marked copy-on-write. */
    uint32_t p2 = (uint32_t)alloc_page(PAGE_REQ_UNDER4GB);

    /* We have to copy the page. In order to avoid a costly and 
       non-reentrant map/unmap pair to temporarily have them
       both mapped into memory, copy first into a buffer on
       the stack (this means the stack must be >4KB). */
    uint8_t buffer[4096];

    uint32_t v = cr2 & 0xFFFFF000;
    memcpy(buffer, (uint8_t*)v, 0x1000);
    
    if (unmap(v, 1) == -1)
      panic("unmap() failed during copy-on-write!");

    unsigned f = ((flags & X86_USER) ? PAGE_USER : 0) |
      ((flags & X86_EXECUTE) ? PAGE_EXECUTE : 0) |
      PAGE_WRITE;
    if (map(v, p2, 1, f) == -1)
      panic("map() failed during copy-on-write!");

    memcpy((uint8_t*)v, buffer, 0x1000);

    /* Mark the old page as having one less reference. */
    cow_refcnt_dec(p);

    return true;
  }
  return false;
}
