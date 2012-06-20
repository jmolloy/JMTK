#ifndef X86_IDE_H
#define X86_IDE_H

/* These #defines studiously stolen from wiki.osdev.org/IDE */

/* Bit definitions for the status register (ATA_REG_STATUS) */
#define ATA_SR_BSY     0x80
#define ATA_SR_DRDY    0x40
#define ATA_SR_DF      0x20
#define ATA_SR_DSC     0x10
#define ATA_SR_DRQ     0x08
#define ATA_SR_CORR    0x04
#define ATA_SR_IDX     0x02
#define ATA_SR_ERR     0x01

/* Bit definitions for the error register (ATA_REG_ERROR) */
#define ATA_ER_BBK      0x80
#define ATA_ER_UNC      0x40
#define ATA_ER_MC       0x20
#define ATA_ER_IDNF     0x10
#define ATA_ER_MCR      0x08
#define ATA_ER_ABRT     0x04
#define ATA_ER_TK0NF    0x02
#define ATA_ER_AMNF     0x01

/* Commands for sending to ATA_REG_COMMAND. */
#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

/* ATAPI-specific commands. */
#define ATAPI_CMD_READ       0xA8
#define ATAPI_CMD_EJECT      0x1B

/* Result of ATA IDENTIFY command, as byte offsets into
   the resulting buffer. */
#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

/* Register addresses - these are as offsets from
   the base register address (dev->base). */
#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

/* Register offsets for the PCI bus mastering registers */
#define ATA_BUSMASTER_CMD       0x00
#define ATA_BUSMASTER_STATUS    0x02
#define ATA_BUSMASTER_PRDT_ADDR 0x04

/* Bit definitions for sending to ATA_BUSMASTER_CMD */
#define ATA_BUSMASTER_START 0x01
#define ATA_BUSMASTER_READ  0x08
#define ATA_BUSMASTER_WRITE 0x00

/* Bit definitions for the status register ATA_BUSMASTER_STATUS */
#define ATA_BUSMASTER_IRQ   0x04
#define ATA_BUSMASTER_ERR   0x02

/* Internal flags for the dev_t::flags field. */
#define IDE_FLAG_LBA28     0x01 /* Device supports LBA28 addressing */
#define IDE_FLAG_LBA48     0x02 /* Device supports LBA48 addressing */
#define IDE_FLAG_ATAPI     0x04 /* Device is ATAPI, not ATA */
#define IDE_FLAG_WRITE     0x08 /* Device is currently performing a write */
#define IDE_FLAG_ERROR     0x10 /* Error occurred */
#define IDE_FLAG_OP_IN_PROGRESS 0x20 /* An operation is in progress. */

/* Bit for setting in the PRDT (see documentation below) to indicate
   this is the last entry. */
#define IDE_PRDT_LAST      0x8000

/* An IDE device descriptor. This identifies an ATA or ATAPI device,
   and also contains state for the current read/write operation it
   may be undergoing. */
typedef struct ide_dev {
  /* Buffer storing the results of the IDENTIFY (or IDENTIFY PACKET command) */
  uint8_t identify[512];
  /* Addresses in I/O space of the base and control registers.
     We mainly care about the base registers. */
  uint16_t base, control;
  /* The IRQ this device is attached to. */
  uint16_t irq;
  /* Each bus can support two devices - one will have chip_select as 0, the
     other 1. */
  uint16_t chip_select;
  /* The address in I/O space of the PCI bus mastering registers. */
  uint16_t busmaster;
  /* The number of sectors available on the device (maximum size/512). */
  uint64_t nsectors;
  /* Combination of IDE_FLAG_* flags. */
  unsigned flags;
  /* The PRDT - see ide_prdt_t below. */
  struct ide_prdt *prdt;
  /* State during an operation - the next address on disk to read/write
     as part of a DMA operation. For every entry in the PRDT, we need to
     issue a new ATA command. This tracks what command should be sent. */
  uint64_t next_addr;
  /* The number of ATA commands left to write in this DMA operation.
     Once this reaches zero, the next IRQ signifies the entire operation
     is complete. */
  unsigned n;
  /* At the end of an operation, this semaphore should be signalled. */
  semaphore_t *sema;
  /* Lock for this device's bus. */
  semaphore_t *lock;
} ide_dev_t;

/* The PRDT - Physical Region Descriptor Table.

   This is an array of PRDs (Physical Region Descriptors), which describe
   a subpart of a DMA operation. Each PRDT entry specifies a (contiguous)
   physical memory address and a size.

   We generally set this up so every entry specifies a 4096-byte page.

   The last entry in the table is marked by its resvd field set to
   IDE_PRDT_LAST. */
typedef struct ide_prdt {
  uint32_t addr;
  uint16_t nbytes;
  uint16_t resvd;
} ide_prdt_t;

#endif
