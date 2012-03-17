// RUN: %compile %s -o %t && %run %t only-run kmalloc-test 2>&1 | %FileCheck %s
// XFAIL: X64

#include "hal.h"
#include "stdio.h"
#include "kmalloc.h"
#include "x86/io.h"
int f () {
  // CHECK: kmalloc(0x10): 0xfefd400{{4|8}}
  // CHECK: kmalloc(0x10): 0xfefd402{{4|8}}
  // CHECK: kmalloc(0x10): 0xfefd404{{4|8}}
  // CHECK: kmalloc(0x10): 0xfefd406{{4|8}}
  kprintf("kmalloc(0x10): %p\n", kmalloc(0x10));
  kprintf("kmalloc(0x10): %p\n", kmalloc(0x10));
  kprintf("kmalloc(0x10): %p\n", kmalloc(0x10));
  kprintf("kmalloc(0x10): %p\n", kmalloc(0x10));

  // CHECK: kmalloc(0x8): 0xfefd000{{4|8}}
  // CHECK: kmalloc(0x8): 0xfefd001{{4|8}}
  // CHECK: kmalloc(0x8): 0xfefd002{{4|8}}
  kprintf("kmalloc(0x8): %p\n", kmalloc(0x8));
  kprintf("kmalloc(0x8): %p\n", kmalloc(0x8));
  kprintf("kmalloc(0x8): %p\n", kmalloc(0x8));

  kfree((void*)0xfefd0010 + sizeof(uintptr_t));
  // CHECK: kmalloc(0x8): 0xfefd001{{4|8}}
  kprintf("kmalloc(0x8): %p\n", kmalloc(0x8));

  // CHECK: kmalloc(0x400): 0xfefd600{{4|8}}
  // CHECK: kmalloc(0x400): 0xfefd200{{4|8}}
  kprintf("kmalloc(0x400): %p\n", kmalloc(0x400));
  kprintf("kmalloc(0x400): %p\n", kmalloc(0x400));

  kfree((void*)0xfefd6004);

  kprintf("ismapped: %d\n", is_mapped(0xfefd6000));

  kprintf("kmalloc(0x400): %p\n", kmalloc(0x400));

  return 0;
}

static const char *p[] = {"console", "x86/serial", "kmalloc", NULL};
static init_fini_fn_t run_on_startup x = {
  .name = "kmalloc-test",
  .prerequisites = p,
  .fn = &f
};
