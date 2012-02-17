#include "string.h"
#include "types.h"
#include "x86/multiboot.h"

#define EARLYALLOC_SZ 2048

extern int main(int argc, char **argv);

multiboot_t mboot;

static uintptr_t earlyalloc(unsigned len) {
  static uint8_t buf[EARLYALLOC_SZ];
  static unsigned idx = 0;

  if (idx + len >= EARLYALLOC_SZ)
    return NULL;

  uint8_t *ptr = &buf[idx];
  idx += len;

  return (uintptr_t)ptr;
}

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

void bringup(multiboot_t *_mboot) {
  memcpy((uint8_t*)&mboot, (uint8_t*)_mboot, sizeof(multiboot_t));

  if (mboot.flags & MBOOT_CMDLINE) {
    int len = strlen((char*)_mboot->cmdline) + 1;
    mboot.cmdline = earlyalloc(len);
    memcpy((uint8_t*)&mboot.cmdline, (uint8_t*)&_mboot->cmdline, len);
  }

  if (mboot.flags & MBOOT_MODULES) {
    int len = mboot.mods_count * sizeof(multiboot_module_entry_t);
    mboot.mods_addr = earlyalloc(len);
    memcpy((uint8_t*)&mboot.mods_addr, (uint8_t*)&_mboot->mods_addr, len);
  }

  if (mboot.flags & MBOOT_ELF_SYMS) {
    int len = mboot.num * mboot.size;
    mboot.addr = earlyalloc(len);
    memcpy((uint8_t*)&mboot.addr, (uint8_t*)&_mboot->addr, len);
  }

  if (mboot.flags & MBOOT_MMAP) {
    mboot.mmap_addr = earlyalloc(mboot.mmap_length);
    memcpy((uint8_t*)&mboot.mmap_addr,
           (uint8_t*)&_mboot->mmap_addr, mboot.mmap_length);
  }

  static char *argv[256];
  int argc = tokenize(' ', (char*)mboot.cmdline, argv, 256);

  (void)main(argc, argv);
}
