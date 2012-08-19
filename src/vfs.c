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
static mutex_t filesystem_lock, mountpoint_lock;
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

  mutex_acquire(&filesystem_lock);
  vector_add(&filesystems, &fsi);
  mutex_release(&filesystem_lock);
  return 0;
}

int unregister_filesystem(const char *ident) {
  mutex_acquire(&filesystem_lock);
  for (unsigned i = 0; i < vector_length(&filesystems); ++i) {
    fs_info_t *fsi = vector_get(&filesystems, i);
    if (!strcmp(fsi->ident, ident)) {
      vector_erase(&filesystems, i);
      mutex_release(&filesystem_lock);
      return 0;
    }
  }
  mutex_release(&filesystem_lock);
  return 1;
}

inode_t *vfs_get_root() {
  return &root;
}

int vfs_mount(dev_t dev, inode_t *node, const char *fs) {
  assert(node->type == it_dir && "mount() called on non-directory inode!");

  dbg("mount: mount dev %x fs %s\n", dev, fs);

  /* Is the device or inode already mounted? */
  mutex_acquire(&mountpoint_lock);
  for (unsigned i = 0; i < vector_length(&mountpoints); ++i) {
    mountpoint_t *mp = *(mountpoint_t**)vector_get(&mountpoints, i);
    if (mp->dev == dev || mp->node == node) {
      dbg("mount: device or inode already mounted!\n");
      set_errno(EBUSY);
      mutex_release(&mountpoint_lock);
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
        mutex_release(&mountpoint_lock);
        return 0;
      }

      if (fs != NULL) {
        dbg("mount() failed\n");
        mutex_release(&mountpoint_lock);
        kfree(mp);
        return 1;
      }
    }
  }
  dbg("mount() failed\n");
  mutex_release(&mountpoint_lock);
  kfree(mp);
  return 1;
}

int vfs_umount(dev_t dev, inode_t *node) {
 dbg("umount: umount dev %x node %x\n", dev, node);

  mutex_acquire(&mountpoint_lock);
  /* Is the device or inode already mounted? */
  for (unsigned i = 0; i < vector_length(&mountpoints); ++i) {
    mountpoint_t *mp = *(mountpoint_t**)vector_get(&mountpoints, i);
    if (mp->dev == dev || mp->node == node) {
      dbg("umount: unmounting device %x from node %x\n",
          mp->dev, mp->node);
      /* FIXME: Check if the FS is busy. Keep a refcount in mountpoint_t? */

      if (mp->fs.destroy)
        mp->fs.destroy(&mp->fs);

      /* Restore the inode as it was before it was mounted. */
      memcpy(mp->node, &mp->orig_inode_data, sizeof(inode_t));
      mutex_release(&mountpoint_lock);
      return 0;
    }
  }

  dbg("umount: target was not a mountpoint!\n");
  set_errno(EINVAL);
  mutex_release(&mountpoint_lock);
  return 1;
}

static void maybe_generate_dircache(inode_t *node) {
  /* Generate the directory cache if needed. */
  if (!node->u.dir_cache) {
    rwlock_read_release(&node->rwlock);
    rwlock_write_acquire(&node->rwlock);

    if (node->u.dir_cache) return;
    /* FIXME: Factor out dir cache generation from here and traverse_node() */
    dbg("... generating directory cache ...\n");
    vector_t v = node->mountpoint->fs.readdir(&node->mountpoint->fs, node);
    
    for (unsigned i = 0; i < vector_length(&v); ++i) {
      dirent_t *dent = vector_get(&v, i);
      inode_t *ino = dent->ino;

      ino->mountpoint = node->mountpoint;
      ino->parent = node;
      ino->write_buffer = NULL;
      ino->handles = 0;
      rwlock_init(&ino->rwlock);
    }

    node->u.dir_cache = directory_cache_new(v);
    assert(node->u.dir_cache);

    rwlock_write_release(&node->rwlock);
    rwlock_read_acquire(&node->rwlock);
  }
}

vector_t vfs_readdir(inode_t *node) {
  dbg("readdir(%x)\n", node);

  assert(node->type == it_dir && "readdir() called on non-directory inode!");

  rwlock_read_acquire(&node->rwlock);
  maybe_generate_dircache(node);

  vector_t v = directory_cache_get_all(node->u.dir_cache);
  rwlock_read_release(&node->rwlock);

  return v;
}

/* Attempt to traverse from 'parent' to its child in 'path', assuming we hold a read lock
   on 'parent'.

   Return an inode with a read lock held. */
static inode_t *traverse_node(inode_t *parent,
                              const char *path, access_fn_t access) {
  dbg("_traverse: parent %x path '%s'\n", parent, path);
  if (parent->type != it_dir) {
    dbg("_traverse: parent was not a directory!\n");
    set_errno(ENOENT);
    rwlock_read_release(&parent->rwlock);
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
    rwlock_read_release(&parent->rwlock);
    return NULL;
  }

  /* Generate the directory cache if needed. */
  maybe_generate_dircache(parent);

  inode_t *child = directory_cache_get(parent->u.dir_cache, path);
  if (!child) {
    dbg("_traverse: no such file or directory!\n");
    set_errno(ENOENT);
    rwlock_read_release(&parent->rwlock);
    return NULL;
  }

  rwlock_read_acquire(&child->rwlock);
  rwlock_read_release(&parent->rwlock);

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
        rwlock_read_release(&inode->rwlock);
        return NULL;
      }

      nbuf = vfs_read(inode, 0, buf, 511);
      assert(nbuf > 0 && "Symlink read failed!");
      buf[nbuf] = '\0';

      inode_t *parent = (buf[0] == '/') ? &root : inode->parent;
      rwlock_read_acquire(&parent->rwlock);
      rwlock_read_release(&inode->rwlock);

      inode = traverse_path( parent, buf, access );
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

  rwlock_read_acquire(&inode->rwlock);
  dbg("lopen: '%s'\n", path);

  inode = traverse_path(inode, path, access);

  if (inode)
    ++inode->handles;

  rwlock_read_release(&inode->rwlock);

  return inode;
}

inode_t *vfs_open(const char *path, access_fn_t access) {
  inode_t *inode = &root;
  char buf[512];
  int nbuf;

  rwlock_read_acquire(&inode->rwlock);

  dbg("open: '%s'\n", path);

  inode = traverse_path(inode, path, access);

  /* Handle symlinks */
  unsigned nloop = 0;
  while (inode && inode->type == it_symlink) {
    if (++nloop >= MAX_SYMLINKS_TO_FOLLOW) {
      set_errno(ELOOP);
      rwlock_read_release(&inode->rwlock);
      return NULL;
    }

    nbuf = vfs_read(inode, 0, buf, 511);
    assert(nbuf > 0 && "Symlink read failed!");
    buf[nbuf] = '\0';

    inode = traverse_path( (buf[0] == '/') ? &root : inode->parent,
                           buf, access );
  }

  if (inode) {
    ++inode->handles;
    rwlock_read_release(&inode->rwlock);
  }

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

  rwlock_write_acquire(&node->rwlock);
  --node->handles;
  rwlock_write_release(&node->rwlock);
}

int vfs_mknod(inode_t *parent,
              const char *name,
              inode_type_t type,
              int mode, int uid, int gid) {
  dbg("mknod('%s', type=%d, mode=%d, uid=%d, gid=%d)\n",
      name, type, mode, uid, gid);

  assert(parent && parent->type == it_dir);

  inode_t *ino = kmalloc(sizeof(inode_t));
  ino->mountpoint = parent->mountpoint;
  ino->type = type;
  ino->parent = parent;
  ino->mode = mode;
  ino->nlink = 1;
  ino->uid = uid;
  ino->gid = gid;
  ino->size = 0;
  ino->handles = 0;
  ino->u.dir_cache = 0;
  rwlock_init(&ino->rwlock);

  int ret = parent->mountpoint->fs.mknod(&parent->mountpoint->fs,
                                         parent, ino, name); 
  if (ret != 0)
    return ret;

  rwlock_write_acquire(&parent->rwlock);
  if (parent->u.dir_cache) {
    dirent_t *dent = kmalloc(sizeof(dirent_t));
    char *name_cpy = kmalloc(strlen(name) + 1);
    strcpy(name_cpy, name);
    dent->name = name_cpy;
    dent->ino = ino;

    directory_cache_add(parent->u.dir_cache, dent);
  }
  rwlock_write_release(&parent->rwlock);

  return 0;
}

static int vfs_init() {
  filesystems = vector_new(sizeof(fs_info_t), 4);
  mountpoints = vector_new(sizeof(mountpoint_t*), 4);

  mutex_init(&filesystem_lock);
  mutex_init(&mountpoint_lock);

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
  rwlock_init(&root.rwlock);

  return 0;
}

static int vfs_fini() {
  /* Call all destroy functions. */
  for (unsigned i = 0; i < vector_length(&mountpoints); ++i) {
    mountpoint_t *mp = * (mountpoint_t**)vector_get(&mountpoints, i);
    
    if (mp->fs.destroy)
      mp->fs.destroy(&mp->fs);
  }

  return 0;
}

static prereq_t req[] = { {"kmalloc",NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "vfs",
  .required = req,
  .load_after = NULL,
  .init = &vfs_init,
  .fini = &vfs_fini
};
