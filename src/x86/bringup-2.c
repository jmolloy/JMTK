/**#2
   Now we get to C code, and can leave pure assembly behind. We have a stack set
   up, and we're about to call a C function called ``bringup``.
 */

/**#4
   OK, one other small problem is that the multiboot spec doesn't say where in
   memory the multiboot info struct needs to be placed by the bootloader, and in
   fact if we're not careful we could end up overwriting it accidentally!

   Because of this, we need to first copy the structure and all of the things it
   points to somewhere we know cannot be overwritten. We also need to adjust all
   the pointers in the structure to point to somewhere in the higher half rather
   than the lower half as they currently do - we do this simply by adding 3GB
   (0xC0000000) to every pointer value.

   For this, we need a memory allocator, ``earlyalloc``. This is a simple
   bump-pointer allocator, which merely increments a pointer by the size
   requested and can never free memory. { */

#include "string.h"
#include "types.h"
#include "x86/multiboot.h"

/* Give the early allocator 2KB to play with. */
#define EARLYALLOC_SZ 2048

extern int main(int argc, char **argv);

/* The global multiboot struct, which will have all its pointers pointing to
   memory that has been earlyalloc()d. */
multiboot_t mboot;

static uintptr_t earlyalloc(unsigned len) {
  static uint8_t buf[EARLYALLOC_SZ];
  static unsigned idx = 0;

  if (idx + len >= EARLYALLOC_SZ)
    /* Return NULL on failure. It's too early in the boot process to give out a
       diagnostic.*/
    return NULL;

  uint8_t *ptr = &buf[idx];
  idx += len;

  return (uintptr_t)ptr;
}

/* Helper function to split a string on space characters ' ', resulting
   in 'n' different strings. This is used to convert the kernel command line
   into a form suitable for passing to main(). */
static int tokenize(char tok, char *in, char **out, int maxout) {
  int n = 0;
  
  while(*in && n < maxout) {
    out[n++] = in;

    /* Spool until the next instance of 'tok', or end of string. */
    while (*in && *in != tok)
      ++in;
    /* If we exited because we saw a token, make it a NUL character
       and step over it.*/
    if (*in == tok)
      *in++ = '\0';
  }

  return n;
}

/* Entry point from assembly. */
void bringup(multiboot_t *_mboot) {
  /* Call all global constructors. */
  extern size_t __ctors_begin;
  extern size_t __ctors_end;
  for (size_t *i = &__ctors_begin; i < &__ctors_end; ++i) {
    ((void (*)(void)) *i)();
  }

  /* Copy the multiboot struct itself. */
  memcpy((uint8_t*)&mboot, (uint8_t*)_mboot, sizeof(multiboot_t));

  /* If the cmdline member is valid, copy it over. */
  if (mboot.flags & MBOOT_CMDLINE) {
    /* We are now operating from the higher half, so adjust the pointer to take
       this into account! */
    _mboot->cmdline += 0xC0000000;
    int len = strlen((char*)_mboot->cmdline) + 1;
    mboot.cmdline = earlyalloc(len);
    if (mboot.cmdline)
      memcpy((uint8_t*)mboot.cmdline, (uint8_t*)_mboot->cmdline, len);
  }

  if (mboot.flags & MBOOT_MODULES) {
    _mboot->mods_addr += 0xC0000000;
    int len = mboot.mods_count * sizeof(multiboot_module_entry_t);
    mboot.mods_addr = earlyalloc(len);
    if (mboot.mods_addr)
      memcpy((uint8_t*)mboot.mods_addr, (uint8_t*)_mboot->mods_addr, len);
  }

  if (mboot.flags & MBOOT_ELF_SYMS) {
    _mboot->addr += 0xC0000000;
    int len = mboot.num * mboot.size;
    mboot.addr = earlyalloc(len);
    if (mboot.addr)
      memcpy((uint8_t*)mboot.addr, (uint8_t*)_mboot->addr, len);
  }

  if (mboot.flags & MBOOT_MMAP) {
    _mboot->mmap_addr += 0xC0000000;
    mboot.mmap_addr = earlyalloc(mboot.mmap_length + 4);
    if (mboot.mmap_addr) {
      memcpy((uint8_t*)mboot.mmap_addr,
             (uint8_t*)_mboot->mmap_addr - 4, mboot.mmap_length+4);
      mboot.mmap_addr += 4;
      mboot.mmap_addr = _mboot->mmap_addr;
    }
  }

  /**
     And then finally all we need to do is take the kernel command line and
     split it for passing to main() - to do this we use a helper function
     ``tokenize()``, defined slightly earlier, to split the string on every
     space character. { */
  static char *argv[256];
  int argc = tokenize(' ', (char*)mboot.cmdline, argv, 256);

  (void)main(argc, argv);
}

