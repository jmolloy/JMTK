#include "assert.h"
#include "hal.h"
#include "kmalloc.h"
#include "stdio.h"
#include "string.h"
#include "vmspace.h"

#define dbg(args...) kprintf("part: " args)

#define PARTITION_BOOTABLE 0x80
#define PARTITION_EXTENDED1 0xf
#define PARTITION_EXTENDED2 0x5

static const int entry_offsets[4] = {0x1BE, 0x1CE, 0x1DE, 0x1EE};

#define PARTITION_MAGIC1 0x55
#define PARTITION_MAGIC2 0xAA

typedef struct partdesc {
  uint8_t bootable;
  uint8_t s_head;
  uint8_t s_sector;
  uint8_t s_cylinder;
  uint8_t system_id;
  uint8_t e_head;
  uint8_t e_sector;
  uint8_t e_cylinder;
  int32_t lba;
  uint32_t num_sectors;
} partdesc_t;

typedef struct partdata {
  uint64_t offset;
  uint64_t length;
  block_device_t *bdev;
} partdata_t;

static int partition_is_null(partdesc_t *pd) {
  return pd->system_id == 0;
}

static int partition_is_extended(partdesc_t *pd) {
  return pd->system_id == PARTITION_EXTENDED1 || pd->system_id == PARTITION_EXTENDED2;
}

static int part_read(block_device_t *obj, uint64_t offset, void *buf, uint64_t len) {
  partdata_t *pdata = (partdata_t*)obj->data;

  assert(offset+len < pdata->length && "read off end of partition!");
  if (pdata->bdev->read)
    return pdata->bdev->read(pdata->bdev, offset + pdata->offset, buf, len);
  return -1;
}

static int part_write(block_device_t *obj, uint64_t offset, void *buf, uint64_t len) {
  partdata_t *pdata = (partdata_t*)obj->data;

  assert(offset+len < pdata->length && "write off end of partition!");
  if (pdata->bdev->write)
    return pdata->bdev->write(pdata->bdev, offset + pdata->offset, buf, len);
  return -1;
}


static uint64_t part_length(block_device_t *obj) {
  partdata_t *pdata = (partdata_t*)obj->data;
  return pdata->length;
}

static void part_describe(block_device_t *obj, char *buf, unsigned bufsz) {
  partdata_t *pdata = (partdata_t*)obj->data;
  return pdata->bdev->describe(pdata->bdev, buf, bufsz);
}

static void part_flush(block_device_t *obj) {
  partdata_t *pdata = (partdata_t*)obj->data;
  return pdata->bdev->flush(pdata->bdev);
}

static void register_partition(dev_t dev, block_device_t *bdev, partdesc_t *pd, int minor) {
  dev = makedev(major(dev), minor+1);

  block_device_t *bd = (block_device_t*)kmalloc(sizeof(block_device_t));
  partdata_t *pdata = (partdata_t*)kmalloc(sizeof(partdata_t));

  bd->read = &part_read;
  bd->write = &part_write;
  bd->flush = &part_flush;
  bd->length = &part_length;
  bd->describe = &part_describe;
  bd->id = dev;
  bd->data = (void*)pdata;

  pdata->offset = pd->lba * 512;
  pdata->length = pd->num_sectors * 512;
  pdata->bdev = bdev;

  register_block_device(dev, bd);
}

static int probe_extended_partition(dev_t dev, block_device_t *bdev, partdesc_t *pd,
                                    partdesc_t *prev_pd, int idx) {
  uint8_t *sector = (uint8_t*)vmspace_alloc(&kernel_vmspace, 0x1000, 1);

  uint64_t offset = pd->lba * 512;
  if (bdev->length(bdev) <= offset) {
    dbg("bdevice was not large enough! (wanted to access byte %#x)\n", offset);
    return 0;
  }
  dbg("probing for extended partition @ sector %d\n", pd->lba);
  
  int nbytes = bdev->read(bdev, offset, (void*)sector, 4096);
  if (nbytes != 4096) {
    dbg("unable to read from device (read returned %d)\n", nbytes);
    vmspace_free(&kernel_vmspace, 0x1000, (uintptr_t)sector, 1);
    return 0;
  }

  if (sector[510] != PARTITION_MAGIC1 || sector[511] != PARTITION_MAGIC2) {
    dbg("extended partition magic number incorrect!\n");
    vmspace_free(&kernel_vmspace, 0x1000, (uintptr_t)sector, 1);
    return 0;
  }
  
  partdesc_t part, link;

  memcpy((uint8_t*)&part, &sector[entry_offsets[0]], sizeof(partdesc_t));
  memcpy((uint8_t*)&link, &sector[entry_offsets[1]], sizeof(partdesc_t));

  /* The logical partition's start LBA is relative to the current partition
     table's. */
  part.lba += pd->lba;

  /* The link partition's start LBA is relative to the *previous* partition
     table (unless this is the first extended partition. */
  dbg("increasing link lba by %#x from %d to %d\n", ((idx == 0) ? pd->lba : prev_pd->lba), link.lba,   (link.lba + ((idx == 0) ? pd->lba : prev_pd->lba)));
  link.lba += (idx == 0) ? pd->lba : prev_pd->lba;

  dbg("Extended partition %d @ %#x size %dMB type %d\n", idx,
      part.lba,
      (uint32_t) ((uint64_t)part.num_sectors * 512ULL / (1024ULL*1024ULL)),
      part.system_id);

  register_partition(dev, bdev, &part, idx+4);

  ++idx;

  vmspace_free(&kernel_vmspace, 0x1000, (uintptr_t)sector, 1);

  if (partition_is_null(&link))
    return idx;
  return probe_extended_partition(dev, bdev, &link, pd, idx);
}

static int detect_partitions(dev_t dev) {
  int num_extendeds = -1;
  char desc[256];

  block_device_t *bdev = get_block_device(dev);
  assert(bdev);
  
  bdev->describe(bdev, desc, 256);

  dbg("detecting partitions on %s;\n", desc);
  
  if (bdev->length(bdev) <= 512)
    return 0;

  uint8_t *sector = (uint8_t*)vmspace_alloc(&kernel_vmspace, 0x1000, 1);

  int nbytes = bdev->read(bdev, 0, (void*)sector, 4096);
  if (nbytes != 4096) {
    dbg("unable to read from device %s (read returned %d)\n", desc, nbytes);
    vmspace_free(&kernel_vmspace, 0x1000, (uintptr_t)sector, 1);
    return 0;
  }

  if (sector[510] != PARTITION_MAGIC1 || sector[511] != PARTITION_MAGIC2) {
    vmspace_free(&kernel_vmspace, 0x1000, (uintptr_t)sector, 1);
    return 0;
  }

  for (int i = 0; i < 4; ++i) {
    partdesc_t part;
    memcpy((uint8_t*)&part, &sector[entry_offsets[i]], sizeof(partdesc_t));
    dbg("Primary partition %d @ %#x size %dMB type %d\n", i,
        part.lba,
        (uint32_t)((uint64_t)part.num_sectors * 512ULL / (1024ULL*1024ULL)),
        part.system_id);

    if (partition_is_extended(&part))
      num_extendeds = probe_extended_partition(dev, bdev, &part, NULL, 0);
    else if (!partition_is_null(&part))
      register_partition(dev, bdev, &part, i);
  }

  vmspace_free(&kernel_vmspace, 0x1000, (uintptr_t)sector, 1);
  return 1;
}

static int partition() {
  for (int maj = DEV_MAJ_HDA; maj <= DEV_MAJ_SDD; ++maj) {
    dev_t dev = makedev(maj, 0);

    block_device_t *bdev = get_block_device(dev);
    if (bdev)
      detect_partitions(dev);
  }
  return 0;
}

static prereq_t load_after[] = { {"x86/ide",NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "partition",
  .required = NULL,
  .load_after = load_after,
  .init = &partition,
  .fini = NULL
};
