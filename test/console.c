// RUN: %compile %s -DTEST1 -o %t && %t | %FileCheck %s
// RUN: %compile %s -DTEST2 -o %t && %t | %FileCheck %s --check-prefix=T2
// XFAIL: X86
// XFAIL: X64
// XFAIL: ARM

#if !defined(HOSTED)
# error This test must be run on a hosted kernel!
#endif
#include "hal.h"
#include "string.h"
#include <stdio.h>

#ifdef TEST1
int written = 0;
int _read = 0;
int opened = 0;
int closed = 0;
int flushed = 0;
int open(console_t *obj) {opened++; return 0;}
int close(console_t *obj) {closed++; return 0;}
int read(console_t *obj, const char *buf, int len) {_read++; return 0;}
int read2(console_t *obj, const char *buf, int len) {return 0;}
int write(console_t *obj, const char *buf, int len) {written++; return 0;}
void flush(console_t *obj) {flushed++;}

console_t c1 = {
  .open = &open,
  .close = &close,
  .read = &read2,
  .write = &write,
  .flush = &flush,
  .data = (void*)12345
};
console_t c2 = {
  .open = &open,
  .close = &close,
  .read = &read,
  .write = &write,
  .flush = &flush,
  .data = (void*)23456
};

static int test() {
  // CHECK: reg c1: 0
  printf("reg c1: %d\n", register_console(&c1, 0));
  // CHECK: reg c2: 0
  printf("reg c2: %d\n", register_console(&c2, 1));
  write_console(NULL, 0);
  // CHECK: read ret: 0
  printf("read ret: %d\n", read_console(NULL, 0));
  unregister_console(&c1);
  return 0;
}
static int test_end() {
  // CHECK: opened 2 closed 2 read 1 written 2 flushed 2
  printf("opened %d closed %d read %d written %d flushed %d\n",
         opened, closed, _read, written, flushed);
  return 0;
}

static const char *p[] = {"console", NULL};

static init_fini_fn_t run_on_startup x = {
  .name = "console-test",
  .prerequisites = NULL,
  .fn = &test
};
static init_fini_fn_t run_on_shutdown y = {
  .name = "console-test-end",
  .prerequisites = p,
  .fn = &test_end
};
#endif

#ifdef TEST2
console_t c2 = {
  .open = NULL,
  .close = NULL,
  .read = NULL,
  .write = NULL,
  .flush = NULL,
  .data = NULL
};

static int test() {
  // CHECK-T2: reg c: 0
  printf("reg c: %d\n", register_console(&c2, 0));
  return 0;
}
static int test_end() {
  unregister_console(&c2);
  
  // CHECK-T2: ok?
  printf("ok?\n");
  return 0;
}

static const char *p[] = {"console", NULL};

static init_fini_fn_t run_on_startup x = {
  .name = "console-test",
  .prerequisites = NULL,
  .fn = &test
};
static init_fini_fn_t run_on_shutdown y = {
  .name = "console-test-end",
  .prerequisites = p,
  .fn = &test_end
};

#endif
