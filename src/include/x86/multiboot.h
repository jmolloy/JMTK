#ifndef X86_MULTIBOOT_H
#define X86_MULTIBOOT_H

#define MBOOT_MEM 1<<0
#define MBOOT_BOOT_DEV 1<<1
#define MBOOT_CMDLINE 1<<2
#define MBOOT_MODULES 1<<3
#define MBOOT_ELF_SYMS 1<<5
#define MBOOT_MMAP 1<<6

typedef struct multiboot {
  uint32_t flags;
  
  uint32_t mem_lower;
  uint32_t mem_upper;

  uint32_t boot_device;

  uint32_t cmdline;

  uint32_t mods_count;
  uint32_t mods_addr;

  uint32_t num, size, addr, shndx;

  uint32_t mmap_length, mmap_addr;

} __attribute__((packed)) multiboot_t;

typedef struct multiboot_module_entry {
  uint32_t mod_start;
  uint32_t mod_end;
  uint32_t string;
  uint32_t reserved;
} __attribute__((packed)) multiboot_module_entry_t;


#define MBOOT_IS_MMAP_TYPE_RAM(x) (x == 1)

typedef struct multiboot_mmap_entry {
  /* "size" is at offset -4. */
  uint32_t base_addr;
  uint32_t length;
  uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;

#endif /* X86_MULTIBOOT_H */
