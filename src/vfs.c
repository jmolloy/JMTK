#include "assert.h"
#include "directory_cache.h"
#include "errno.h"
#include "kmalloc.h"
#include "stdio.h"
#include "string.h"
#include "vfs.h"

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

inode_t *vfs_get_root() {
  return &root;
}

int vfs_mount(dev_t dev, inode_t *node, const char *fs) {
  assert(node->type == it_dir && "mount() called on non-directory inode!");

  dbg("mount() dev %x fs %s\n", dev, fs);
  mountpoint_t *mp = kmalloc(sizeof(mountpoint_t));
  for (unsigned i = 0; i < vector_length(&filesystems); ++i) {
    fs_info_t *fsi = vector_get(&filesystems, i);

    if (fs == NULL || !strcmp(fs, fsi->ident)) {
      dbg("considering FS '%s'\n", fsi->ident);
      if (fsi->probe(dev, &mp->fs) == 0) {
        mp->dev = dev;
        mp->node = node;

        node->mountpoint = mp;
        vector_add(&mountpoints, &mp);

        mp->fs.get_root(&node->mountpoint->fs, node);
        node->u.dir_cache = NULL;

        dbg("mount() succeeded\n");
        return 0;
      }

      if (fs != NULL) {
        dbg("mount() failed\n");
        kfree(mp);
        return 1;
      }
    }
  }
  dbg("mount() failed\n");
  kfree(mp);
  return 1;
}

vector_t vfs_readdir(inode_t *node) {
  dbg("readdir('%s')\n", node->name);
  assert(node->type == it_dir && "readdir() called on non-directory inode!");

  /* Generate the directory cache if needs be. */
  if (!node->u.dir_cache) {
    dbg("... generating directory cache ...\n");
    vector_t v = node->mountpoint->fs.readdir(&node->mountpoint->fs, node);
    
    for (unsigned i = 0; i < vector_length(&v); ++i) {
      dirent_t *dent = vector_get(&v, i);
      inode_t *ino = dent->ino;

      ino->mountpoint = node->mountpoint;
      ino->parent = node;
      ino->write_buffer = NULL;
      ino->handles = 0;
    }

    node->u.dir_cache = directory_cache_new(v);
    assert(node->u.dir_cache);
  }

  return directory_cache_get_all(node->u.dir_cache);
}

/* Attempt to traverse from 'parent' to its child in 'path'. */
static inode_t *traverse(inode_t *parent, const char *path, access_fn_t access) {
  if (parent->type != it_dir) {
    set_errno(ENOENT);
    return NULL;
  }

  /* Check the directory has search access. If the LSB is set (i.e. a+x),
     don't bother calling the access function. */
  if ((parent->mode & 1) == 0 || access(parent->mode) == false) {
    set_errno(EACCES);
    return NULL;
  }

  /* Generate the directory cache if needs be. */
  if (!parent->u.dir_cache) {
    vector_t v = parent->mountpoint->fs.readdir(&parent->mountpoint->fs, parent);
    parent->u.dir_cache = directory_cache_new(v);
    assert(parent->u.dir_cache);
  }

  inode_t *child = directory_cache_get(parent->u.dir_cache, path);
  if (!child) {
    set_errno(ENOENT);
    return NULL;
  }

  return child;
}

inode_t *vfs_open(const char *path, access_fn_t access) {
  char buf[512];
  unsigned nbuf = 0;
  inode_t *inode = &root;

  while (*path) {
    assert(*path == '/');
    ++path;

    nbuf = 0;
    while (*path && *path != '/')
      buf[nbuf++] = *path++;
    buf[nbuf] = '\0';

    inode = traverse(inode, buf, access);
  }
  return inode;
}

int64_t vfs_read(inode_t *node, uint64_t offset, void *buf, uint64_t sz) {
  dbg("read('%s')\n", node->name);
  assert(node->type != it_dir && "read() called on a directory!");
  return node->mountpoint->fs.read(&node->mountpoint->fs, node, offset, buf, sz);
}

int64_t vfs_write(inode_t *node, uint64_t offset, void *buf, uint64_t sz) {
  dbg("write('%s')\n", node->name);
  assert(node->type != it_dir && "write() called on a directory!");
  return node->mountpoint->fs.write(&node->mountpoint->fs, node, offset, buf, sz);
}

void vfs_close(inode_t *node) {
  assert(node->handles > 0 && "close() called on inode with no handles!");
  --node->handles;
}

static int vfs_init() {
  filesystems = vector_new(sizeof(fs_info_t), 4);
  mountpoints = vector_new(sizeof(mountpoint_t*), 4);

  root.mountpoint = NULL;
  root.type = it_dir;
  root.parent = NULL;
  root.mode = 0777;
  root.nlink = 0;
  root.uid = 0;
  root.gid = 0;
  root.size = -1;
  root.atime = root.mtime = root.ctime = 0;
  root.write_buffer = NULL;
  root.handles = 0;
  root.u.dir_cache = NULL;
  root.data = NULL;

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
