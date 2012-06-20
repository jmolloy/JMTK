#include "hal.h"
#include "stdio.h"
#include "kmalloc.h"
#include "x86/io.h"
#include "x86/pci.h"

#include "pci_ids.h"

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA    0xCFC

/*
31     30    24 23 16 15   11 10      8 7       2 10
Enable Reserved Bus # Device# Function# Register# 00
*/

#define ENABLE_BIT (1U << 31)
#define BUS_M      0xFF
#define BUS_S      16
#define DEV_M      0x1F
#define DEV_S      11
#define FN_M       0x7
#define FN_S       8
#define REG_M      0x3F
#define REG_S      2

/* Set in the header type field if the device has multiple functions. */
#define HEADER_TYPE_MF 0x80

#define NUM_CLASS_CODE_STRS 18
const char *class_code_strs[NUM_CLASS_CODE_STRS] = {
  "Very old device",
  "Mass storage controller",
  "Network controller",
  "Display controller",
  "Multimedia controller",
  "Memory controller",
  "Bridge device",
  "Simple communication controller",
  "Base system peripheral",
  "Input device",
  "Docking station",
  "Processor",
  "Serial bus controller",
  "Wireless controller",
  "Intelligent I/O controller",
  "Satellite communication controller",
  "Encryption/Decryption controller",
  "Data acquisition o signal processing controller",
};

/* Reads a 32-bit value from the PCI configuration space. */
static uint32_t pci_read32(unsigned bus, unsigned dev, unsigned fn, unsigned reg) {
  uint32_t addr = ((bus&BUS_M) << BUS_S) |
    ((dev&DEV_M) << DEV_S) |
    ((fn&FN_M) << FN_S) |
    ((reg&REG_M) << REG_S) |
    ENABLE_BIT;

  outl(CONFIG_ADDRESS, addr);
  return inl(CONFIG_DATA);
}

#define MAX_PCI_DEVICES 64
static pci_dev_t *devices[MAX_PCI_DEVICES] = {NULL};
static unsigned num_devices = 0;

static const char *get_vendor_name(uint16_t id, unsigned verbose) {
  for (unsigned i = 0; i < PCI_VENTABLE_LEN; ++i) {
    if (PciVenTable[i].VenId == id)
      return verbose ? PciVenTable[i].VenFull : PciVenTable[i].VenShort;
  }
  return NULL;
}

static const char *get_device_name(uint16_t vendor_id, uint16_t id,
                                   unsigned verbose) {
  for (unsigned i = 0; i < PCI_DEVTABLE_LEN; ++i) {
    if (PciDevTable[i].VenId == vendor_id &&
        PciDevTable[i].DevId == id)
      return verbose ? PciDevTable[i].ChipDesc : PciDevTable[i].Chip;
  }
  return NULL;
}

static const char *get_class_code(uint8_t class) {
  if (class >= NUM_CLASS_CODE_STRS)
    return NULL;
  return class_code_strs[class];
}

static void print_device_brief(pci_header_t *h) {
  kprintf("0x%04x:0x%04x:%s: %s %s\n", h->vendor_id, h->device_id,
          get_class_code(h->class),
          get_vendor_name(h->vendor_id, 1),
          get_device_name(h->vendor_id, h->device_id, 1));
}

void pci_print_device(pci_dev_t *d) {
  kprintf("%02x:%02x:%02x - %04x:%04x\n", d->bus_id, d->dev_id, d->fn_id,
          d->header.vendor_id, d->header.device_id);
  kprintf("class %x subclass %x progIF %x int_line %x int_pin %x\n",
          d->header.class, d->header.subclass, d->header.prog_if,
          d->header.u.h00.interrupt_line, d->header.u.h00.interrupt_pin);
  for (unsigned i = 0; i < 6 ; ++i)
    kprintf("BAR%d: %08x\n", i, d->header.u.h00.bar[i]);
}

pci_dev_t **pci_get_devices() {
  return devices;
}

static pci_dev_t *pci_probe(unsigned bus, unsigned dev, unsigned fn) {
  if (pci_read32(bus, dev, fn, 0) == 0xFFFFFFFF)
    return NULL;

  pci_dev_t *d = kmalloc(sizeof(pci_header_t));

  d->bus_id = bus;
  d->dev_id = dev;
  d->fn_id = fn;
  uint32_t *h32 = (uint32_t*)&d->header;
  for (unsigned i = 0; i < 0x10; ++i)
    h32[i] = pci_read32(bus, dev, fn, i);

  devices[num_devices++] = d;
  return d;
}

static int pci_init() {

  /* Scan PCI buses. */
  for (unsigned bus = 0; bus < 256; ++bus) {
    for (unsigned dev = 0; dev < 32; ++dev) {
      pci_dev_t *d = pci_probe(bus, dev, 0);
      if (!d) continue;
      print_device_brief(&d->header);

      if (d->header.header_type & HEADER_TYPE_MF) {
        for (unsigned fn = 1; fn < 8; ++fn) {
          d = pci_probe(bus, dev, fn);
          if (!d) continue;
          print_device_brief(&d->header);
        }
      }
    }
  }

  return 0;
}

static prereq_t prereqs[] = { {"kmalloc",NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "x86/pci",
  .required = prereqs,
  .load_after = NULL,
  .init = &pci_init,
  .fini = NULL
};
