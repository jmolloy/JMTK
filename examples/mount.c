#include "hal.h"
#include "vfs.h"
#include "stdio.h"

int f() {
  /* mount("/dev/hda1", "/") */
  vfs_mount(makedev(DEV_MAJ_HDA, 0), vfs_get_root(), NULL);

  vector_t dirs = vfs_readdir(vfs_get_root());
  kprintf("len(dirs) = %d\n", vector_length(&dirs));

  inode_t *a = vector_get(&dirs, 0);
  kprintf("a name %s type %d\n", a->name, a->type);
  vector_t adirs = vfs_readdir(a);
  kprintf("len(adirs) = %d\n", vector_length(&adirs));

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
