#ifndef X86_PCI_H
#define X86_PCI_H

#define PCI_CLASS_MASS_STORAGE 0x01

#define PCI_SUBCLASS_IDE  0x01

/* Header structure for a device with header type 0x00, which is most
   devices. */
typedef struct pci_header_00 {
  uint32_t bar[6];
  uint32_t cardbus_cis_ptr;
  uint16_t subsys_vendor_id, subsys_id;
  uint32_t expansion_rom_addr;
  uint8_t  capabilities, resvd1[3];
  uint32_t resvd2;
  uint8_t  interrupt_line, interrupt_pin, min_grant, max_latency;
} pci_header_00_t;

/* Header structure for a device with header type 0x01, which is
   generally a PCI-to-PCI bridge */
typedef struct pci_header_01 {
  uint32_t bar[2];
  uint8_t  pri_bus_num, sec_bus_num, sub_bus_num, secondary_latency_timer;
  uint8_t  io_base, io_limit;
  uint16_t sec_status;
  uint16_t memory_base, memory_limit;
  uint16_t prefetch_memory_base, prefetch_memory_limit;
  uint32_t prefetchable_base_hi32;
  uint32_t prefetchable_base_lo32;
  uint16_t io_limit_lo16, io_limit_hi16;
  uint8_t  capabilities, resvd[3];
  uint32_t expansion_rom_addr;
  uint8_t  interrupt_line, interrupt_pin;
  uint16_t bridge_ctl;
} pci_header_01_t;

/* Header structure for a PCI device - common to all header types. */
typedef struct pci_header {
  uint16_t vendor_id, device_id;
  uint16_t command, status;
  uint8_t  revision_id, prog_if, subclass, class;
  uint8_t  cache_line_sz, latency_timer, header_type, bist;
  union {
    struct pci_header_00 h00;
    struct pci_header_01 h01;
  } u;
} pci_header_t;

/* A PCI device descriptor. */
typedef struct pci_dev {
  uint16_t bus_id, dev_id, fn_id;
  pci_header_t header;
} pci_dev_t;

/* Returns a NULL-terminated array of PCI devices detected. */
pci_dev_t **pci_get_devices();

/* Prints out a PCI device descriptor in detail. */
void pci_print_device(pci_dev_t *d);

#endif
