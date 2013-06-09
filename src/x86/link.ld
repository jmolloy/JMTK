/**#5
   You might think that we're done already, but we have yet to get our kernel to build!

   The next thing we have to do is tell the linker how to link our
   kernel. Normal application programs under linux have a lot of magic done by
   the linker - we want to stop all that happening and also do some magic of our
   own. For this, we need a linker script (\*.ld).

   Note that the format of a linker script is not the nicest in the world, so 
   I'm going to assume that you either don't care about the syntax or can sort
   of pick it up as you go along. Firstly we have to inform the linker of our
   target, which is 32-bit x86. {*/
OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH("i386")
ENTRY(_start)

/**
   Then we define how to map sections. A section is an ELF concept, and is a
   chunk of code or data. It can have a name, a physical location and a virtual
   location - that is, you can instruct the linker to create an ELF section that
   will be loaded at one address but linked as if it were at another
   address. We'll need that functionality for the higher half part of our kernel.

   Firstly we want to create the ``.init`` section, which will be linked and
   loaded at 1MB, and the special ``.init.bss`` section which contains some of
   the magic in the bringup process that has yet to be fully described. { */
SECTIONS
{
  .init 0x100000 :
  {
    PROVIDE(__start = .);
    *(.init)
  }
  .init.bss ALIGN(4096) :
  {
    *(.init.bss)
  }

  /**
     Now that the lower half stuff has been defined, we add 3GB to the link
     address (``.``) so that everything else will be linked in the higher half. { */
  . += 0xC0000000;

  /**
     All other sections (``.text``, which contains the program code,
     ``.rodata`` which contains read-only data, and ``.data`` which contains
     mutable data) should be aligned on a page boundary, and loaded at the current
     link address (which will be >3GB) minus 3GB so that they are loaded to
     addresses in the lower half. { */
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

    PROVIDE (__ctors_begin = .);
    *(.ctors)
    PROVIDE (__ctors_end = .);

    /* Hack to silence warning when using GNU Gold as the linker. */
    *(.note.gnu.gold-version)
    *(.note.gnu.build-id)
  }

  .bss ALIGN(4096) : AT(ADDR(.bss) - 0xC0000000)
  {
   *(.dynbss)
   *(.bss .bss.* .gnu.linkonce.b.*)
   *(COMMON)
    PROVIDE(__end = . - 0xC0000000);
  }

  /* Get rid of all other sections. */
  /DISCARD/ : { *(.*) }

}

/** Now we *are* actually done! we need to compile the kernel with some special
    command line options however::

        -nostdlibinc -fno-builtin -DX86 -m32 -ffreestanding 

    ``-nostdlibinc``
      This causes the compiler not to implicitly allow includes of files from
      the C standard library. Some tutorials recommend ``-nostdinc`` -
      ``-nostdlibinc`` is better as this removes the problems surrounding
      including C library headers but *still allows* headers required by the C
      standard and defined by the compiler - notably ``<stdarg.h>`` and
      ``<stdint.h>``, which allow us later to use ``uintptr_t`` and ``size_t``.

    ``-fno-builtin``
      This stops the compiler from inserting calls to ``memcpy()`` and
      ``memset()`` when it feels it should - it can't do this because we don't
      have a version of memcpy yet! :(

    ``-DX86``
      Sets the preprocessor define ``#define X86``, which we use in several
      places to detect the target architecture.

    ``-m32``
      Forces compilation for 32-bit mode, which is required on a 64-bit system
      which will default to 64-bit.

    ``-ffreestanding``
      ``#undef`` s ``STDC_HOSTED`` and a variety of other ``#define`` s that
      standard headers use to detect if you are on a system with an OS
      underneath. We're not ;)

    We require some options for the linker too::

        -Tsrc/x86/link.ld -lgcc -n

    ``-Tsrc/x86/link.ld``
      This tells the linker to use the linker script "src/x86/link.ld" that we
      just defined instead of its default.

    ``-lgcc``
      Forces the linking in of libgcc, which contains definitions for 64-bit
      arithmetic and other libcalls that the compiler relies on.

    ``-n``
      Works around a "feature" of GNU Gold. If you're really interested, see the
      comment in the Makefile.

    This should now compile and link the kernel. All that's left is to squirt
    the built ELF image into a floppy disc image with GRUB preinstalled. This is
    all handled by the kernel's build system, so you just need to type::

        make TARGET=x86 -j3
        qemu -fda build/kernel.img

*/