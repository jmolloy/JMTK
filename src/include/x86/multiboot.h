#ifndef X86_MULTIBOOT_H
#define X86_MULTIBOOT_H

/**#3
   Now we come on to getting information out of the bootloader, and that brings
   us on to defining the *multiboot info struct*.

   You'll recall that we passed a set of flags to the bootloader to tell it to
   do stuff for us (just pass a memory map, in our case) - well there is a
   similar set of flags that the bootloader will pass to us to tell us exactly
   what it *did*. Here's the flag definitions and the info struct definition;
   the fields are as such:

   ``flags``
     Consists of a logical-OR of the ``MBOOT_*`` constants, describing which
     parts of the structure are actually valid.

   ``mem_lower``, ``mem_upper``
     The address of the first usable memory address in lower (< 1MB) and upper
     (>= 1MB) memory. If ``flags & MBOOT_MMAP`` is nonzero, there is better
     information in the ``mmap_*`` fields.

   ``cmdline``
     Valid if ``flags & MBOOT_CMDLINE`` is nonzero. This contains the address of
     a NUL-terminated string containing the command line given to the
     bootloader. This can contain arguments for the kernel (and we will use as
     our argc/argv to pass to ``main``).

   ``mods_count``, ``mods_addr``
     Valid if ``flags & MBOOT_MODULES`` is nonzero - this points to a list of
     kernel modules loaded via the "module" command in GRUB.

   ``num``, ``size``, ``addr``, ``shndx``
     Valid if ``flags & MBOOT_ELF_SYMS`` is nonzero, these describe the
     location and size of the ELF symbol table that the bootloader has
     loaded. ``addr`` refers to the address of an array of ``num`` ELF section headers,
     each of which is ``size`` large, and of which the ``shndx``'th is the
     section header that describes the ``.shstrtab`` section, which is required
     to identify other sections. We'll come on to this a bit more in a later
     chapter, on debugging.

   ``mmap_length``, ``mmap_addr``
     Valid if ``flags & MBOOT_MMAP`` is nonzero, ``mmap_addr`` points to an
     array of structures that describe how the physical memory space is layed
     out - in particular this can tell you where exactly RAM (which has type
     '1') is located, because it's often not contiguous from zero, as you might
     have expected! { */

#define MBOOT_MEM      (1<<0)
#define MBOOT_BOOT_DEV (1<<1)
#define MBOOT_CMDLINE  (1<<2)
#define MBOOT_MODULES  (1<<3)
#define MBOOT_ELF_SYMS (1<<5)
#define MBOOT_MMAP     (1<<6)

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

#define MBOOT_IS_MMAP_TYPE_RAM(x) (x == 1)

typedef struct multiboot_mmap_entry {
  uint32_t size;
  uint64_t base_addr;
  uint64_t length;
  uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry_t;

typedef struct multiboot_module_entry {
  uint32_t mod_start;
  uint32_t mod_end;
  uint32_t string;
  uint32_t reserved;
} __attribute__((packed)) multiboot_module_entry_t;

#endif /* X86_MULTIBOOT_H */
