#include "hal.h"
#include "vfs.h"



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
  uint16_t sectors_per_fat;
  uint16_t sector_per_track;
  uint16_t num_heads;
  uint32_t hidden_sectors;
  uint32_t large_num_sectors;

  union {
    struct vfat_16_header {
      uint8_t  drive_number;
      uint8_t  flags;
      uint8_t  signature;
      uint32_t volume_id;
      char     volume_label[11];
      char     system_ident[8];
      uint8_t  bootcode[448];
      uint16_t signature;       /* Should be 0xAA55 */
    } __attribute__((packed)) h16;

    struct vfat_32_header {
      uint32_t sectors_per_fat;
      uint16_t flags;
      uint16_t version;
      uint32_t root_cluster;
      uint16_t fsinfo_cluster;
      uint16_t backup_boot_sector;
      uint8_t  reserved[12];
      uint8_t  drive_number;
      uint8_t  flags;
      uint8_t  signature;
      uint32_t volume_id;
      char     volume_label[11];
      char     system_ident[8];
      uint8_t  bootcode[420];
      uint16_t signature;       /* Should be 0xAA55 */
    } __attribute__((packed)) h32;
  } u;

} __attribute__((packed)) vfat_header_t;

static uint16_t follow_chain_16(filesystem_t *fs, vfat_header_t *hdr, unsigned cluster) {
  unsigned offset = cluster * 2;
  unsigend cluster_size = hdr.sectors_per_cluster / hdr.bytes_per_sector;
  unsigned sector = hdr.reserved_sectors + (offset / cluster_size);
  unsigned idx = offset % cluster_size;

  unsigned char *data = get_sector(fs, sector);

  return * (uint16_t) &data[idx];
}

static uint16_t follow_chain_32(filesystem_t *fs, vfat_header_t *hdr, unsigned cluster) {
  unsigned offset = cluster * 4;
  unsigend cluster_size = hdr.sectors_per_cluster / hdr.bytes_per_sector;
  unsigned sector = hdr.reserved_sectors + (offset / cluster_size);
  unsigned idx = offset % cluster_size;

  unsigned char *data = get_sector(fs, sector);

  return (* (uint32_t) &data[idx]) & 0x0FFFFFFF;
}

static int64_t vfat_read(filesystem_t *fs, void *node_data, uint64_t offset,
                         void *buf, uint64_t sz) {
  assert(0 && "Not implemented!");
}

static int64_t vfat_write(filesystem_t *fs, void *node_data, uint64_t offset,
                          void *buf, uint64_t sz) {
  assert(0 && "Not implemented!");
}

static unsigned vfat_num_dir_entries(filesystem_t *fs, void *node_data, unsigned n) {
  assert(0 && "Not implemented!");
}

static const char *vfat_read_dir_entry_name(filesystem_t *fs, void *node_data,
                                            unsigned n) {
  assert(0 && "Not implemented!");
}

static int vfat_fill_dir_entry(filesystem_t *fs, void *node_data, unsigned n,
                               inode_t *inode) {
  assert(0 && "Not implemented!");
}

static int mknod(filesystem_t *fs, void *node_data, inode_t *inode) {
  assert(0 && "Not implemented!");
}

static filesystem_t fs_vfat = {
  .read = &vfat_read,
  .write = &vfat_write,
  .num_dir_entries = &vfat_num_dir_entries,
  .read_dir_entry_name = &vfat_read_dir_entry_name,
  .fill_dir_entry = &vfat_fill_dir_entry,
  .mknod = &vfat_mknod
};
