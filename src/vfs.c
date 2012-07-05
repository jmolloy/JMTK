#include "string.h"
#include "vfs.h"
#include "stdio.h"

#ifdef DEBUG_vfs
# define dbg(args...) kprintf("vfs: " args)
#else
# define dbg(args...)
#endif

static vector_t filesystems, mountpoints;
static inode_t root;

typedef struct fs_info {
  const char *ident;
  int (*probe)(dev_t, filesystem_t*);
} fs_info_t;

int register_filesystem(const char *ident,
                        int (*probe)(dev_t dev, filesystem_t *fs)) {
  fs_info_t fsi;
  fsi.ident = ident;
  fsi.probe = probe;

  vector_add(&filesystems, &fsi);
  return 0;
}

int unregister_filesystem(const char *ident) {
  for (unsigned i = 0; i < vector_length(&filesystems); ++i) {
    fs_info_t *fsi = vector_get(&filesystems, i);
    if (!strcmp(fsi->ident, ident)) {
      vector_erase(&filesystems, i);
      return 0;
    }
  }
  return 1;
}

inode_t *get_root() {
  return &root;
}

int mount(dev_t dev, inode_t *node, const char *fs) {
  dbg("mount() dev %x fs %s\n", dev, fs);
  mountpoint_t mp;
  for (unsigned i = 0; i < vector_length(&filesystems); ++i) {
    fs_info_t *fsi = vector_get(&filesystems, i);

    if (fs == NULL || !strcmp(fs, fsi->ident)) {
      dbg("considering FS '%s'\n", fsi->ident);
      if (fsi->probe(dev, &mp.fs) == 0) {
        mp.dev = dev;
        mp.node = node;

        vector_add(&mountpoints, &mp);
        node->mountpoint = vector_get(&mountpoints,
                                      vector_length(&mountpoints) - 1);

        mp.fs.get_root(&node->mountpoint->fs, node);
        mp.fs.readdir(&node->mountpoint->fs, node);
        dbg("mount() succeeded\n");
        return 0;
      }

      if (fs != NULL) {
        dbg("mount() failed\n");
        return 1;
      }
    }
  }
  dbg("mount() failed\n");
  return 1;
}

static int vfs_init() {
  filesystems = vector_new(sizeof(fs_info_t), 4);
  mountpoints = vector_new(sizeof(mountpoint_t), 4);

  return 0;
}

static prereq_t req[] = { {"kmalloc",NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "vfs",
  .required = req,
  .load_after = NULL,
  .init = &vfs_init,
  .fini = NULL
};
