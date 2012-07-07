#if 0
exit `$1 $2 --hda test/inputs/partition-test.img | ./test/FileCheck $0`
#endif

#include "hal.h"

// CHECK: Partition 0 @ 0x1 size 0MB type 131
// CHECK: Partition 2 @ 0x12 size 0MB type 131
// CHECK: Partition 4 @ 0x24 size 0MB type 131
// CHECK: Partition 5 @ 0x2a size 0MB type 131

static prereq_t p[] = { {"partition",NULL}, {NULL,NULL} };
static prereq_t la[] = { {"x86/ide",NULL}, {NULL,NULL} };
  
static module_t run_on_startup x = {
  .name = "partition-test",
  .required = p,
  .load_after = la,
  .init = NULL,
  .fini = NULL
};
module_t *test_module = &x;
