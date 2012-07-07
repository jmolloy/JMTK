#include "hal.h"
#include "vfs.h"
#include "stdio.h"

void emit_indent(int indent) {
  for (int i = 0; i < indent; ++i)
    kprintf(" ");
}

void emit_tree(const char *name, inode_t *ino, int indent, vector_t *done) {
  for (unsigned i = 0; i < vector_length(done); ++i)
    if (ino == *(inode_t**)vector_get(done, i))
      return;
  
  vector_add(done, &ino);

  emit_indent(indent);
  kprintf("'%s' %s (size %d)\n", name,
          ((ino->type == it_dir) ? "DIR" : ""),
          ino->size);

  if (ino->type == it_dir) {
    vector_t files = vfs_readdir(ino);
    for (unsigned i = 0; i < vector_length(&files); ++i) {
      dirent_t *dent = vector_get(&files, i);
      emit_tree(dent->name, dent->ino, indent + 2, done);
    }
  }
}

int f() {
  /* mount("/dev/hda1", "/") */
  vfs_mount(makedev(DEV_MAJ_HDA, 0), vfs_get_root(), NULL);

  vector_t done = vector_new(sizeof(inode_t*), 16);

  emit_tree("", vfs_get_root(), 0, &done);
  /* vector_t dirs = vfs_readdir(vfs_get_root()); */
  /* kprintf("len(dirs) = %d\n", vector_length(&dirs)); */

  /* inode_t *a = *(inode_t**)vector_get(&dirs, 0); */
  /* kprintf("a name %s type %d\n", a->name, a->type); */
  /* vector_t adirs = vfs_readdir(a); */
  /* kprintf("len(adirs) = %d\n", vector_length(&adirs)); */

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
