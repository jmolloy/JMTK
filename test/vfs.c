#if 0
exit `$1 $2 | ./test/FileCheck $0`
#endif

#include "vfs.h"
#include "hal.h"
#include "stdio.h"
#include "assert.h"
#include "string.h"
#include "kmalloc.h"
#include "errno.h"

/*
  /
  +-+ a
  | +-- b
  | +-+ c
  | | +-- d
  | +-- e
  +-+ f
    +-- g
    +-- h  -> "g"
    +-- i  -> "/a/b"
    +-- j  -> "h"
*/

enum {
  n_root, n_a, n_b, n_c, n_d, n_e, n_f, n_g, n_h, n_i, n_j, n_END
} node_num;

typedef struct dummyfs {
  inode_t nodes[n_END];
  inode_t *root_inode;
} dummyfs_t;

char *datas[n_END] = {
  "",
  "",
  "my name is b, and b is my name\n",
  "",
  "I am d, son of c, first of my name\n",
  "Fool of a Took!\n",
  "",
  "You shall not pass!\n",
  "g",
  "/a/b",
  "h"
};

static int num_for_inode(dummyfs_t *dfs, inode_t *inode) {
  int n = -1;
  if (inode == dfs->root_inode) n = n_root;
  else if (inode == &dfs->nodes[n_a]) n = n_a;
  else if (inode == &dfs->nodes[n_b]) n = n_b;
  else if (inode == &dfs->nodes[n_c]) n = n_c;
  else if (inode == &dfs->nodes[n_d]) n = n_d;
  else if (inode == &dfs->nodes[n_e]) n = n_e;
  else if (inode == &dfs->nodes[n_f]) n = n_f;
  else if (inode == &dfs->nodes[n_g]) n = n_g;
  else if (inode == &dfs->nodes[n_h]) n = n_h;
  else if (inode == &dfs->nodes[n_i]) n = n_i;
  else if (inode == &dfs->nodes[n_j]) n = n_j;
  
  return n;
}

vector_t dreaddir(filesystem_t *fs, inode_t *dir) {
  dummyfs_t *dfs = fs->data;
  vector_t v = vector_new(sizeof(dirent_t), 4);
  dirent_t de;

#define ADD(x) de.name = #x; de.ino = &dfs->nodes[n_##x]; vector_add(&v, &de)
  if (dir == dfs->root_inode) {
    ADD(a);
    ADD(f);
  } else if (dir == &dfs->nodes[n_a]) {
    ADD(b);
    ADD(c);
    ADD(e);
  } else if (dir == &dfs->nodes[n_c]) {
    ADD(d);
  } else if (dir == &dfs->nodes[n_f]) {
    ADD(g);
    ADD(h);
    ADD(i);
    ADD(j);
  }

  return v;
#undef ADD
}

int64_t dread(filesystem_t *fs, inode_t *inode, uint64_t offset, void *buf, uint64_t sz) {
  dummyfs_t *dfs = fs->data;

  int num = num_for_inode(dfs, inode);
  switch (num) {
  default: assert(0 && "Not a file!");
  case n_b:
  case n_d:
  case n_e:
  case n_g:
  case n_h:
  case n_i:
  case n_j:
    assert(offset <= (unsigned)strlen(datas[num]));
    if (offset + sz >= (unsigned)strlen(datas[num])) {
      sz = (unsigned)strlen(datas[num]) - offset;
    }
    memcpy(buf, datas[num] + offset, sz);
    return sz;
  }
}

int64_t dwrite(filesystem_t *fs, inode_t *inode, uint64_t offset, void *buf, uint64_t sz) {
  dummyfs_t *dfs = fs->data;
  int num = num_for_inode(dfs, inode);
  switch (num) {
  default: assert(0 && "Not a file!");
  case n_b:
  case n_d:
  case n_e:
  case n_g:
  case n_h:
  case n_i:
  case n_j:
    assert(offset <= (unsigned)strlen(datas[num]));
    if (offset + sz >= (unsigned)strlen(datas[num])) {
      sz = strlen(datas[num]) - offset;
    }
    memcpy(datas[num] + offset, buf, sz);
    return sz;
  }
}

int dget_root(filesystem_t *fs, inode_t *inode) {
  dummyfs_t *dfs = fs->data;
  dfs->root_inode = inode;
  return 0;
}

filesystem_t dummyfs = {
  .read = &dread,
  .write = &dwrite,
  .readdir = &dreaddir,
  .mknod = NULL,
  .get_root = &dget_root
};

int dprobe(dev_t dev, filesystem_t *fs) {
  memcpy(fs, &dummyfs, sizeof(filesystem_t));

  dummyfs_t *dfs = kmalloc(sizeof(dummyfs_t));

  memset(dfs->nodes, 0, sizeof(inode_t) * n_END);
  dfs->nodes[n_a].type = it_dir;
  dfs->nodes[n_c].type = it_dir;
  dfs->nodes[n_f].type = it_dir;

  dfs->nodes[n_b].type = it_file; dfs->nodes[n_b].size = strlen(datas[n_b]);
  dfs->nodes[n_d].type = it_file; dfs->nodes[n_d].size = strlen(datas[n_d]);
  dfs->nodes[n_e].type = it_file; dfs->nodes[n_e].size = strlen(datas[n_e]);
  dfs->nodes[n_g].type = it_file; dfs->nodes[n_g].size = strlen(datas[n_g]);

  dfs->nodes[n_h].type = it_symlink; dfs->nodes[n_h].size = strlen(datas[n_h]);
  dfs->nodes[n_i].type = it_symlink; dfs->nodes[n_i].size = strlen(datas[n_i]);
  dfs->nodes[n_j].type = it_symlink; dfs->nodes[n_j].size = strlen(datas[n_j]);

  fs->data = dfs;

  return 0;
}

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

bool daccess(int mode) {
  return true;
}

int test() {
  // Ensure data is writable.
  for (int i = 0; i < n_END; ++i) {
    const char *di = datas[i];
    datas[i] = kmalloc(strlen(di) + 1);
    memcpy(datas[i], di, strlen(di) + 1);
  }

  // Check mounting
  // ----------------------------------------------------------------------

  // CHECK: register_filesystem = 0
  kprintf("register_filesystem = %d\n",
          register_filesystem("dummyfs", &dprobe));

  // CHECK: mount = 0
  kprintf("mount = %d\n",
          vfs_mount(makedev(DEV_MAJ_NULL, 0), vfs_get_root(), "dummyfs"));

  dummyfs_t *dfs = vfs_get_root()->mountpoint->fs.data;

  // CHECK: '' DIR
  // CHECK:   'a' DIR
  // CHECK:     'b'  (size 31)
  // CHECK:     'c' DIR
  // CHECK:       'd' (size 35)
  // CHECK:     'e' (size 16)
  // CHECK:   'f' DIR
  // CHECK:     'g' (size 20)
  // CHECK:     'h'  (size 1)
  // CHECK:     'i'  (size 4)
  vector_t done = vector_new(sizeof(inode_t*), 16);
  emit_tree("", vfs_get_root(), 0, &done);
  
  // Check basic reading/writing
  // ----------------------------------------------------------------------

  // CHECK: read = 20
  // CHECK: str = You shall not pass!
  char str[256];
  kprintf("read = %d\n",
          (int)vfs_read(&dfs->nodes[n_g], 0, str, 255));
  kprintf("str = %s\n", str);

  // CHECK: write = 10
  kprintf("write = %d\n",
          (int)vfs_write(&dfs->nodes[n_d], 6, "ragonheart", 10));
  // CHECK: read = 35
  // CHECK: str = I am dragonheart, first of my name
  kprintf("read = %d\n",
          (int)vfs_read(&dfs->nodes[n_d], 0, str, 255));
  kprintf("str = %s\n", str);

  // Check mounting subdirectories
  // ----------------------------------------------------------------------

  // Ensure we can't mount over a current mountpoint. check for EBUSY
  // CHECK: mount = 1
  // CHECK: errno = 16, EBUSY = 16
  kprintf("mount = %d\n",
          vfs_mount(makedev(DEV_MAJ_NULL, 0), vfs_get_root(), "dummyfs"));
  kprintf("errno = %d, EBUSY = %d\n", get_errno(), EBUSY);

  // Device should be already mounted!
  // CHECK: mount = 1
  // CHECK: errno = 16, EBUSY = 16
  kprintf("mount = %d\n",
          vfs_mount(makedev(DEV_MAJ_NULL, 0), &dfs->nodes[n_c], "dummyfs"));
  kprintf("errno = %d, EBUSY = %d\n", get_errno(), EBUSY);

  // CHECK: mount = 0
  kprintf("mount = %d\n",
          vfs_mount(makedev(DEV_MAJ_NULL, 1), &dfs->nodes[n_c], "dummyfs"));

  // Check mounting in a subdirectory.
  // CHECK: '' DIR
  // CHECK:   'a' DIR (size 0)
  // CHECK:     'b'  (size 31)
  // CHECK:     'c' DIR (size 0)
  // CHECK:       'a' DIR (size 0)
  // CHECK:         'b'  (size 31)
  // CHECK:         'c' DIR (size 0)
  // CHECK:           'd'  (size 35)
  // CHECK:         'e'  (size 16)
  // CHECK:       'f' DIR (size 0)
  // CHECK:         'g'  (size 20)
  // CHECK:         'h'  (size 1)
  // CHECK:         'i'  (size 4)
  // CHECK:     'e'  (size 16)
  // CHECK:   'f' DIR (size 0)
  // CHECK:     'g'  (size 20)
  // CHECK:     'h'  (size 1)
  // CHECK:     'i'  (size 4)
  vector_destroy(&done);
  done = vector_new(sizeof(inode_t*), 16);
  emit_tree("", vfs_get_root(), 0, &done);

  // Check unmounting.
  // ----------------------------------------------------------------------
  // CHECK: umount = 0
  // CHECK: '' DIR
  // CHECK:   'a' DIR (size 0)
  // CHECK:     'b'  (size 31)
  // CHECK:     'c' DIR (size 0)
  // CHECK:       'd'  (size 35)
  // CHECK:     'e'  (size 16)
  // CHECK:   'f' DIR (size 0)
  // CHECK:     'g'  (size 20)
  // CHECK:     'h'  (size 1)
  // CHECK:     'i'  (size 4)
  kprintf("umount = %d\n",
          vfs_umount(makedev(DEV_MAJ_NULL, 1), NULL));
  vector_destroy(&done);
  done = vector_new(sizeof(inode_t*), 16);
  emit_tree("", vfs_get_root(), 0, &done);

  // Check open()
  // ----------------------------------------------------------------------
  // CHECK: open() = {{[^0][0-9a-f]+}}
  kprintf("open() = %x\n", vfs_open("/a", &daccess));
  // CHECK: open() = 0
  kprintf("open() = %x\n", vfs_open("/azfg", &daccess));
  // CHECK: open() = {{[^0][0-9a-f]+}}
  kprintf("open() = %x\n", vfs_open("/a/b", &daccess));
  // CHECK: open() = 0
  // CHECK: errno = 2, ENOENT = 2
  kprintf("open() = %x\n", vfs_open("/a/f/g", &daccess));
  kprintf("errno = %d, ENOENT = %d\n", get_errno(), ENOENT);

  // CHECK: open() = {{[^0][0-9a-f]+}}
  kprintf("open() = %x\n", vfs_open("/a/c/d", &daccess));

  // CHECK: open(/f/g) = [[g:[0-9a-f]+]]
  // CHECK: open(/f/h) = [[g]]
  kprintf("open(/f/g) = %x\n", vfs_open("/f/g", &daccess));
  kprintf("open(/f/h) = %x\n", vfs_open("/f/h", &daccess));

  // CHECK: open(/a/b) = [[b:[0-9a-f]+]]
  // CHECK: open(/f/i) = [[b]]
  kprintf("open(/a/b) = %x\n", vfs_open("/a/b", &daccess));
  kprintf("open(/f/i) = %x\n", vfs_open("/f/i", &daccess));

  // Check transitive link handling (/f/j -> /f/h -> /f/g)
  // CHECK: open(/f/j) = [[g]]
  kprintf("open(/f/j) = %x\n", vfs_open("/f/j", &daccess));
  

  return 0;
}

static prereq_t r[] = { {"vfs",NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "vfs-test",
  .required = r,
  .load_after = NULL,
  .init = &test,
  .fini = NULL
};
module_t *test_module = &x;
