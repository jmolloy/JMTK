#if 0
exit `$1 | ./test/FileCheck $0`
#endif

#include "hal.h"
#include "string.h"
#include <stdio.h>

static int written = 0;
static int _read = 0;
static int opened = 0;
static int closed = 0;
static int flushed = 0;
static int open(console_t *obj) {opened++; return 0;}
static int close(console_t *obj) {closed++; return 0;}
static int read(console_t *obj, char *buf, int len) {_read++; return 1;}
static int read2(console_t *obj, char *buf, int len) {return 1;}
static int write(console_t *obj, const char *buf, int len) {written++; return 0;}
static void flush(console_t *obj) {flushed++;}

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
  printf("reg c1: %d\n", register_console(&c1));
  // CHECK: reg c2: 0
  printf("reg c2: %d\n", register_console(&c2));
  write_console(NULL, 0);
  // CHECK: read ret: 1
  char tmp;
  printf("read ret: %d\n", read_console(&tmp, 1));
  unregister_console(&c1);
  return 0;
}
static int test_end() {
  // CHECK: opened 2 closed 2 read 1 written {{.*}} flushed 2
  printf("opened %d closed %d read %d written %d flushed %d\n",
         opened, closed, _read, written, flushed);
  return 0;
}

static prereq_t p[] = { {"console",NULL}, {NULL,NULL} };

static module_t run_on_startup x = {
  .name = "console-test",
  .required = p,
  .load_after = NULL,
  .init = &test,
  .fini = &test_end
};
module_t *test_module = &x;
