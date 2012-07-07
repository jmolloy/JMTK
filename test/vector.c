#if 0
exit `$1 $2 | ./test/FileCheck $0`
#endif

#include "hal.h"
#include "stdio.h"
#include "adt/vector.h"

static void dump_vector(vector_t v) {
  kprintf("data %x itemsz %d nitems %d sz %d\n",
          v.data, v.itemsz, v.nitems, v.sz);
}

static int test() {
  vector_t v = vector_new(4, 2);
  // CHECK: data [[vdata:[^0][0-9a-f]+]] itemsz 4 nitems 0 sz 2
  dump_vector(v);

  vector_t v2 = vector_clone(v);
  // CHECK-NOT: data [[vdata]]
  dump_vector(v2);
  // CHECK: itemsz 4 nitems 0 sz 2
  dump_vector(v2);

  vector_drop(&v);
  // CHECK: data {{0+}} itemsz 4
  dump_vector(v);

  vector_destroy(&v2);
  // CHECK: data {{0+}} itemsz 4
  dump_vector(v2);

  v = vector_new(4, 2);
  // CHECK: len = 0
  kprintf("len = %d\n", vector_length(&v));

  // CHECK: len = 1 item = 1234
  uint32_t x = 1234, y = 2345, z = 3456;
  vector_add(&v, &x);
  kprintf("len = %d item = %d\n", vector_length(&v), *(uint32_t*)vector_get(&v, 0));

  vector_add(&v, &y);
  vector_add(&v, &z);

  // CHECK: len = 3 item[0] = 1234, item[1] = 2345, item[2] = 3456
  kprintf("len = %d item[0] = %d, item[1] = %d, item[2] = %d\n",
          vector_length(&v), *(uint32_t*)vector_get(&v, 0), *(uint32_t*)vector_get(&v, 1), *(uint32_t*)vector_get(&v, 2));

  // CHECK: sz 6
  vector_reserve(&v, 6);
  dump_vector(v);

  // CHECK: len = 2 item[0] = 1234, item[1] = 3456
  vector_erase(&v, 1);
  kprintf("len = %d item[0] = %d, item[1] = %d\n",
          vector_length(&v), *(uint32_t*)vector_get(&v, 0), *(uint32_t*)vector_get(&v, 1));


  return 0;
}

static prereq_t r[] = { {"kmalloc",NULL}, {NULL,NULL} };
static prereq_t p[] = { {"hosted/free_memory",NULL},
                        {"x86/free_memory",NULL},
                        {"hosted/console", NULL}, {"x86/screen",NULL},
                        {"x86/serial",NULL}, {"console",NULL}, {NULL,NULL} };
static module_t run_on_startup x = {
  .name = "vector-test",
  .required = r,
  .load_after = p,
  .init = &test,
  .fini = NULL
};
module_t *test_module = &x;
