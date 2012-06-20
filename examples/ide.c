#include "hal.h"
#include "thread.h"
#include "stdio.h"
#include "vmspace.h"
#include "assert.h"

int isalpha(char c) {
  return c >= ' ' && c <= '~';
}

void hexdump(uint8_t *buf, unsigned size, uintptr_t address) {
  unsigned i = 0, j = 0;
  const unsigned STRIDE = 16;

  /* Loop over lines... */
  while (i < size) {
    
    kprintf("%08x: ", address);

    for (j = 0; j < STRIDE; ++j) {
      if (i + j < size)
        kprintf("%02x ", buf[i+j]);
      else
        kprintf("   ");
    }

    for (j = 0; j < STRIDE; ++j) {
      if (i + j < size) {
        if (isalpha(buf[i+j]))
          kprintf("%c", buf[i+j]);
        else
          kprintf(".");
      } else {
        kprintf(" ");
      }
    }

    kprintf("\n");

    address += STRIDE;
    i += STRIDE;
  }
}

int f() {
  enable_interrupts();

  /* Get /dev/hda */
  block_device_t *hd = get_block_device(makedev(DEV_MAJ_HDA, 0));
  assert(hd);

  /* Get some buffer space. */
  uintptr_t buf = vmspace_alloc(&kernel_vmspace, 0x1000, 1);

  assert(hd->read);
  int ret = hd->read(hd, 0, (uint8_t*)buf, 0x1000);

  kprintf("Ret: %d\n", ret);

  hexdump((void*)buf, 0x100, 0);

  return 0;
}

static prereq_t p[] = { {"x86/ide",NULL}, {NULL,NULL} };
static prereq_t p2[] = { {"x86/screen",NULL}, {"x86/keyboard",NULL},
                         {"x86/serial",NULL}, {NULL,NULL} };
                         

static module_t x run_on_startup = {
  .name = "ide-example",
  .required = p,
  .load_after = p2,
  .init = &f,
  .fini = NULL
};
