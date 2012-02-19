#include "stdio.h"
#include "string.h"
#include "hal.h"

int kprintf(const char *format, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, format);

  int n = kvsnprintf(buf, 512, format, ap);

  va_end(ap);

  write_console(buf, n);
  return n;
}

int ksnprintf(char *str, size_t size, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  int n = kvsnprintf(str, size, format, ap);
  va_end(ap);
  return n;
}

int ksprintf(char *str, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  int n = kvsnprintf(str, ~0UL, format, ap);
  va_end(ap);
  return n;
}

int ksnprint_bitmask(char *str, size_t size, const char *mask, uint64_t value) {
  int masklen = strlen(mask);

  size_t j = 0;
  for (int i = masklen-1; i >= 0; --i) {
    if (j == size) {
      *(str-1) = '\0';
      return j-1;
    }

    if (*mask >= 'a' && *mask <= 'z' &&
        (value & (1ULL<<i)))
      *str++ = *mask++ - 'a' + 'A';
    else
      *str++ = *mask++;
    ++j;
  }

  *str = '\0';
  return j;
}

int kprint_bitmask(const char *mask, uint64_t value) {
  char buf[65];
  int n = ksnprint_bitmask(buf, 65, mask, value);

  write_console(buf, n);
  return n;
}
