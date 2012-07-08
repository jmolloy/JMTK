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

#define MAX_SYMLINKS_TO_FOLLOW 10

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

  dbg("mount: mount dev %x fs %s\n", dev, fs);

  /* Is the device or inode already mounted? */
  for (unsigned i = 0; i < vector_length(&mountpoints); ++i) {
    mountpoint_t *mp = *(mountpoint_t**)vector_get(&mountpoints, i);
    if (mp->dev == dev || mp->node == node) {
      dbg("mount: device or inode already mounted!\n");
      set_errno(EBUSY);
      return 1;
    }
  }

  mountpoint_t *mp = kmalloc(sizeof(mountpoint_t));
  for (unsigned i = 0; i < vector_length(&filesystems); ++i) {
    fs_info_t *fsi = vector_get(&filesystems, i);

    if (fs == NULL || !strcmp(fs, fsi->ident)) {
      dbg("considering FS '%s'\n", fsi->ident);
      if (fsi->probe(dev, &mp->fs) == 0) {
        mp->dev = dev;
        mp->node = node;
        /* Back up the inode data to be restored on umount. */
        memcpy(&mp->orig_inode_data, node, sizeof(inode_t));

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

int vfs_umount(dev_t dev, inode_t *node) {
 dbg("umount: umount dev %x node %x\n", dev, node);

  /* Is the device or inode already mounted? */
  for (unsigned i = 0; i < vector_length(&mountpoints); ++i) {
    mountpoint_t *mp = *(mountpoint_t**)vector_get(&mountpoints, i);
    if (mp->dev == dev || mp->node == node) {
      dbg("umount: unmounting device %x from node %x\n",
          mp->dev, mp->node);
      /* FIXME: Check if the FS is busy. Keep a refcount in mountpoint_t? */

      /* Restore the inode as it was before it was mounted. */
      memcpy(mp->node, &mp->orig_inode_data, sizeof(inode_t));
      return 0;
    }
  }

  dbg("umount: target was not a mountpoint!\n");
  set_errno(EINVAL);
  return 1;
}

vector_t vfs_readdir(inode_t *node) {
  dbg("readdir(%x)\n", node);
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
static inode_t *traverse_node(inode_t *parent,
                              const char *path, access_fn_t access) {
  dbg("_traverse: parent %x path '%s'\n", parent, path);
  if (parent->type != it_dir) {
    dbg("_traverse: parent was not a directory!\n");
    set_errno(ENOENT);
    return NULL;
  }

  /* Check for zero-length string - just return the parent. */
  if (path[0] == '\0')
    return parent;

  /* Check the directory has search access. If the LSB is set (i.e. a+x),
     don't bother calling the access function. */
  if (! ((parent->mode & 1) == 1 || access(parent->mode)) ) {
    dbg("_traverse: search access denied!\n");
    set_errno(EACCES);
    return NULL;
  }

  /* Generate the directory cache if needed. */
  if (!parent->u.dir_cache) {
    vector_t v = parent->mountpoint->fs.readdir(&parent->mountpoint->fs, parent);
    parent->u.dir_cache = directory_cache_new(v);
    assert(parent->u.dir_cache);
  }

  inode_t *child = directory_cache_get(parent->u.dir_cache, path);
  if (!child) {
    dbg("_traverse: no such file or directory!\n");
    set_errno(ENOENT);
    return NULL;
  }

  return child;
}

static inode_t *traverse_path(inode_t *inode,
                              const char *path, access_fn_t access) {
  char buf[512];
  unsigned nbuf = 0;

  while (*path && inode) {
    if (*path == '/')
      ++path;

    /* Handle symlinks */
    unsigned nloop = 0;
    while (inode->type == it_symlink) {
      if (++nloop >= MAX_SYMLINKS_TO_FOLLOW) {
        set_errno(ELOOP);
        return NULL;
      }

      nbuf = vfs_read(inode, 0, buf, 511);
      assert(nbuf > 0 && "Symlink read failed!");
      buf[nbuf] = '\0';

      inode = traverse_path( (buf[0] == '/') ? &root : inode->parent,
                             buf, access );
      if (!inode) return NULL;
    }

    nbuf = 0;
    while (*path && *path != '/')
      buf[nbuf++] = *path++;
    buf[nbuf] = '\0';

    inode = traverse_node(inode, buf, access);
  }
  return inode;
}

inode_t *vfs_lopen(const char *path, access_fn_t access) {
  inode_t *inode = &root;

  dbg("lopen: '%s'\n", path);

  inode = traverse_path(inode, path, access);

  if (inode)
    ++inode->handles;
  return inode;
}

inode_t *vfs_open(const char *path, access_fn_t access) {
  inode_t *inode = &root;
  char buf[512];
  int nbuf;

  dbg("open: '%s'\n", path);

  inode = traverse_path(inode, path, access);

  /* Handle symlinks */
  unsigned nloop = 0;
  while (inode && inode->type == it_symlink) {
    if (++nloop >= MAX_SYMLINKS_TO_FOLLOW) {
      set_errno(ELOOP);
      return NULL;
    }

    nbuf = vfs_read(inode, 0, buf, 511);
    assert(nbuf > 0 && "Symlink read failed!");
    buf[nbuf] = '\0';

    inode = traverse_path( (buf[0] == '/') ? &root : inode->parent,
                           buf, access );
  }

  if (inode)
    ++inode->handles;
  return inode;
}

int64_t vfs_read(inode_t *node, uint64_t offset, void *buf, uint64_t sz) {
  dbg("read(%x)\n", node);
  assert(node->type != it_dir && "read() called on a directory!");
  return node->mountpoint->fs.read(&node->mountpoint->fs, node, offset, buf, sz);
}

int64_t vfs_write(inode_t *node, uint64_t offset, void *buf, uint64_t sz) {
  dbg("write(%x)\n", node);
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
