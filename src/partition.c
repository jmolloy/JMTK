/**
   Partition tables
   ================

   Now that we're able to read and write to the hard disk, the next thing to do
   is parse the partition table.

   I'm not going to patronise you by explaining what a partition table is - I'm
   assuming that if you're following a walkthrough on writing a UNIX kernel
   you're already au fait with disk partitioning.

   I will however describe the format of the partition table, because it's far
   from simple.

   A partition table is part of the boot sector, which is 512 bytes in
   length. It is always located right at the beginning of a drive.
   
   The boot sector normally contains the first stage bootloader code, a
   partition table and a magic signature at the end (0xAA55). For our purposes
   we don't care about the bootloader - all we want is the partition table. { */

#include "assert.h"
#include "hal.h"
#include "kmalloc.h"
#include "stdio.h"
#include "string.h"
#include "vmspace.h"

#ifdef DEBUG_part
# define dbg(args...) kprintf("part: " args)
#else
# define dbg(args...)
#endif

struct partdesc;
static void register_partition(dev_t dev, block_device_t *bdev,
                               struct partdesc *pd, int minor);


/**
   There are four partition table entries in the bootsector. These are at
   offsets 0x1BE, 0x1CE, 0x1DE and 0x1EE respectively, and are 16 bytes in
   length, with the format in ``struct partdesc``:
   
      * *bootable*: Most significant bit is set if this is a bootable
        partition - this is used by the BIOS and can be ignored by us.
      * *s_head*, *s_sector*, *s_cylinder*: The start address in CHS form.
        We can safely ignore this, as the easier LBA form is given later on.
      * *system_id*: This specifies the filesystem type on the partition. There
        are also several magic values - 0x00, 0x0F and 0x05. More on that later.
      * *e_head*, *e_sector*, *e_cylinder*: End address in CHS form. Ignore this.
      * *lba*: LBA of the start of the partition.
      * *num_sectors*: Number of sectors in the partition.

   In general we only care about the ``system_id``, ``lba`` and ``num_sectors``
   fields of a partition table entry. { */

#define PARTITION_EXTENDED1 0xf
#define PARTITION_EXTENDED2 0x5
#define PARTITION_MAGIC1 0x55
#define PARTITION_MAGIC2 0xAA

#define PARTITION_ENTRY_START 0x1BE

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

/** As said before, there are several values of ``system_id`` that are
    reserved. The value 0x00 indicates a null or unused partition entry. { */

#define PARTITION_IS_NULL(pd) (pd.system_id == 0)

/** There are also values that denote an *extended* partition, which is a
    special partition (read: dirty hack) that enables a disk to have more than 4
    partitions on it. { */

#define PARTITION_IS_EXTENDED(pd) (pd.system_id == 0x0f || pd.system_id == 0x05)

/**
   Extended partitions are slightly special. You can have one and only one
   extended partition in (the main) partition table. It defines a space on disk
   that then contains in turn one or more *logical* partitions.

   So a quick jargon-buster:
   
   *Physical partition*
       One of the four partitions in the boot sector.

   *Extended partition*
       A partition that cannot contain a filesystem itself, but is a container 
       for *logical* partitions.

   *Logical partition*
       Contains a filesystem, is contained by an *extended* partition.

   To make it even clearer, perhaps the diagram below might help...::

      +------------+------------+------------+----------------------------------------+
      | Physical 1 | Physical 2 | Physical 3 | Extended                               |
      +------------+------------+------------++-----------+-+-----------+-+-----------+
                                              | Logical 1 | | Logical 2 | | Logical 3 |
                                              +-----------+ +-----------+ +-----------+

   Note the gaps between the logical partitions. At the start of the extended
   partition is another "boot sector". I put it in quotes because it's not
   really a boot sector - The bootcode is unused and only the first two
   partition table entries are used. The first defines a logical partition, the
   other points to *another* boot sector, like a linked list. When this second
   entry is NULL, all sectors have been found. */

/** Let's get started on some code then. Firstly let's define a function to read
    a boot sector from the disk. Given a block device and an address to read, this
    will read 512 bytes (actually 4096 as this is the IDE driver's minimum request
    size) and if successful store the read partition table entries in
    ``partitions``. { */

static bool read_boot_sector(block_device_t *bdev, uint64_t address,
                             partdesc_t *partitions) {
  uint8_t *sector = (uint8_t*)vmspace_alloc(&kernel_vmspace, 0x1000, 1);

  if (bdev->length(bdev) <= address) {
    dbg("device was not large enough! (wanted to access byte %#x)\n", address);
    return false;
  }
  dbg("probing for extended partition @ sector %d\n", (uint32_t)address / 512);
  
  bool ret = true;

  int nbytes = bdev->read(bdev, address, (void*)sector, 4096);
  if (nbytes != 4096) {
    dbg("unable to read from device (read returned %d)\n", nbytes);
    ret = false;
  } else if (sector[510] != PARTITION_MAGIC1 || sector[511] != PARTITION_MAGIC2) {
    dbg("extended partition magic number incorrect!\n");
    ret = false;
  } else {
    memcpy((uint8_t*) partitions, &sector[PARTITION_ENTRY_START], sizeof(partdesc_t) * 4);
  }

  vmspace_free(&kernel_vmspace, 0x1000, (uintptr_t)sector, 1);
  return ret;
}

/** Now let's use this helper function to detect logical partitions on an
    extended partition. Given a device, a LBA sector address for the fake "boot
    sector", and the LBA sector for the extended partition this logical is contained
    in, find one or more logical partitions. { */

static bool logical_partition(dev_t dev, int64_t lba, int64_t ext_lba,
                              unsigned idx) {
  partdesc_t partitions[4];

  block_device_t *bdev = get_block_device(dev);
  assert(bdev);

  /** We start by noting that the LBA address value in a logical partition table
      entry is *relative* to the LBA of its parent extended partition table. So when
      calculating the address to read, we must add the LBA of the extended
      partition.  { */

  if (!read_boot_sector(bdev, (ext_lba + lba) * 512, partitions))
    return false;

  /** We'll define ``register_partition`` a little later. This takes a partition
      descriptor and will create a block device out of it. What we should note
      though is that the logical partition address is relative, again. But it isn't
      relative to the extended partition start, it's relative to the start of the
      previous logical partition, just to be confusing. { */
  partitions[0].lba += ext_lba + lba;
  register_partition(dev, bdev, &partitions[0], idx + 4);
  
  /** Remember that logical partitions form a linked list. If the "link"
      partition (partition ``1``) is NULL (has a ``system_id`` of 0x00) then the
      list is finished. Else there is another logical partition to find. { */
  if (PARTITION_IS_NULL(partitions[1]))
    return true;
  else
    return logical_partition(dev, partitions[1].lba, ext_lba, idx + 1);
}

/** Now that we can find logical partitions, let's use that to deal properly
    with an extended partition. { */
static bool extended_partition(dev_t dev, int64_t lba) {
  return logical_partition(dev, 0ULL, lba, 0);
}

/** Now that we can deal with extended and logical partitions, let's deal with
    physical partitions! { */
static bool detect_partitions(dev_t dev) {
  char desc[256];
  partdesc_t partitions[4];

  block_device_t *bdev = get_block_device(dev);
  assert(bdev);
  
  bdev->describe(bdev, desc, 256);
  dbg("detecting partitions on %s;\n", desc);
  
  if (!read_boot_sector(bdev, 0ULL, partitions))
    return false;

  for (int i = 0; i < 4; ++i) {
    if (PARTITION_IS_EXTENDED(partitions[i])) {
      if (!extended_partition(dev, partitions[i].lba))
        return false;
    }

    else if (!PARTITION_IS_NULL(partitions[i]))
      register_partition(dev, bdev, &partitions[i], i);
  }

  return true;
}

/** We've written all the code required to traverse the partition table! Now all
    we need to do is write the code to create a "partition" device. This will be a
    block device that just forwards all requests to an underlying block device, with
    an offset applied (and the length of the device modified).

    We'll need a structure to act as the "opaque data" member of a
    ``block_device_t`` that will hold the offset, length and underlying block
    device. { */

typedef struct partdata {
  uint64_t offset;
  uint64_t length;
  block_device_t *bdev;
} partdata_t;


/** Then we produce a set of forwarding functions that just add an offset and
    check no writes go off the end of a partition. { */
static int part_read(block_device_t *obj, uint64_t offset, void *buf,
                     uint64_t len) {
  partdata_t *pdata = (partdata_t*)obj->data;

  assert(offset+len < pdata->length && "read off end of partition!");
  if (pdata->bdev->read)
    return pdata->bdev->read(pdata->bdev, offset + pdata->offset, buf, len);
  return -1;
}

static int part_write(block_device_t *obj, uint64_t offset, void *buf,
                      uint64_t len) {
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

static void part_describe(block_device_t *obj, char *buf,
                          unsigned bufsz) {
  partdata_t *pdata = (partdata_t*)obj->data;
  return pdata->bdev->describe(pdata->bdev, buf, bufsz);
}

static void part_flush(block_device_t *obj) {
  partdata_t *pdata = (partdata_t*)obj->data;
  return pdata->bdev->flush(pdata->bdev);
}

/** With this in place, we can simply define ``register_partition``. We passed
    an integer index into our calls to this function - that will become the minor
    device number (hda0, hda1, hda2, hda3 for the first four partitions, for
    example). { */

static void register_partition(dev_t dev, block_device_t *bdev, partdesc_t *pd,
                               int minor) {
  dev = makedev(major(dev), minor+1);

  kprintf("part: Partition %d @ %#x size %dMB type %d\n", minor,
          pd->lba,
          (uint32_t)((uint64_t)pd->num_sectors * 512ULL / (1024ULL*1024ULL)),
          pd->system_id);

  block_device_t *bd = (block_device_t*) kmalloc(sizeof(block_device_t));
  partdata_t *pdata = (partdata_t*) kmalloc(sizeof(partdata_t));

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

/** Then finally we can pull it all together by iterating through all major
    devices, looking for partition tables! { */

static void part_callback(dev_t dev) {
  if (minor(dev) != 0)
    return;

  block_device_t *bdev = get_block_device(dev);
  if (bdev)
    detect_partitions(dev);
}

static int partition() {
  register_block_device_listener(&part_callback);
  
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
