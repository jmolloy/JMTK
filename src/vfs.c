#include "string.h"
#include "vfs.h"

vector_t filesystems;

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
