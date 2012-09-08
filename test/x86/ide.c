#if 0
exit `HDD_IMAGE=test/inputs/ide-test.img $1 $2 | ./test/FileCheck $0`
#endif

#include "assert.h"
#include "hal.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "vmspace.h"

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
  assert(get_interrupt_state() != 0);
  /* Get /dev/hda */
  block_device_t *hd = get_block_device(makedev(DEV_MAJ_HDA, 0));
  assert(hd);

  /* Get some buffer space. */
  uintptr_t buf = vmspace_alloc(&kernel_vmspace, 0x1000, 1);
  assert(get_interrupt_state() != 0);
  assert(hd->read);
  hd->read(hd, 0, (uint8_t*)buf, 0x1000);
  assert(get_interrupt_state() != 0);
  // CHECK: 00000000: e1 76 31 19 c3 4e d7 82 03 b6 36 43 22 f4 78 cb
  hexdump((void*)buf, 0x10, 0);
  // CHECK: 000001f0: fa f7 e0 a1 d5 8c d4 c3 27 13 61 ca de 23 a0 ef
  hexdump((void*)buf + 512 - 0x10, 0x10, 512-0x10);
  assert(get_interrupt_state() != 0);  
  hd->read(hd, 4*512, (uint8_t*)buf, 0x1000);
  // CHECK: 00000800: cc 0d 31 f0 36 20 ce 29 3c b5 28 01 92 ee 4b 82
  hexdump((void*)buf, 0x10, 4*512 + 0);
  // CHECK: 000009f0: f9 e3 c7 7a 36 51 96 f6 5e 4f 97 96 b1 ba 23 b2
  hexdump((void*)buf + 512 - 0x10, 0x10, 4*512 + 512-0x10);

  return 0;
}

static prereq_t p[] = { {"console",NULL}, {"x86/ide",NULL},
                        {"interrupts",NULL}, {NULL,NULL} };

static module_t run_on_startup x = {
  .name = "ide-test",
  .required = p,
  .init = &f,
  .fini = NULL
};
module_t *test_module = &x;
