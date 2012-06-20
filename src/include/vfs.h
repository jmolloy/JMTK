#ifndef VFS_H
#define VFS_H

#include "types.h"

/*******************************************************************************
 * Virtual filesystem
 ******************************************************************************/

struct inode;

/* A filesystem driver. */
typedef struct filesystem {
  /* Reads from node 'node_data' 'sz' bytes from the given offset.
     Requires 'node_data' to be a normal file. Returns the number of
     bytes written or -errno on error. */
  int64_t (*read)(struct filesystem *fs,
                  void *node_data, uint64_t offset, void *buf, uint64_t sz);
  /* Writes to node 'node_data', 'sz' bytes from the given offset.
     Requires 'node_data' to be a normal file. Returns the number of
     bytes written or -errno on error. */
  int64_t (*write)(struct filesystem *fs,
                   void *node_data, uint64_t offset, void *buf, uint64_t sz);
  /* Returns the number of directory entries 'node_data' has. Requires
     'node_data' to be a directory. */
  unsigned (*num_dir_entries)(struct filesystem *fs,
                              void *node_data);
  /* Returns the name of the n'th entry in the directory 'node_data'.
     n must be less than the return value of num_dir_entries(). */
  const char * (*read_dir_entry_name)(struct filesystem *fs,
                                      void *node_data, unsigned n);
  /* Given a directory node and a blank inode structure, fill in the inode
     structure with data from the n'th directory entry. Return 0 on success,
     -errno on failure. */
  int (*fill_dir_entry)(struct filesystem *fs,
                        void *node_data, unsigned n,
                        struct inode *inode);
  /* Create a new node as a child of the directory 'node_data'. Requires
     'node_data' to be a directory. The fields 'name', 'type', 'mode',
     'uid' and 'gid' are taken from 'inode', and node specific data is
     written to it, along with 'ctime', 'mtime' and 'atime' which are
     all set to the current timestamp, and 'nlink' which is set to 1.
     'size' is set to 0. */
  int (*mknod)(struct filesystem *fs,
               void *node_data, struct inode *inode);

	/* Filesystem-specific data. */
	void *data;

	/* Device mounted on. */
	dev_t dev;
} filesystem_t;

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

// JM: Move this to vfs.h
#if 0

/* A mount point. This has a device that has been mounted, a location,
   and a filesystem to handle requests. */
typedef struct mountpoint {
  dev_t         dev;
  struct inode *node;
  filesystem_t *fs;
} mountpoint_t;

/* The type a VFS node can be. Normal, character/block device, pipe (FIFO) or
   socket. */
typedef enum inode_type {
  it_file, it_dir, it_chardev, it_blockdev, it_fifo, it_socket
} inode_type_t;

/* A VFS node, commonly known as an "inode". */
typedef struct inode {
  const char   *name;
  mountpoint_t *mountpoint;
  inode_type_t  type;
  struct inode *parent;

  /* Standard UNIX stat state. */
  int mode, nlink, uid, gid, size;
  uint64_t atime, mtime, ctime;

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
  };

  /* Implementation dependent data passed to the filesystem. */
  void         *data;
} inode_t;

/* Mounts a filesystem on the directory 'node'. If 'fs' is NULL, all known
   filesystems are probed. If not, only the filesystem specified is probed.
   
   Returns zero on success, -errno on failure. */
int mount(dev_t dev, inode_t node, const char *fs);
/* Unmounts. If the device is given (isn't 0), the mountpoint
   associated with it is unmounted. If not, the inode is expected to be
   valid and is unmounted. */
int umount(dev_t dev, inode_t inode);

/* Returns an inode_t for the given path and increments its open count. */
inode_t open(const char *path);
/* Performs a read of sz bytes into buf at offset. */
int64_t read(uint64_t offset, void *buf, uint64_t sz);
/* Performs a write of sz bytes from buf at offset. */
int64_t write(uint64_t offset, void *buf, uint64_t sz);
/* Decrements the open count of inode. */
void close(inode_t inode);

#endif /* VFS_H */
