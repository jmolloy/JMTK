#include "adt/hashtable.h"
#include "assert.h"
#include "block_cache.h"
#include "errno.h"
#include "hal.h"
#include "kmalloc.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "vfs.h"
#include "vmspace.h"

#ifdef DEBUG_vfat
# define dbg(args...) kprintf("vfat: " args)
#else
# define dbg(args...)
#endif

#define AREA_RESERVED 0
#define AREA_FAT      1
#define AREA_DATA     2
#define AREA_END      3

typedef struct vfat_filesystem {
  unsigned ty;
  uint32_t root_cluster;
  uint32_t cluster_size;
  uint32_t num_clusters;
  uint32_t pointers_per_cluster;
  uint32_t free_cluster_hint;
  uint64_t areas[AREA_END+1];
  block_device_t *dev;

  disk_cache_t *cache;
  void *cache_ptr;
  uint64_t cached_address;

  hashtable_t known_inos;
} vfat_filesystem_t;

typedef struct vfat_file {
  vector_t clusters;
  bool cluster_chain_read;
  struct vfat_file *dir_file;
  uint32_t dir_offset;
  uintptr_t size;
  uintptr_t first_free_dir_entry;
} vfat_file_t;

/* Clusters ************************************/

static unsigned char *read_cluster(vfat_filesystem_t *fs, uint32_t cluster, int area) {
  const char *area_str;
  switch (area) {
  case AREA_RESERVED: area_str = "AREA_RESERVED"; break;
  case AREA_FAT: area_str = "AREA_FAT"; break;
  case AREA_DATA: area_str = "AREA_DATA"; break;
  default: area_str = "<INVALID>";
  }
  dbg("read_cluster(%d, %s)\n", cluster, area_str);
  /* Silence unused variable warnings in release mode. */
  (void)area_str;
  
  /* If it's difficult or you can't be bothered to implement some edge-case,
     assume it can't happen ;) */
  assert(fs->cluster_size <= get_page_size() && "More than one page per cluster not implemented yet!");

  uint64_t address = fs->areas[area] + cluster * fs->cluster_size;
  uint32_t offset = address & get_page_mask();

  if (fs->cached_address != ~0ULL) {
    disk_cache_release(fs->cache, fs->cached_address);
    unmap((uintptr_t)fs->cache_ptr, 1);
  }
  disk_cache_get(fs->cache, address, fs->cache_ptr);
  fs->cached_address = address;

  return (unsigned char*)fs->cache_ptr + offset;
}

static uint32_t find_free_cluster(vfat_filesystem_t *fs) {
  uint32_t cached_cluster = ~0U;
  uint16_t *cache16 = NULL;
  uint32_t *cache32 = NULL;
  
  for (uint32_t i = fs->free_cluster_hint; i < fs->num_clusters; ++i) {
    uint32_t cluster_num = i / fs->pointers_per_cluster;
    uint32_t cluster_idx = i % fs->pointers_per_cluster;

    if (cached_cluster != cluster_num) {
      cache16 = (uint16_t*) read_cluster(fs, cluster_num, AREA_FAT);
      cache32 = (uint32_t*) cache16;
      cached_cluster = cluster_num;
    }
    assert(cache16 && cache32);

    if ((fs->ty == 16 && cache16[cluster_idx] == 0) ||
        (fs->ty == 32 && cache32[cluster_idx] == 0)) {
      dbg("find_free_cluster() -> %d\n", i);
      return i;
    }
  }
  return ~0U;
}

/* Cluster chains ******************************/

static uintptr_t eoc_value(vfat_filesystem_t *fs) {
  switch (fs->ty) {
  default: assert(0); break;
  case 12: return 0xFF8;
  case 16: return 0xFFF8;
  case 32: return 0x0FFFFF8;
  }  
}

static bool is_eoc(vfat_filesystem_t *fs, uint32_t cluster) {
  return cluster >= eoc_value(fs);
}

static uint32_t get_next_cluster(vfat_filesystem_t *fs,
                                 uint32_t cluster) {
  assert(fs->ty != 12 && "FAT12 not supported!");
  assert(cluster != 0);

  unsigned idx = cluster % fs->pointers_per_cluster;

  uint16_t *data16 = (uint16_t*) read_cluster(fs, cluster / fs->pointers_per_cluster, AREA_FAT);
  uint32_t *data32 = (uint32_t*) data16;

  unsigned ret;
  if (fs->ty == 16)
    ret = data16[idx];
  else
    ret = data32[idx] & 0x0FFFFFFF;

  dbg("get_next_cluster(%x) -> %x\n", cluster, ret);
  return ret;
}

static void set_next_cluster(vfat_filesystem_t *fs,
                             uint32_t cluster,
                             uint32_t next_cluster) {
  assert(fs->ty != 12 && "FAT12 not supported!");
  dbg("set_next_cluster(cluster=%#x, next_cluster=%#x)\n",
      cluster, next_cluster);
  
  unsigned idx = cluster % fs->pointers_per_cluster;

  uint16_t *data16 = (uint16_t*) read_cluster(fs, cluster / fs->pointers_per_cluster, AREA_FAT);
  uint32_t *data32 = (uint32_t*) data16;

  if (fs->ty == 16)
    data16[idx] = next_cluster & 0xFFFF;
  else
    data32[idx] = (data32[idx] & 0xF0000000) | (next_cluster & 0x0FFFFFFF);
}

static void read_cluster_chain(vfat_filesystem_t *fs, vfat_file_t *file) {
  assert(!file->cluster_chain_read);
  assert(vector_length(&file->clusters) == 1);

  dbg("read_cluster_chain() start\n");

  uintptr_t cluster = *(uintptr_t*) vector_get(&file->clusters, 0);
  while (!is_eoc(fs, cluster)) {
    dbg("    cluster: %d\n", cluster);
    cluster = get_next_cluster(fs, cluster);
    if (!is_eoc(fs, cluster))
      vector_add(&file->clusters, &cluster);
  }
  file->cluster_chain_read = true;

  dbg("read_cluster_chain() end\n");
}

static void write_cluster_chain(vfat_filesystem_t *fs, vfat_file_t *file) {
  assert(file->cluster_chain_read);
  
  for (unsigned i = 0; i < vector_length(&file->clusters); ++i) {
    uintptr_t cluster = *(uintptr_t*) vector_get(&file->clusters, i);
    uintptr_t next_cluster = (i != vector_length(&file->clusters) - 1) ?
      *(uintptr_t*)vector_get(&file->clusters, i+1) : eoc_value(fs);
    set_next_cluster(fs, cluster, next_cluster);
  }
}

/* Dates ***************************************/

static uint64_t to_unix_time(uint16_t date, uint16_t time) {
  unsigned day_of_month  = date & 0x1F;
  unsigned month_of_year = (date >> 5) & 0x0F;
  unsigned year          = (date >> 9) + 1980;

  unsigned seconds       = (time & 0x1F) * 2;
  unsigned minutes       = (time >> 5) & 0x3F;
  unsigned hours         = (time >> 11) & 0x1F;

  return to_unix_timestamp(day_of_month, month_of_year, year,
                           seconds, minutes, hours);
}

static void from_unix_time(uint64_t ts, uint16_t *date, uint16_t *time) {
  unsigned day_of_month, month_of_year, year;
  unsigned seconds, minutes, hours;

  from_unix_timestamp(ts, &day_of_month, &month_of_year, &year,
                      &seconds, &minutes, &hours);

  *date  = day_of_month & 0x1F;
  *date |= (month_of_year & 0x0F) << 5;
  *date |= (year - 1980) << 9;

  *time  = (seconds / 2) & 0x1F;
  *time |= (minutes & 0x3F) << 5;
  *time |= (hours & 0x1F) << 11;
}

/* Data ****************************************/

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20

/** The directory structure contains the file's name in **8.3** format - for example MYEXEC~1.EXE.

    Microsoft added the *long file name extension* late in FAT's lifetime to get around this restriction. It works by having pseudo directory descriptors before a real descriptor, each defining 13 characters (in UTF-16) of the filename. It's a pretty ugly hack.

    If a directory entry has the attribute 0x0F, it should be treated as a long file name pseudo entry. { */
#define ATTR_LFN       0x0F

/* A directory in the FAT filesystem. */
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

/* A long-filename pseudo-directory. */
typedef struct vfat_lfn {
  uint8_t order;
  uint16_t name_1[5]; /* Characters 0..4 */
  uint8_t attribute;
  uint8_t type;
  uint8_t checksum;
  uint16_t name_2[6]; /* Characters 5..10 */
  uint16_t zero;
  uint16_t name_3[2]; /* Characters 11..12 */
} __attribute__((packed)) vfat_lfn_t;

static int64_t write(vfat_filesystem_t *vfs, vfat_file_t *file, uint64_t offset,
                     void *buf, uint64_t sz, bool update_attributes) {
  unsigned char *cbuf = (unsigned char*) buf;
  uint64_t sz_to_write = sz;

  bool clusters_modified = false;
  
  if (!file->cluster_chain_read)
    read_cluster_chain(vfs, file);
  
  /* Update accessed time and modified time. */
  if (update_attributes && file->dir_file) {
    uint16_t date, time;
    from_unix_time(get_timestamp(), &date, &time);

    write(vfs, file->dir_file,
          file->dir_offset + offsetof(vfat_dir_t, adate), &date, 2, false);
    write(vfs, file->dir_file,
          file->dir_offset + offsetof(vfat_dir_t, mdate), &date, 2, false);
    write(vfs, file->dir_file,
          file->dir_offset + offsetof(vfat_dir_t, mtime), &time, 2, false);
    if (sz + offset > file->size) {
      file->size = sz + offset;
      write(vfs, file->dir_file,
            file->dir_offset + offsetof(vfat_dir_t, size),
            &file->size, 4, false);
    }
  }

  while (sz > 0) {
    unsigned cluster_num  = offset / vfs->cluster_size;
    unsigned cluster_offs = offset % vfs->cluster_size;
    unsigned cluster_avail = vfs->cluster_size - cluster_offs;
    unsigned write_sz = (sz >= cluster_avail) ? cluster_avail : sz;

    if (cluster_num >= vector_length(&file->clusters)) {
      clusters_modified = true;
      uint32_t c = find_free_cluster(vfs);
      if (c == ~0U) {
        dbg("find_free_cluster returned failure!");
        set_errno(ENOSPC);
        return sz_to_write - sz;
      }
      vector_add(&file->clusters, &c);
    }

    unsigned char *data = read_cluster(vfs,
                                       *(uintptr_t*)vector_get(&file->clusters, cluster_num) - 2,
                                       AREA_DATA);
    memcpy(data+cluster_offs, cbuf, write_sz);

    offset += write_sz;
    sz -= write_sz;
    cbuf += write_sz;
  }
  
  if (clusters_modified)
    write_cluster_chain(vfs, file);

  return (int64_t)sz_to_write;
}

static int64_t read(vfat_filesystem_t *vfs, vfat_file_t *file, uint64_t offset,
                    void *buf, uint64_t sz) {
  unsigned char *cbuf = (unsigned char*) buf;
  uint64_t max_read_sz = file->size - offset;
  uint64_t sz_to_read = sz = (sz > max_read_sz) ? max_read_sz : sz;

  dbg("read: offset %d size %d clusters[0] %d\n", offset, sz,
      * (uintptr_t*) vector_get(&file->clusters, 0));

  if (!file->cluster_chain_read)
    read_cluster_chain(vfs, file);
  
  /* Update accessed time. */
  uint16_t date, time;
  from_unix_time(get_timestamp(), &date, &time);
  if (file->dir_file)
    write(vfs, file->dir_file,
          file->dir_offset + offsetof(vfat_dir_t, adate), &date, 2, false);

  while (sz > 0) {
    unsigned cluster_num  = offset / vfs->cluster_size;
    unsigned cluster_offs = offset % vfs->cluster_size;
    unsigned cluster_avail = vfs->cluster_size - cluster_offs;
    unsigned read_sz = (sz >= cluster_avail) ? cluster_avail : sz;

    unsigned cluster = *(uintptr_t*)vector_get(&file->clusters, cluster_num) - 2;

    unsigned char *data = read_cluster(vfs,
                                       cluster,
                                       AREA_DATA);
    memcpy(cbuf, data+cluster_offs, read_sz);

    offset += read_sz;
    sz -= read_sz;
    cbuf += read_sz;
  }
  
  return (int64_t)sz_to_read;
}

/* Directories *********************************/

static vfat_file_t *make_file_t(uint32_t cluster,
                                vfat_file_t *parent_dir,
                                uint32_t parent_offset,
                                uint32_t size) {
  vfat_file_t *file = kmalloc(sizeof(vfat_file_t));
  file->clusters = vector_new(sizeof(uintptr_t), 8);
  vector_add(&file->clusters, &cluster);
  file->cluster_chain_read = false;
  file->dir_file = parent_dir;
  file->dir_offset = parent_offset;
  file->size = size;

  return file;
}

static uintptr_t cache_dir_size(vfat_filesystem_t *fs, vfat_file_t *dir) {
  if (!dir->cluster_chain_read)
    read_cluster_chain(fs, dir);
  dir->size = fs->cluster_size * vector_length(&dir->clusters);
  return dir->size;
}

static uint32_t add_to_directory(vfat_filesystem_t *fs, vfat_file_t *dir, vector_t *entries) {
  dbg("add_to_directory(nentries=%d, first_free=%d)\n", vector_length(entries), dir->first_free_dir_entry);
  vfat_dir_t *dirs = kmalloc(sizeof(vfat_dir_t) * vector_length(entries));

  for (unsigned i = 0; i < vector_length(entries); ++i)
    dirs[i] = *(vfat_dir_t*)vector_get(entries, i);

  uint32_t offs = dir->first_free_dir_entry;

  write(fs, dir, offs, dirs,
        sizeof(vfat_dir_t) * vector_length(entries), true);  

  dir->first_free_dir_entry += sizeof(vfat_dir_t) * vector_length(entries);

  kfree(dirs);

  return dir->first_free_dir_entry - sizeof(vfat_dir_t);
}

static vector_t read_directory(vfat_filesystem_t *fs, vfat_file_t *node) {
  dbg("read_directory(cluster[0] = %d)\n", *(uintptr_t*)vector_get(&node->clusters, 0));
  vector_t entries = vector_new(sizeof(dirent_t), 4);

  unsigned char *buf = kmalloc(4096);
  uintptr_t offset = 0;
  vector_t name = vector_new(2, 16);

  uint64_t sz_read;
  bool cont = true;
  do {
    sz_read = read(fs, node, offset, buf, 4096);

    for (unsigned idx = 0; idx < sz_read; idx += sizeof(vfat_dir_t)) {
      vfat_dir_t *dir = (vfat_dir_t*) &buf[idx];

      if ((unsigned char)dir->name[0] == 0xE5)
        /* Entry does not exist (but there are more entries after this). */
        continue;
      
      if (dir->name[0] == 0x00) {
        /* Entry does not exist, and there are no more entries. */
        node->first_free_dir_entry = offset + idx;
        cont = false;
        break;
      }

      if (dir->attributes == ATTR_LFN) {
        /* Long filename entry. */
        vfat_lfn_t *lfn = (vfat_lfn_t*) dir;
        
        vector_add_multiple(&name, lfn->name_1, 5);
        vector_add_multiple(&name, lfn->name_2, 6);
        vector_add_multiple(&name, lfn->name_3, 2);
        continue;
      }

      uintptr_t data_cluster = dir->cluster_lo | (dir->cluster_hi << 16);

      char *n;
      /* If we had a LFN entry, convert from utf16. */
      if (vector_length(&name) > 0) {
        uint16_t *n_utf16 = (uint16_t*) vector_get_data(&name);
        n = kmalloc(vector_length(&name) * 2);
        vector_drop(&name);
        utf16_to_utf8((uint8_t*)n, n_utf16);
      } else {
        n = kmalloc(12);
        memcpy(n, dir->name, 11);
        n[11] = '\0';
      }

      /* The name may be right-padded with spaces. */
      while (n[strlen(n) - 1] == ' ')
        n[strlen(n) - 1] = '\0';

      dirent_t dent;
      dent.name = n;

      /* Have we seen this cluster before? */
      inode_t *ino = hashtable_get(&fs->known_inos, (void*)data_cluster);
      if (ino) {
        dent.ino = ino;
        ++ino->nlink;
        vector_add(&entries, &dent);
        continue;
      }

      /* FIXME: Use a slab. */
      ino = kmalloc(sizeof(inode_t));

      ino->type = (dir->attributes == ATTR_DIRECTORY) ? it_dir : it_file;
      /* Copy the default permissions given by Linux. */
      ino->mode = (dir->attributes == ATTR_DIRECTORY) ? 040755 : 0100755;
      ino->nlink = 1;
      ino->uid = ino->gid = ino->handles = 0;
      ino->u.dir_cache = 0;
      rwlock_init(&ino->rwlock);
      ino->atime = to_unix_time(dir->adate, 0);
      ino->ctime = to_unix_time(dir->cdate, dir->ctime);
      ino->mtime = to_unix_time(dir->mdate, dir->mtime);

      vfat_file_t *file = make_file_t(data_cluster, node, idx, dir->size);
      ino->data = file;

      ino->size = (dir->attributes == ATTR_DIRECTORY) ?
        cache_dir_size(fs, file) : dir->size;

      dent.ino = ino;
      vector_add(&entries, &dent);

      hashtable_set(&fs->known_inos, (void*)data_cluster, ino);
    }

    offset += sz_read;
  } while (cont && sz_read == 4096);

  vector_destroy(&name);
  kfree(buf);

  return entries;
}

/* Header/Initialisation ***********************/

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

  /** The FAT header differs depending on type - FAT12 and FAT16 have one set of fields, FAT32 has a different set. { */

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

static vfat_filesystem_t *probe(block_device_t *dev) {
  vfat_header_t hdr;
  
  if (!read_bpb(dev, &hdr)) {
    dbg("probe failed: bpb read failed!\n");
    return NULL;
  }

  vfat_filesystem_t *fs = (vfat_filesystem_t*) kmalloc(sizeof(vfat_filesystem_t));

  /* The size of the volume is stored differently depending on FAT type. */
  uint32_t num_sectors = (hdr.num_sectors > 0) ? hdr.num_sectors : hdr.large_num_sectors;
  /* Similarly with the sectors_per_fat variable. */
  uint32_t sectors_per_fat = (hdr.sectors_per_fat16) ? hdr.sectors_per_fat16 :
    hdr.u.h32.sectors_per_fat32;

  uint32_t first_data_sector = hdr.reserved_sectors + (hdr.num_fats * sectors_per_fat);
  uint32_t num_data_sectors = num_sectors - first_data_sector;

  fs->cluster_size = hdr.sectors_per_cluster * 512;
  fs->root_cluster = hdr.u.h32.root_cluster;

  fs->areas[AREA_RESERVED] = 0;
  fs->areas[AREA_FAT]      = hdr.reserved_sectors * 512;
  fs->areas[AREA_DATA]     = first_data_sector * 512;
  fs->areas[AREA_END]      = fs->areas[AREA_DATA] + num_data_sectors * 512;

  /* FIXME: Read free cluster hint from fsinfo struct on FAT32. */
  fs->free_cluster_hint = 0;

  dbg("num_sectors %x sectors_per_fat %x first_data_sector %x\n",
      num_sectors, sectors_per_fat, first_data_sector);
  dbg("num_data_sectors %x cluster_size %x\n",
      num_data_sectors, fs->cluster_size);

  if (fs->cluster_size == 0) {
    dbg("Cluster size was 0, bailing!\n");
    kfree(fs);
    return NULL;
  }

  fs->num_clusters = num_data_sectors / hdr.sectors_per_cluster;
  if (fs->num_clusters < 4085)
    fs->ty = 12;
  else if (fs->num_clusters < 65525)
    fs->ty = 16;
  else
    fs->ty = 32;

  fs->pointers_per_cluster = fs->cluster_size / ( fs->ty == 32 ? 4 : 2 );

  dbg("FAT%d partition detected\n", fs->ty);

  fs->cache = disk_cache_new(disk_cache_group_get_default(), dev);
  fs->cache_ptr = (void*)vmspace_alloc(&kernel_vmspace, 0x1000, 0);
  fs->cached_address = ~0ULL;

  fs->dev = dev;
  fs->known_inos = hashtable_new(257);

  return fs;
}


static int64_t vfat_read(filesystem_t *fs, inode_t *ino, uint64_t offset,
                         void *buf, uint64_t sz) {
  vfat_filesystem_t *vfs = (vfat_filesystem_t*) fs->data;
  return read(vfs, (vfat_file_t*)ino->data, offset, buf, sz);
}

static int64_t vfat_write(filesystem_t *fs, inode_t *ino, uint64_t offset,
                         void *buf, uint64_t sz) {
  vfat_filesystem_t *vfs = (vfat_filesystem_t*) fs->data;
  return write(vfs, (vfat_file_t*)ino->data, offset, buf, sz, true);
}

static vector_t vfat_readdir(filesystem_t *fs, inode_t *dir) {
  assert(dir->type == it_dir && "readdir can only be called on a directory!");
  vfat_filesystem_t *vfs = (vfat_filesystem_t*) fs->data;

  return read_directory(vfs, (vfat_file_t*)dir->data);
}

static void populate_entries_for_lfn(vector_t *entries, const char *name, uint8_t checksum) {
  uint16_t *name16 = kmalloc(strlen(name) * 2 + 1);
  utf8_to_utf16(name16, (uint8_t*)name);

  vector_t entries_reversed = vector_new(sizeof(vfat_lfn_t), 1);

  /* Calculate the size of the string, *including the null terminator!* */
  int sz = 0;
  while (name16[sz++] != 0)
    ;

  int i = 0;
  while (i < sz+1) {
    /* Can store 11 16-bit characters per dirent. */

    /* If we've only got the NULL character left to insert, we don't
       need to create a new dirent for it. */
    if (i == sz) {
      assert(name16[i] == 0 && "Expected U+0000 terminator!");
      break;
    }
    vfat_lfn_t ent;
    memset(&ent, 0, sizeof(vfat_lfn_t));

    ent.attribute = ATTR_LFN;
    ent.checksum = checksum;

    /* Pad with 0xFFFF. */
    unsigned j; 
    for (j = 0; j < 5; ++j)
      ent.name_1[j] = 0xFFFF;
    for (j = 0; j < 6; ++j)
      ent.name_2[j] = 0xFFFF;
    for (j = 0; j < 2; ++j)
      ent.name_3[j] = 0xFFFF;

    for (j = 0; j < 5 && i < sz; ++i,++j)
      ent.name_1[j] = name16[i];
    for (j = 0; j < 6 && i < sz; ++i,++j)
      ent.name_2[j] = name16[i];
    for (j = 0; j < 2 && i < sz; ++i,++j)
      ent.name_3[j] = name16[i];

    vector_add(&entries_reversed, &ent);
  }

  /* The LFN entries are actually written in reverse order. */
  for (i = vector_length(&entries_reversed)-1; i >= 0; --i) {
    vfat_lfn_t *ent = vector_get(&entries_reversed, i);
    ent->order = i+1;
    if (i == 0) ent->order |= 0x40;
    vector_add(entries, ent);
  }

  kfree(name16);
  vector_destroy(&entries_reversed);
}

static void populate_8_11_entry(vfat_dir_t *ent, const char *name) {
  /* Separate out the filename and extension, if applicable. */
  unsigned idx = 0;
  bool found_dot = false;

  /* Pad with spaces. */
  memset(ent->name, (uint8_t)' ', 11);
  
  for (unsigned i = 0; i < strlen(name); ++i) {
    /* Skip over non-ASCII chars. */
    if (name[i] < 0) continue;

    if (idx < 8 && !found_dot)
      ent->name[idx] = name[i];
    else if (idx >= 8 && idx < 11 && found_dot)
      ent->name[idx] = name[i];
    else if (!found_dot && name[i] == '.')
      found_dot = true;
  }
}

static uint8_t calculate_lfn_checksum(vfat_dir_t *ent) {
  uint8_t sum = 0;

  for (unsigned i = 0; i < 11; ++i)
    sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + ent->name[i];

  return sum;
}

static int vfat_mknod(filesystem_t *fs, inode_t *dir_ino, inode_t *dest_ino,
                      const char *name) {
  vfat_filesystem_t *vfs = (vfat_filesystem_t*) fs->data;

  /* FAT filesystems can only contain files and directories. */
  if (dest_ino->type != it_file && dest_ino->type != it_dir) {
    set_errno(EINVAL);
    return -1;
  }

  uint16_t date, time;
  from_unix_time(get_timestamp(), &date, &time);

  vfat_dir_t main_entry;
  memset(&main_entry, 0, sizeof(vfat_dir_t));
  populate_8_11_entry(&main_entry, name);
  
  vector_t entries = vector_new(sizeof(vfat_dir_t), 2);
  populate_entries_for_lfn(&entries, name, calculate_lfn_checksum(&main_entry));

  main_entry.attributes = (dest_ino->type == it_dir) ? ATTR_DIRECTORY : 0;
  main_entry.cdate = main_entry.mdate = main_entry.adate = date;
  main_entry.ctime = main_entry.mtime = time;

  /* The entry (directory or file) must have at least one cluster.
     Find a cluster and set it to all zeroes (so that it is valid as a
     directory). */

  uint32_t cluster = find_free_cluster(vfs);
  if (cluster == ~0U) {
    dbg("find_free_cluster returned failure!");
    set_errno(ENOSPC);
    return -1;
  }

  unsigned char *data = read_cluster(vfs, cluster-2, AREA_DATA);
  memset(data, 0, vfs->cluster_size);

  set_next_cluster(vfs, cluster, eoc_value(vfs));

  main_entry.cluster_lo = cluster & 0xFFFF;
  main_entry.cluster_hi = cluster >> 16;

  vector_add(&entries, &main_entry);

  uint32_t offset = add_to_directory(vfs, (vfat_file_t*) dir_ino->data, &entries);

  vector_destroy(&entries);

  vfat_file_t *file = make_file_t(cluster, (vfat_file_t*) dir_ino->data, offset, 0);
  dest_ino->data = file;

  return 0;
}

static int vfat_get_root(filesystem_t *fs, inode_t *ino) {
  vfat_filesystem_t *vfs = (vfat_filesystem_t*) fs->data;

  uintptr_t cluster = (vfs->ty == 32) ? vfs->root_cluster : 0;

  /* FIXME: How do we know the size of the root cluster? */
  ino->data = make_file_t(cluster, NULL, 0, 512);
  cache_dir_size(vfs, (vfat_file_t*)ino->data);

  ino->type = it_dir;
  ino->mode = 0777;
  ino->nlink = 1;
  ino->uid = ino->gid = ino->handles = 0;
  ino->u.dir_cache = 0;

  hashtable_set(&vfs->known_inos, (void*)cluster, ino);
  hashtable_set(&vfs->known_inos, (void*)0, ino);

  return 0;
}

static void vfat_destroy(filesystem_t *fs) {
  vfat_filesystem_t *vfs = (vfat_filesystem_t*) fs->data;

  if (vfs->cached_address != ~0ULL) {
    disk_cache_release(vfs->cache, vfs->cached_address);
    unmap((uintptr_t)vfs->cache_ptr, 1);
  }

  disk_cache_destroy(vfs->cache);
}

static filesystem_t fs_vfat = {
  .read = &vfat_read,
  .write = &vfat_write,
  .readdir = &vfat_readdir,
  .mknod = &vfat_mknod,
  .get_root = &vfat_get_root,
  .destroy = &vfat_destroy
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
