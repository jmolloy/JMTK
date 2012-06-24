#if 0
exit `$1 $2 | ./test/FileCheck $0`
#endif

#include "hal.h"
#include "stdio.h"
#include "kmalloc.h"
#include "x86/io.h"

int f () {
  // CHECK: kmalloc(0x10): 0xfefe000{{4|8}}
  // CHECK: kmalloc(0x10): 0xfefe002{{4|8}}
  // CHECK: kmalloc(0x10): 0xfefe004{{4|8}}
  // CHECK: kmalloc(0x10): 0xfefe006{{4|8}}
  kprintf("kmalloc(0x10): %p\n", kmalloc(0x10));
  kprintf("kmalloc(0x10): %p\n", kmalloc(0x10));
  kprintf("kmalloc(0x10): %p\n", kmalloc(0x10));
  kprintf("kmalloc(0x10): %p\n", kmalloc(0x10));

  // CHECK: kmalloc(0x8): 0xfefe200{{4|8}}
  // CHECK: kmalloc(0x8): 0xfefe201{{4|8}}
  // CHECK: kmalloc(0x8): 0xfefe202{{4|8}}
  kprintf("kmalloc(0x8): %p\n", kmalloc(0x8));
  kprintf("kmalloc(0x8): %p\n", kmalloc(0x8));
  kprintf("kmalloc(0x8): %p\n", kmalloc(0x8));

  kfree((void*)0xfefe2010 + sizeof(uintptr_t));
  // CHECK: kmalloc(0x8): 0xfefe201{{4|8}}
  kprintf("kmalloc(0x8): %p\n", kmalloc(0x8));

  // CHECK: kmalloc(0x400): 0xfefc000{{4|8}}
  // CHECK: kmalloc(0x400): 0xfefc100{{4|8}}
  kprintf("kmalloc(0x400): %p\n", kmalloc(0x400));
  kprintf("kmalloc(0x400): %p\n", kmalloc(0x400));

  kfree((void*)0xfefc0004);

  return 0;
}

static prereq_t p[] = { {"console",NULL}, {"x86/serial",NULL}, {NULL,NULL} };
static prereq_t p2[] = { {"kmalloc",NULL}, {NULL,NULL} };

static module_t run_on_startup x = {
  .name = "kmalloc-test",
  .load_after = p,
  .required = p2,
  .init = &f,
  .fini = NULL
};
module_t *test_module = &x;
