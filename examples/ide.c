#include "hal.h"
#include "thread.h"
#include "stdio.h"
#include "vmspace.h"
#include "assert.h"

int f() {
  enable_interrupts();

  /* Get /dev/hda */
  block_device_t *hd = get_block_device(makedev(DEV_MAJ_HDA, 0));
  assert(hd);

  /* Get some buffer space. */
  uintptr_t buf = vmspace_alloc(&kernel_vmspace, 0x1000, 1);

  assert(hd->read);
  int ret = hd->read(hd, 0, (void*)buf, 0x1000);

  kprintf("Ret: %d\n", ret);

  return 0;
}

static const char *p[] = {"x86/screen", "x86/keyboard", "x86/serial",
                          "x86/ide", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "ide-example",
  .prerequisites = p,
  .fn = &f
};
