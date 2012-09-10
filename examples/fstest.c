#include "stdio.h"
#include "vfs.h"
#include "hal.h"
#include "string.h"
#include "assert.h"
#include "kmalloc.h"


bool dummy_access(int mode) {
  return true;
}

#define SZ 128
void op_cat(const char **params, int nparams) {
  assert(nparams > 0);
  inode_t *ino = vfs_open(params[0], &dummy_access);

  assert(ino && "File not found!");
  assert(ino->type == it_file && "File is not a regular file!");

  kprintf("START CAT (size %d)\n", ino->size);

  char *buf = kmalloc(ino->size+1);
  vfs_read(ino, 0, buf, ino->size);
  unsigned sz = ino->size;
  unsigned offs = 0;
  while (sz) {
    unsigned thesz = (sz > SZ) ? SZ : sz;

    unsigned char tmp = buf[offs+thesz];
    buf[offs+thesz] = 0;
    kprintf("%s", &buf[offs]);
    buf[offs+thesz] = tmp;

    offs += thesz;
    sz -= thesz;
  }

  kprintf("\nEND CAT (size %d)\n", ino->size);
}

void op_ls(const char **params, int nparams) {
  assert(nparams > 0);
  inode_t *ino = vfs_open(params[0], &dummy_access);

  assert(ino && "File not found!");
  vector_t files = vfs_readdir(ino);

  for (unsigned i = 0; i < vector_length(&files); ++i) {
    dirent_t *dent = vector_get(&files, i);
    const char *c;
    switch (dent->ino->type) {
    case it_file: c = "FILE"; break;
    case it_dir: c = "DIR"; break;
    case it_chardev: c = "CDEV"; break;
    case it_blockdev: c = "BDEV"; break;
    case it_fifo: c = "FIFO"; break;
    case it_socket: c = "SOCK"; break;
    default: assert(0);
    }
    kprintf("[[%s]] %s : nlink %d mode %x ctime %d mtime %d atime %d uid %d gid %d size %d\n",
            c,
            dent->name,
            dent->ino->nlink, dent->ino->mode,
            dent->ino->ctime, dent->ino->mtime, dent->ino->atime,
            dent->ino->uid, dent->ino->gid, dent->ino->size);
  }
}

void op_write(const char **params, int nparams) {
  assert(nparams > 1);
  inode_t *ino = vfs_open(params[0], &dummy_access);

  if (!ino) {
    /* Find parent. */
    char *str = NULL;
    int i;
    for (i = strlen(params[0]); i >= 0; --i) {
      if (params[0][i] == '/') {
        str = kmalloc(i+1);
        strncpy(str, params[0], i);
        str[i] = '\0';
        break;
      }
    }
    assert(str && "Parent directory not found!");

    ino = vfs_open(str, &dummy_access);
    assert(ino && "Parent directory not found!");

    vfs_mknod(ino, &params[0][i+1], it_file, 0755, 0, 0);
    vfs_close(ino);
    ino = vfs_open(params[0], &dummy_access);

    assert(ino && "File not found after having created it!");
  }

  assert(ino && "File not found!");
  assert(ino->type == it_file && "File is not a regular file!");

  vfs_write(ino, 0, (void*)params[1], strlen(params[1]));
  vfs_close(ino);
}

void op_mkdir(const char **params, int nparams) {
  inode_t *ino = vfs_open(params[0], &dummy_access);

  assert(!ino && "Directory exists!");
    
  /* Find parent. */
  char *str = NULL;
  int i;
  for (i = strlen(params[0]); i >= 0; --i) {
    if (params[0][i] == '/') {
      str = kmalloc(i+1);
      strncpy(str, params[0], i);
      str[i] = '\0';
      break;
    }
  }
  assert(str && "Parent directory not found!");

  ino = vfs_open(str, &dummy_access);
  assert(ino && "Parent directory not found!");

  vfs_mknod(ino, &params[0][i+1], it_dir, 0777, 0, 0);

  ino = vfs_open(params[0], &dummy_access);
  assert(ino && "Directory not found after having created it!");
  
  vfs_close(ino);
}

void kmain(int argc, char **argv) {
  assert(argc > 2 && "Usage: fstest <fstype> <op> <params>");

  const char *fstype = argv[1];
  const char *op = argv[2];
  const char **params = (const char**) &argv[3];
  int nparams = argc - 2;

  /* Mount /dev/hda on / */
  int st = vfs_mount(makedev(DEV_MAJ_HDA, 0), vfs_get_root(), fstype);
  assert(st == 0 && "mount failed!");

  if (!strcmp(op, "cat"))
    op_cat(params, nparams);
  else if (!strcmp(op, "ls"))
    op_ls(params, nparams);
  else if (!strcmp(op, "write"))
    op_write(params, nparams);
  else if (!strcmp(op, "mkdir"))
    op_mkdir(params, nparams);
  else
    kprintf("Unknown command: %s!\n", op);
}
