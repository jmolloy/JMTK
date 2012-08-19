#ifndef VFS_H
#define VFS_H

#include "hal.h"
#include "types.h"
#include "adt/vector.h"

/*******************************************************************************
 * Virtual filesystem
 ******************************************************************************/

struct inode;

/* A filesystem driver. */
typedef struct filesystem {
  /* Reads from node 'node_data' 'sz' bytes from the given offset.
     Requires 'node_data' to be a normal file. Returns the number of
     bytes written or -1 on error. */
  int64_t (*read)(struct filesystem *fs,
                  struct inode *inode, uint64_t offset, void *buf, uint64_t sz);
  /* Writes to node 'node_data', 'sz' bytes from the given offset.
     Requires 'node_data' to be a normal file. Returns the number of
     bytes written or -1 on error. */
  int64_t (*write)(struct filesystem *fs,
                   struct inode *inode, uint64_t offset, void *buf, uint64_t sz);
  /* Returns a vector of inode_t's that are the children of directory 'dir'. */
  vector_t (*readdir)(struct filesystem *fs, struct inode *dir);
  /* Create a new node as a child of the directory 'node_data'. Requires
     'dir_inode' to be a directory. The fields 'type', 'mode',
     'uid' and 'gid' are taken from 'inode', and node specific data is
     written to it, along with 'ctime', 'mtime' and 'atime' which are
     all set to the current timestamp, and 'nlink' which is set to 1.
     'size' is set to 0. The name of the node is given in 'name'. */
  int (*mknod)(struct filesystem *fs,
               struct inode *dir_inode, struct inode *dest_inode,
               const char *name);

  /* Populates 'inode' with information for the root directory.
     Returns 0 on success, nonzero on failure. */
  int (*get_root)(struct filesystem *fs, struct inode *inode);

  /* Run when the filesystem is unmounted / torn down. */
  void (*destroy) (struct filesystem *fs);

  /* Filesystem-specific data. */
  void *data;
  
  /* Device mounted on. */
  dev_t dev;
} filesystem_t;

struct mountpoint;

/* The type a VFS node can be. Normal, character/block device, pipe (FIFO) or
   socket. */
typedef enum inode_type {
  it_file, it_dir, it_chardev, it_blockdev, it_fifo, it_socket, it_symlink
} inode_type_t;

/* A VFS node, commonly known as an "inode". */
typedef struct inode {
  struct mountpoint *mountpoint;
  inode_type_t  type;
  struct inode *parent;

  /* Standard UNIX stat state. */
  int mode, nlink, uid, gid, size;
  uint64_t atime, mtime, ctime;

  /* Readers/writers lock for mutation. */
  rwlock_t rwlock;

  /* Write buffer */
  void *write_buffer;

  /* How many times this inode has been opened. */
  unsigned handles;

  union {
    /* If this is a directory, this will hold a cache of the currently known
       directory entries. */
    struct directory_cache *dir_cache;
    /* If this is a special device, this will hold the device ID. */
    dev_t dev;
  } u;

  /* Implementation dependent data passed to the filesystem. */
  void *data;
} inode_t;

typedef struct dirent {
  const char *name;
  inode_t *ino;
} dirent_t;

/* A mount point. This has a device that has been mounted, a location,
   and a filesystem to handle requests. */
typedef struct mountpoint {
  dev_t         dev;
  struct inode *node;
  filesystem_t  fs;
  inode_t       orig_inode_data;
} mountpoint_t;

/* Registers a filesystem with name "ident", and a probe function that
   will attempt to find a filesystem of this kind on the given device.

   If it succeeds, it should fill in the 'fs' structure and return zero.
   Else, it should return nonzero.

   This function returns zero if the FS was successfully registered,
   nonzero otherwise. */
int register_filesystem(const char *ident,
                        int (*probe)(dev_t dev, filesystem_t *fs));
/* Unregisters a filesystem previously registered by register_filesystem.
   Returns zero on success, nonzero on failure. */
int unregister_filesystem(const char *ident);

/* Mounts a filesystem on the directory 'node'. If 'fs' is NULL, all known
   filesystems are probed. If not, only the filesystem specified is probed.
   
   Returns zero on success, nonzero on failure. */
int vfs_mount(dev_t dev, inode_t *node, const char *fs);

/* Unmounts. If the device is given (isn't 0), the mountpoint
   associated with it is unmounted. If not, the inode is expected to be
   valid and is unmounted. */
int vfs_umount(dev_t dev, inode_t *inode);

typedef bool (*access_fn_t)(int mode);

/* Returns an inode_t for the given path and increments its open count.

   If given, the function 'access' is called when open() is uncertain if
   a directory is searchable (+x) for the current user. It is called with
   the mode of the directory, and should return true if the user is allowed
   to search the directory. */
inode_t *vfs_open(const char *path, access_fn_t access);

/* Identical to vfs_open, except that if path refers to a symbolic link,
   return the inode for the link rather than its pointee. */
inode_t *vfs_lopen(const char *path, access_fn_t access);

/* Performs a read of sz bytes into buf at offset. */
int64_t vfs_read(inode_t *inode, uint64_t offset, void *buf, uint64_t sz);

/* Performs a write of sz bytes from buf at offset. */
int64_t vfs_write(inode_t *inode, uint64_t offset, void *buf, uint64_t sz);

/* Decrements the open count of inode. */
void vfs_close(inode_t *inode);

/* Returns the inodes contained in this directory. */
vector_t vfs_readdir(inode_t *inode);

/* Returns the root inode. */
inode_t *vfs_get_root();

int vfs_mknod(inode_t *parent,
              const char *name,
              inode_type_t type,
              int mode, int uid, int gid);

#endif /* VFS_H */
