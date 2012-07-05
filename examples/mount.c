#include "hal.h"
#include "vfs.h"
#include "stdio.h"

int f() {
  /* mount("/dev/hda1", "/") */
  mount(makedev(DEV_MAJ_HDA, 0), get_root(), NULL);

  return 0;
}

static prereq_t r[] = { {"vfs",NULL} };
static prereq_t la[] = { {"x86/ide",NULL}, {"partition",NULL}, {"fs_vfat",NULL} };
static module_t x run_on_startup = {
  .name = "mount-example",
  .required = r,
  .load_after = la,
  .init = &f,
  .fini = NULL
};
