#include "hal.h"
#include "vfs.h"
#include "assert.h"
#include "stdio.h"
#include "string.h"
#include "kmalloc.h"
#include "vmspace.h"

#ifdef DEBUG_vfat
# define dbg(args...) kprintf("vfat: " args)
#else
# define dbg(args...)
#endif

typedef struct vfat_header {
  uint8_t  header[3];           /* Should be 6B 3C 90 (JMP 3C; NOP). Irrelevant. */
  uint8_t  identifier[8];
  uint16_t bytes_per_sector;
  uint8_t  sectors_per_cluster;
  uint16_t reserved_sectors;
  uint8_t  num_fats;
  uint16_t num_dir_entries;
  uint16_t num_sectors;         /* If 0, this value is stored in 'large_num_sectors'. */
  uint8_t  media_desc_type;
  uint16_t sectors_per_fat16;
  uint16_t sector_per_track;
  uint16_t num_heads;
  uint32_t hidden_sectors;
  uint32_t large_num_sectors;

  union {
    struct vfat_16_header {
      uint8_t  drive_number;
      uint8_t  flags;
      uint8_t  boot_signature;
      uint32_t volume_id;
      char     volume_label[11];
      char     system_ident[8];
      uint8_t  bootcode[448];
      uint16_t signature;       /* Should be 0xAA55 */
    } __attribute__((packed)) h16;

    struct vfat_32_header {
      uint32_t sectors_per_fat32;
      uint16_t flags;
      uint16_t version;
      uint32_t root_cluster;
      uint16_t fsinfo_cluster;
      uint16_t backup_boot_sector;
      uint8_t  reserved[12];
      uint8_t  drive_number;
      uint8_t  flags2;
      uint8_t  boot_signature;
      uint32_t volume_id;
      char     volume_label[11];
      char     system_ident[8];
      uint8_t  bootcode[420];
      uint16_t signature;       /* Should be 0xAA55 */
    } __attribute__((packed)) h32;
  } u;

} __attribute__((packed)) vfat_header_t;

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20

#define ATTR_LFN       0x0F

typedef struct vfat_dir {
  char name[11];                /* 8.3 filename */
  uint8_t attributes;
  uint8_t reserved;
  uint8_t ctime_tenths;
  uint16_t ctime;
  uint16_t cdate;
  uint16_t adate;
  uint16_t cluster_hi;
  uint16_t mtime;
  uint16_t mdate;
  uint16_t cluster_lo;
  uint32_t size;
} __attribute__((packed)) vfat_dir_t;

typedef struct vfat_lfn {
  uint8_t order;
  uint16_t name_1[5];
  uint8_t attribute;
  uint8_t type;
  uint8_t checksum;
  uint16_t name_2[6];
  uint16_t zero;
  uint16_t name_3[2];
} __attribute__((packed)) vfat_lfn_t;

typedef struct vfat_filesystem {
  unsigned ty;
  vfat_header_t hdr;
  uint32_t first_data_sector;
  uint32_t cluster_size;
  uint32_t num_sectors;
  uint32_t sectors_per_fat;
  uint32_t num_data_sectors;
  uint32_t num_clusters;
  block_device_t *dev;
} vfat_filesystem_t;

static unsigned char *read_data(vfat_filesystem_t *fs, uint32_t sect) {
  kprintf("read_data: 1\n");
  uint8_t *sector = (uint8_t*)vmspace_alloc(&kernel_vmspace, 0x1000, 1);

  uintptr_t offs = 0 ;//sect % 8;
  
  kprintf("read_data: 2: %d\n", sect);
  assert(get_interrupt_state() != 0);
  int nbytes = fs->dev->read(fs->dev, (sect - offs) * 512, (void*)sector, 0x1000);
  kprintf("read_data: 3\n");
  if (nbytes != 0x1000) {
    dbg("unable to read from device (read returned %d)\n", nbytes);
    return NULL;
  }
  kprintf("read_data: offs = %d, %x\n", offs, sector + (offs * 512));
  return sector + (offs * 512);
}

static uint64_t to_unix_time(uint16_t date, uint16_t time) {
  return 0;
}

static uint32_t get_next_cluster(vfat_filesystem_t *fs,
                                 uint32_t cluster) {
  assert(fs->ty != 12 && "FAT12 not supported!");

  unsigned offset = (fs->ty == 32) ? (cluster * 4) : (cluster * 2);
  unsigned cluster_size = fs->hdr.sectors_per_cluster * fs->hdr.bytes_per_sector;

  unsigned sector = fs->hdr.reserved_sectors + (offset / cluster_size);
  unsigned idx = offset % cluster_size;

  unsigned char *data = read_data(fs, sector);
  
  uint32_t ret = * (uint32_t*) &data[idx];
  if (fs->ty == 16)
    ret &= 0xFFFF;
  dbg("get_next_cluster returning %x for %x\n", ret, cluster);
  return ret;
}

static unsigned char *get_cluster(vfat_filesystem_t *fs, uint32_t cluster) {
  uint32_t sector = ((cluster - 2) * fs->hdr.sectors_per_cluster) +
    fs->first_data_sector;

  return read_data(fs, sector);
}

static bool read_bpb(block_device_t *dev, vfat_header_t *hdr) {
  uint8_t *sector = (uint8_t*)vmspace_alloc(&kernel_vmspace, 0x1000, 1);

  if (dev->length(dev) < 512)
    return NULL;

  bool ret = true;

  int nbytes = dev->read(dev, 0ULL, (void*)sector, 0x1000);
  if (nbytes != 0x1000) {
    dbg("unable to read from device (read returned %d)\n", nbytes);
    ret = false;
  } else if (sector[510] != 0x55 || sector[511] != 0xAA) {
    ret = false;
  } else {
    memcpy((void*)hdr, sector, 512);
  }

  vmspace_free(&kernel_vmspace, 0x1000, (uintptr_t)sector, 1);
  return ret;
}

static vector_t read_directory(vfat_filesystem_t *fs, uint32_t cluster) {
  vector_t entries = vector_new(sizeof(inode_t*), 4);
  dbg("read_directory: %x\n", cluster);
  while (cluster) {
    dbg("woof\n");
    dbg("cluster size: %x (get clus %d)\n", fs->cluster_size, cluster);
    unsigned char *data = get_cluster(fs, cluster);
    dbg("HERE\n");
    vector_t name = vector_new(1, 16);
    for (unsigned idx = 0; idx < fs->cluster_size; idx += sizeof(vfat_dir_t)) {
      vfat_dir_t *dir = (vfat_dir_t*) &data[idx];
      if (dir->name[0] == 0x00 || ((unsigned char)dir->name[0]) == 0xE5)
        /* Entry does not exist. */
        continue;

      if (dir->attributes == ATTR_LFN) {
        /* Long filename entry. */
        vfat_lfn_t *lfn = (vfat_lfn_t*) dir;
      dbg("2\n");
        vector_add_multiple(&name, lfn->name_1, 5);
        vector_add_multiple(&name, lfn->name_2, 6);
        vector_add_multiple(&name, lfn->name_3, 2);
        dbg("3\n");
        continue;
      }
      dbg("1\n");

      vector_add_multiple(&name, dir->name, 11);
           vector_add(&name, "x");
            dbg("dir->name %s\n", vector_get_data(&name));
      /* FIXME: Use a slab. */
      inode_t *ino = kmalloc(sizeof(inode_t));

      ino->name = (char*) vector_get_data(&name);
      ino->type = (dir->attributes == ATTR_DIRECTORY) ? it_dir : it_file;
      ino->data = (void*) (dir->cluster_lo | (dir->cluster_hi << 16));
      ino->mode = 0777;
      ino->nlink = 1;
      ino->uid = ino->gid = ino->handles = 0;
      ino->u.dir_cache = 0;
      ino->size = dir->size;
      ino->atime = to_unix_time(dir->adate, 0);
      ino->ctime = to_unix_time(dir->cdate, dir->ctime);
      ino->mtime = to_unix_time(dir->mdate, dir->mtime);

      dbg("%s ty %d data %#x\n", "kk", ino->type, ino->data);
      vector_add(&entries, ino);
      
      vector_drop(&name);

    }
    dbg("end!\n");
    vector_destroy(&name);

    cluster = get_next_cluster(fs, cluster);
  }
  assert(0 && "impl, bitch.");
  return entries;
}

static vfat_filesystem_t *probe(block_device_t *dev) {
  vfat_header_t hdr;
  dbg("attempting probe\n");
  
  if (!read_bpb(dev, &hdr))
    return NULL;

  vfat_filesystem_t *fs = (vfat_filesystem_t*) kmalloc(sizeof(vfat_filesystem_t));

  fs->num_sectors = (hdr.num_sectors > 0) ? hdr.num_sectors : hdr.large_num_sectors;
  fs->sectors_per_fat = (hdr.sectors_per_fat16) ? hdr.sectors_per_fat16 :
    hdr.u.h32.sectors_per_fat32;
  fs->first_data_sector = hdr.reserved_sectors + (hdr.num_fats * fs->sectors_per_fat);

  fs->num_data_sectors = fs->num_sectors - fs->first_data_sector;

  fs->cluster_size = hdr.sectors_per_cluster * 512;

  if (fs->cluster_size == 0) {
    dbg("Cluster size was 0, bailing!\n");
    kfree(fs);
    return NULL;
  }
   
  fs->num_clusters = fs->num_data_sectors / hdr.sectors_per_cluster;
  if (fs->num_clusters < 4085)
    fs->ty = 12;
  else if (fs->num_clusters < 65525)
    fs->ty = 16;
  else
    fs->ty = 32;

  dbg("FAT%d partition detected\n", fs->ty);

  memcpy((uint8_t*)&fs->hdr, (uint8_t*)&hdr, sizeof(vfat_header_t));

  fs->dev = dev;

  return fs;
}

static int64_t vfat_read(filesystem_t *fs, inode_t *ino, uint64_t offset,
                         void *buf, uint64_t sz) {
  assert(0 && "Not implemented!");
}

static int64_t vfat_write(filesystem_t *fs, inode_t *ino, uint64_t offset,
                          void *buf, uint64_t sz) {
  assert(0 && "Not implemented!");
}

static vector_t vfat_readdir(filesystem_t *fs, inode_t *dir) {
  assert(dir->type == it_dir && "readdir can only be called on a directory!");
  vfat_filesystem_t *vfs = (vfat_filesystem_t*) fs->data;

  return read_directory(vfs, (uint32_t)dir->data);
}

static int vfat_mknod(filesystem_t *fs, inode_t *dir_ino, inode_t *dest_ino) {
  assert(0 && "Not implemented!");
}

static int vfat_get_root(filesystem_t *fs, inode_t *ino) {
  vfat_filesystem_t *vfs = (vfat_filesystem_t*) fs->data;

  uint32_t cluster = (vfs->ty == 32) ? vfs->hdr.u.h32.root_cluster : 0;

  ino->data = (void*) cluster;
  ino->type = it_dir;
  ino->mode = 0777;
  ino->nlink = 1;
  ino->uid = ino->gid = ino->handles = 0;
  ino->u.dir_cache = 0;

  return 0;
}

static filesystem_t fs_vfat = {
  .read = &vfat_read,
  .write = &vfat_write,
  .readdir = &vfat_readdir,
  .mknod = &vfat_mknod,
  .get_root = &vfat_get_root
};

static int vfat_probe(dev_t dev, filesystem_t *fs) {
  block_device_t *bdev = get_block_device(dev);
  if (!bdev) return 1;

  vfat_filesystem_t *vfs = probe(bdev);
  if (!vfs) return 1;

  memcpy(fs, &fs_vfat, sizeof(filesystem_t));
  fs->data = vfs;

  return 0;
}

static int vfat_init() {
  assert(register_filesystem("vfat", &vfat_probe) == 0);
  return 0;
}

static prereq_t req[] = { {"vfs",NULL}, {"kmalloc",NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "fs_vfat",
  .required = req,
  .load_after = NULL,
  .init = &vfat_init,
  .fini = NULL
};

