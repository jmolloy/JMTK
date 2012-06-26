#include "hal.h"
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

static int partition_is_null(partdesc_t *pd) {
  return pd->system_id == 0;
}

static int partition_is_extended(partdesc_t *pd) {
  return pd->system_id == PARTITION_EXTENDED1 || pd->system_id == PARTITION_EXTENDED2;
}

static int probe_extended_partition(block_device_t *dev, partdesc_t *pd,
                                    partdesc_t *prev_pd, partdesc_t *extendeds,
                                    int idx) {
  uint8_t *sector = (uint8_t*)vmspace_alloc(&kernel_vmspace, 0x1000, 1);

  uint64_t offset = pd->lba * 512;
  if (dev->length(dev) <= offset) {
    dbg("device was not large enough! (wanted to access byte %#x)\n", offset);
    return 0;
  }

  int nbytes = dev->read(dev, offset, (void*)sector, 4096);
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

  memcpy((uint8_t*)&extendeds[idx], &sector[entry_offsets[0]], sizeof(partdesc_t));

  partdesc_t link;
  memcpy((uint8_t*)&link, &sector[entry_offsets[1]], sizeof(partdesc_t));

  /* The logical partition's start LBA is relative to the current partition
     table. */
  extendeds[idx].lba += pd->lba;

  /* The link partition's start LBA is relative to the *previous* partition
     table (unless this is the first extended partition. */
  link.lba += (idx == 0) ? pd->lba : prev_pd->lba;

  dbg("Extended partition %d @ %#x size %dMB type %d\n", idx,
      extendeds[idx].lba,
      (uint64_t)extendeds[i].num_sectors * 512ULL / (1024ULL*1024ULL),
      extendeds[idx].system_id);

  ++idx;

  vmspace_free(&kernel_vmspace, 0x1000, (uintptr_t)sector, 1);

  if (partition_is_null(&link))
    return idx;
  return probe_extended_partition(dev, &link, pd, extendeds, idx);
}

static int detect_partitions(block_device_t *dev) {
  partdesc_t logicals[4];
  partdesc_t extendeds[16];
  int num_extendeds = -1;
  char desc[256];
  
  dev->describe(dev, desc, 256);

  dbg("detecting partitions on %s;\n", desc);
  
  if (dev->length(dev) <= 512)
    return 0;

  uint8_t *sector = (uint8_t*)vmspace_alloc(&kernel_vmspace, 0x1000, 1);

  int nbytes = dev->read(dev, 0, (void*)sector, 4096);
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
    memcpy((uint8_t*)&logicals[i], &sector[entry_offsets[i]], sizeof(partdesc_t));
    dbg("Logical partition %d @ %#x size %dMB type %d\n", i,
        logicals[i].lba,
        (uint64_t)logicals[i].num_sectors * 512ULL / (1024ULL*1024ULL),
        logicals[i].system_id);

    if (partition_is_extended(&logicals[i]))
      num_extendeds = probe_extended_partition(dev, &logicals[i], NULL, extendeds, 0);
  }

  vmspace_free(&kernel_vmspace, 0x1000, (uintptr_t)sector, 1);
  return 1;
}

static int partition() {
  /* FIXME: Change this to something more dynamic, like an event listener. */
  for (int maj = DEV_MAJ_HDA; maj <= DEV_MAJ_SDD; ++maj) {
    dev_t dev = makedev(maj, 0);

    block_device_t *bdev = get_block_device(dev);
    if (bdev)
      detect_partitions(bdev);
  }
  return 0;
}

static prereq_t load_after[] = { {"ide",NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "partition",
  .required = NULL,
  .load_after = load_after,
  .init = &partition,
  .fini = NULL
};
