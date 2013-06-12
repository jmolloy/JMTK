#include "assert.h"
#include "hal.h"
#include "kmalloc.h"
#include "stdio.h"
#include "stdlib.h"
#include "vmspace.h"
#include "x86/ide.h"
#include "x86/io.h"
#include "x86/pci.h"

/* FIXME: This implementation only allows one 4k page per table entry due to the physical
   memory manager not being able to dish out more than 4k at once. */
#ifdef DEBUG_ide
#define dbg(args...) kprintf("ide: " args)
#else
#define dbg(args...)
#endif

static void ide_describe(block_device_t *bdev, char *buf, unsigned bufsz) {
  ide_dev_t *dev = (ide_dev_t*)bdev->data;

  const char *model = (const char *)&dev->identify[ATA_IDENT_MODEL];
  if (dev->flags & IDE_FLAG_ATAPI)
    ksnprintf(buf, bufsz, "%s (ATAPI)", model);
  else
    ksnprintf(buf, bufsz, "%s (%dMB)", model, (dev->nsectors/(2*1024)));
}

static uint64_t ide_length(block_device_t *bdev) {
  ide_dev_t *dev = (ide_dev_t*)bdev->data;

  return (uint64_t)dev->nsectors * 512ULL;
}

static void send_chip_select(uint16_t base, uint16_t cs) {
  dbg("send_chip_select(%x, %d)\n", base, cs);

  /* Send device select command. */
  outb(base+ATA_REG_HDDEVSEL, cs ? 0xB0 : 0xA0);

  /* Wait 400ns (approx 4 read/write cycles) for the device to 
     respond to the chip-select. */
  inb(base+ATA_REG_STATUS);
  inb(base+ATA_REG_STATUS);
  inb(base+ATA_REG_STATUS);
  inb(base+ATA_REG_STATUS);
}

static void send_lba_command(ide_dev_t *dev, uint64_t addr, uint8_t sectors,
                             uint8_t cmd28, uint8_t cmd48) {
  dbg("send_lba_command(%x, %x, %x, %d)\n", (uint32_t)addr, cmd28, cmd48, sectors);

  outb(dev->base+ATA_REG_SECCOUNT0, sectors);

  assert((addr & 0x3F) == 0 && "Addr must be a multiple of 512!");
  /* Convert addr into sectors. */
  addr >>= 9;
  if (addr >= (1ULL<<28)) {
    assert((dev->flags & IDE_FLAG_LBA48) && "Device doesn't support LBA48!");
    assert(0 && "LBA48 not implemented!");
  } else {
    assert((dev->flags & IDE_FLAG_LBA28) && "Device doesn't support LBA28!");
    
    uint8_t head = ((addr>>24) & 0x0F) | ((dev->chip_select) << 4)
      | 0xE0;
    outb(dev->base+ATA_REG_HDDEVSEL, head);
    outb(dev->base+ATA_REG_LBA0, addr & 0xFF);
    outb(dev->base+ATA_REG_LBA1, (addr>>8) & 0xFF);
    outb(dev->base+ATA_REG_LBA2, (addr>>16) & 0xFF);

    outb(dev->base+ATA_REG_COMMAND, cmd28);
  }
}

static void dma_setup(ide_dev_t *dev, uintptr_t buf,
                      unsigned size, unsigned write) {
  dbg("dma_setup(%x, %d, %d)\n", buf, size, write);

  assert((size & 0xFFF) == 0 && "DMA read size must be a multiple of 4K!");
  assert((buf & 0xFFF) == 0 && "DMA read buffer must be page aligned!");
  assert(size <= 0x200000 && "DMA size of one operation cannot be > 2MB!");

  /* First, stop DMA transfers */
  outb(dev->busmaster+ATA_BUSMASTER_CMD, 0x00);

  unsigned flags;
  /* Set up the PRDT with descriptors for this operation. */
  unsigned i;
  for (i = 0; i < size; i += 0x1000) {
    uint64_t phys = get_mapping(buf+i, &flags);
    assert(phys != ~0ULL && "Page was not mapped!");
    assert(phys <= 0xFFFFFFFFU &&
           "DMA page must be in lower 4GB of phys memory!");
    dev->prdt[i>>12].addr = phys;
    dev->prdt[i>>12].nbytes = 4096;
    dev->prdt[i>>12].resvd = 0;
  }
  dev->prdt[(i-0x1000)>>12].resvd |= IDE_PRDT_LAST;

  /* Ensure interrupts are enabled. */
  /* FIXME: add #defines for this. */
  outb(dev->control+6, 0x08);

  /* Set the PRDT address. */
  uint64_t phys = get_mapping((uintptr_t)dev->prdt, &flags);
  assert(phys != ~0ULL && "Page was not mapped!");
  assert(phys <= 0xFFFFFFFFU && "DMA page must be in lower 4GB of phys memory!");
  dev->flags |= IDE_FLAG_OP_IN_PROGRESS;
  outl(dev->busmaster+ATA_BUSMASTER_PRDT_ADDR, (uint32_t)phys);

  outb(dev->busmaster+ATA_BUSMASTER_CMD, ATA_BUSMASTER_START |
       ((write) ? ATA_BUSMASTER_WRITE : ATA_BUSMASTER_READ));
}

static void dma_start_read(ide_dev_t *dev, uintptr_t buf,
                           unsigned size, uint64_t address,
                           semaphore_t *sema) {
  dbg("dma_start_read(%x, %x, %d, %x)\n", dev, buf, size, (uint32_t)address);

  send_chip_select(dev->base, dev->chip_select);

  send_lba_command(dev, address, size/512, ATA_CMD_READ_DMA, ATA_CMD_READ_DMA_EXT);

  dev->next_addr = address+4096;
  dev->n = size/4096 - 1;
  dev->sema = sema;
  dev->flags &= ~IDE_FLAG_WRITE;
  dev->flags &= ~IDE_FLAG_ERROR;

  dma_setup(dev, buf, size, 0);
}

static void dma_start_write(ide_dev_t *dev, uintptr_t buf,
                            unsigned size, uint64_t address,
                            semaphore_t *sema) {
  dbg("dma_start_write(%x, %d, %x)\n", buf, size, (uint32_t)address);

  send_chip_select(dev->base, dev->chip_select);

  send_lba_command(dev, address, size/512, ATA_CMD_WRITE_DMA, ATA_CMD_WRITE_DMA_EXT);

  dev->next_addr = address+4096;
  dev->n = size/4096 - 1;
  dev->sema = sema;
  dev->flags |= IDE_FLAG_WRITE;
  dev->flags &= ~IDE_FLAG_ERROR;

  dma_setup(dev, buf, size, 1);
}

static int ide_read(block_device_t *bdev, uint64_t offset, void *buf, uint64_t len) {
  dbg("ide_read(%x, %x, %x)\n", (uint32_t)offset, buf, (uint32_t)len);

  uintptr_t bufp = (uintptr_t)buf;

  //  assert((offset & 0xFFF) == 0 && "Read length must be a multiple of 4096!");
  assert((len & 0xFFF) == 0 && "Read length must be a multiple of 4096!");
  assert((bufp & 0xFFF) == 0 && "Buffer must be a multiple of 4096!");

  ide_dev_t *dev = (ide_dev_t*)bdev->data;

  assert((dev->flags & IDE_FLAG_ATAPI) == 0 && "ATAPI reads not supported yet!");

  semaphore_t sema;
  semaphore_init(&sema);

  semaphore_wait(dev->lock);

  dev->sema = &sema;

  dma_start_read(dev, bufp, len, offset, &sema);

  semaphore_wait(&sema);
  unsigned error = dev->flags & IDE_FLAG_ERROR;

  kprintf("ERROR: %d, buf[0] = %x\n", error, *(unsigned int*)buf);

  semaphore_signal(dev->lock);

  return error ? -1 : (int)len;
}

static int ide_write(block_device_t *bdev, uint64_t offset, void *buf, uint64_t len) {
  dbg("ide_write(%x, %x, %x)\n", (uint32_t)offset, buf, (uint32_t)len);

  uintptr_t bufp = (uintptr_t)buf;

  //  assert((offset & 0xFFF)0 && "Write offset must be a multiple of 4096!");
  assert((len & 0xFFF) == 0 && "Write length must be a multiple of 4096!");
  assert((bufp & 0xFFF) == 0 && "Buffer must be a multiple of 4096!");

  ide_dev_t *dev = (ide_dev_t*)bdev->data;

  assert((dev->flags & IDE_FLAG_ATAPI) == 0 && "Can't write to an ATAPI device!");

  semaphore_t sema;
  semaphore_init(&sema);

  semaphore_wait(dev->lock);

  dev->sema = &sema;

  dma_start_write(dev, bufp, len, offset, &sema);

  semaphore_wait(&sema);
  unsigned error = dev->flags & IDE_FLAG_ERROR;

  semaphore_signal(dev->lock);

  return error ? -1 : (int)len;
}

static int dma_handle_irq(struct regs *r, void *p) {
  ide_dev_t *dev = (ide_dev_t*)p;
  /* First, check if it was this device that caused the IRQ by checking
     the status bit. */
  uint8_t status = inb(dev->busmaster+ATA_BUSMASTER_STATUS);
  if ((status & ATA_BUSMASTER_IRQ) == 0)
    return 0;

  dbg("dma_handle_irq: status=%x\n", status);

  /* Acknowledge IRQ, resetting the IRQ flag. */
  outb(dev->busmaster+ATA_BUSMASTER_STATUS, ATA_BUSMASTER_IRQ);

  /* Second, check if an operation is actually in progress. */
  if ((dev->flags & IDE_FLAG_OP_IN_PROGRESS) == 0)
    return 0;

  dbg("dma_handle_irq: was intended for this device.\n");

  if (status & ATA_BUSMASTER_ERR) {
    dbg("dma_handle_irq: error!\n");
    /* An error occurred :( */
    dev->flags = IDE_FLAG_ERROR;

    /* Operation complete - abort. */
    outb(dev->busmaster+ATA_BUSMASTER_CMD, 0);
    dev->flags &= ~IDE_FLAG_OP_IN_PROGRESS;

    semaphore_signal(dev->sema);

    return 0;
  }

  if (dev->n == 0) {
    /* Operation complete. */
    outb(dev->busmaster+ATA_BUSMASTER_CMD, 0);
    dev->flags &= ~IDE_FLAG_OP_IN_PROGRESS;
    semaphore_signal(dev->sema);

    return 0;
  }

  /* Send the next address to read. */
  send_lba_command(dev, dev->next_addr, 4096/512,
                   (dev->flags & IDE_FLAG_WRITE) ?
                   ATA_CMD_WRITE_DMA : ATA_CMD_READ_DMA,
                   (dev->flags & IDE_FLAG_WRITE) ?
                   ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT);
  dev->next_addr += 4096;
  dev->n--;

  return 0;
}

/* Probes a potential ATA[PI] device, sending an IDENTIFY packet.

   If the device responds, a new ide_dev_t and block_device_t structure
   is created, and returned. Else, NULL is returned. */
static block_device_t *probe_dev(uint16_t base, uint16_t control, uint16_t irq,
                                 uint16_t chip_select, uint16_t busmaster,
                                 semaphore_t *bus_lock) {
  /* First, select the device we want to access. */
  send_chip_select(base, chip_select);

  /* Set the sector count, and all lba registers to zero. */
  outb(base+ATA_REG_SECCOUNT0, 0);
  outb(base+ATA_REG_LBA0, 0);
  outb(base+ATA_REG_LBA1, 0);
  outb(base+ATA_REG_LBA2, 0);

  /* Send the identify command. */
  outb(base+ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

  /* If the status register is zero, this isn't an ATA device. */
  if (inb(base+ATA_REG_STATUS) == 0)
    return NULL;

  /* Poll until the device is no longer busy. */
  while (inb(base+ATA_REG_STATUS) & ATA_SR_BSY)
    ;

  unsigned flags = 0;

  /* Check if the device is ATAPI or SATA. */
  uint8_t lba1 = inb(base+ATA_REG_LBA1);
  uint8_t lba2 = inb(base+ATA_REG_LBA2);
  if (lba1 || lba2) {
    if (lba1 == 0x14 && lba2 == 0xEB) {
      flags |= IDE_FLAG_ATAPI;

      /* Perform the ATAPI IDENTIFY PACKET command instead. */
      outb(base+ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
    }
    else if (lba1 == 0x3C && lba2 == 0xC3) {
      /* SATA is complex - unimplemented :( */
      kprintf("ide: SATA device detected - not supported!\n");
      return NULL;
    }
    /* Else bad device. */
    else return NULL;
  }

  /* Continue polling until either the DRQ or ERR bits are set */
  while ((inb(base+ATA_REG_STATUS) & (ATA_SR_DRQ|ATA_SR_ERR)) == 0)
    ;

  if (inb(base+ATA_REG_STATUS) & ATA_SR_ERR) {
    kprintf("ide: Error sending IDENTIFY packet!\n");
    return NULL;
  }

  ide_dev_t *dev = kmalloc(sizeof(ide_dev_t));
  for (unsigned i = 0; i < 256; ++i) {
    uint16_t w = inw(base+ATA_REG_DATA);

    /* The textual fields (serial and model description) need to be
       endian-reversed. The rest of the fields are fine in little-endian
       form. */
    if (i*2 >= ATA_IDENT_SERIAL  && i*2 < ATA_IDENT_CAPABILITIES) {
      dev->identify[i*2+0] = (w>>8) & 0xFF;
      dev->identify[i*2+1] = w & 0xFF;
    } else {
      dev->identify[i*2+0] = w & 0xFF;
      dev->identify[i*2+1] = (w>>8) & 0xFF;
    }
  }

  if (inb(base+ATA_REG_STATUS) & ATA_SR_ERR) {
    kprintf("ide: Error after sending IDENTIFY packet!\n");
    kfree(dev);
    return NULL;
  }

  /* Bytes 200-208 contain the number of LBA48 addressable sectors. */
  uint64_t lba48 = *(uint64_t*) &dev->identify[200];
  /* Bytes 120-124 contain the number of LBA28 addressable sectors. */
  uint32_t lba28 = *(uint32_t*) &dev->identify[120];

  if (lba28 != 0 && lba48 != 0) {
    flags |= IDE_FLAG_LBA28 | IDE_FLAG_LBA48;
    dev->nsectors = lba48;
  } else if (lba28 != 0) {
    flags |= IDE_FLAG_LBA28;
    dev->nsectors = lba28;
  } else if ((flags & IDE_FLAG_ATAPI) == 0) {
    assert(0 && "CHS sector addressing not supported!");
  } 

  /* FIXME: As we cannot yet read from ATAPI devices, bail out here. */
  if (flags & IDE_FLAG_ATAPI) {
    kfree(dev);
    return NULL;
  }

  dev->flags = flags;
  dev->base = base;
  dev->control = control;
  dev->irq = irq;
  dev->chip_select = chip_select;
  dev->busmaster = busmaster;
  dev->lock = bus_lock;
  dev->prdt = (ide_prdt_t*)vmspace_alloc(&kernel_vmspace, 0x1000,
                                         /*alloc_phys=*/1);

  block_device_t *bdev = kmalloc(sizeof(block_device_t));
  bdev->read = &ide_read;
  bdev->write = &ide_write;
  bdev->flush = NULL;
  bdev->length = &ide_length;
  bdev->describe = &ide_describe;
  bdev->data = (void*)dev;

  register_interrupt_handler(IRQ(dev->irq), &dma_handle_irq, (void*)dev);

  return bdev;
}

/* Scans the PCI bus looking for potential ATA/IDE devices.

   This code is less precise than most, because it has to deal with legacy
   devices and the documentation about how to deal with them appears to be
   spread about the internet. */
int ide_init() {
  enable_interrupts();

  for (pci_dev_t **devs = pci_get_devices(); *devs != NULL; ++devs) {
    pci_dev_t *dev = *devs;

    if (dev->header.class != PCI_CLASS_MASS_STORAGE ||
        dev->header.subclass != PCI_SUBCLASS_IDE)
      continue;

    unsigned major_base = DEV_MAJ_SDA;

    uint16_t pri_base    = dev->header.u.h00.bar[0] & 0xFFFC;
    uint16_t pri_control = dev->header.u.h00.bar[1] & 0xFFFC;
    uint16_t sec_base    = dev->header.u.h00.bar[2] & 0xFFFC;
    uint16_t sec_control = dev->header.u.h00.bar[3] & 0xFFFC;
    uint16_t busmaster   = dev->header.u.h00.bar[4] & 0xFFFC;

    uint16_t pri_irq = dev->header.u.h00.interrupt_line;
    uint16_t sec_irq = dev->header.u.h00.interrupt_line;

    semaphore_t *pri_bus_lock = semaphore_new();
    semaphore_signal(pri_bus_lock);
    semaphore_t *sec_bus_lock = semaphore_new();
    semaphore_signal(sec_bus_lock);
    
    /* If the ProgIF (programming interface) field is 0x80 or
       0x8A, the device is a legacy ISA bridge and so the IRQs and
       base addresses are standardised - we should ignore any value
       in the BARs. */
    if (dev->header.prog_if == 0x80 || dev->header.prog_if == 0x8A) {
      pri_base = 0x1F0;
      pri_control = 0x3F4;
      sec_base = 0x170;
      sec_control = 0x374;
      pri_irq = 14;
      sec_irq = 15;
      major_base = DEV_MAJ_HDA;
    }

    block_device_t *bdev;

    /* Primary channel, first device. */
    if ((bdev = probe_dev(pri_base, pri_control, pri_irq, 0, busmaster,
                          pri_bus_lock))) {
      register_block_device(makedev(major_base+0, 0), bdev);
    }

    if ((bdev = probe_dev(pri_base, pri_control, pri_irq, 1, busmaster,
                          pri_bus_lock))) {
      /* Primary channel, second device. */
      register_block_device(makedev(major_base+1, 0), bdev);
    }

    if ((bdev = probe_dev(sec_base, sec_control, sec_irq, 0, busmaster+8,
                          sec_bus_lock))) {
      /* Secondary channel, first device. */
      register_block_device(makedev(major_base+2, 0), bdev);
    }

    if ((bdev = probe_dev(sec_base, sec_control, sec_irq, 1, busmaster+8,
                          sec_bus_lock))) {
      /* Secondary channel, second device. */
      register_block_device(makedev(major_base+3, 0), bdev);
    }
  }
  return 0;
}

static prereq_t prereqs[] = { {"x86/pci",NULL}, {"threading",NULL},
                              {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "x86/ide",
  .required = prereqs,
  .load_after = NULL,
  .init = &ide_init,
  .fini = NULL
};
