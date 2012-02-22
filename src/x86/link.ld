OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH("i386")
ENTRY(_start)
SECTIONS
{
  .init 0x100000 :
  {
    *(.init)
  }

  . += 0xC0000000;

  .text ALIGN(4096) : AT(ADDR(.text) - 0xC0000000)
  {
    *(.mboot)
    *(.text.unlikely .text.*_unlikely)
    *(.text.exit .text.exit.*)
    *(.text.startup .text.startup.*)
    *(.text.hot .text.hot.*)
    *(.text .stub .text.* .gnu.linkonce.t.*)
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
  }

  .rodata ALIGN(4096) : AT(ADDR(.rodata) - 0xC0000000) {
    *(.rodata .rodata.* .gnu.linkonce.r.*)
  }
  .data ALIGN(4096) : AT(ADDR(.data) - 0xC0000000)
  {
    PROVIDE (__startup_begin = .);
    *(.startup)
    PROVIDE (__startup_end = .);
    PROVIDE (__shutdown_begin = .);
    *(.shutdown)
    PROVIDE (__shutdown_end = .);

    *(.data .data.* .gnu.linkonce.d.*)
    SORT(CONSTRUCTORS)
  }

  .bss ALIGN(4096) : AT(ADDR(.bss) - 0xC0000000)
  {
   *(.dynbss)
   *(.bss .bss.* .gnu.linkonce.b.*)
   *(COMMON)
  }

  .extra 0x0 : {
    *(.note.gnu.gold-version)
  }

  /DISCARD/ : { *(.*) }


}
