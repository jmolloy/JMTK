#include "string.h"
#include "types.h"

#if !defined(HOSTED)

// Copy len bytes from src to dest.
void memcpy(void *dest_, const void *src_, uint32_t len)
{
  uint8_t *dest = dest_;
  const uint8_t *src = src_;
  for(; len != 0; len--) *dest++ = *src++;
}

void memmove(void *dest_, const void *src_, uint32_t len) {
  uint8_t *dest = dest_;
  const uint8_t *src = src_;

  if (dest < src) {
    memcpy(dest, src, len);
  } else {
    dest += len;
    src += len;
    for(; len != 0; len--) *--dest = *--src;
  }
}

// Write len copies of val into dest.
void memset(void *dest_, uint8_t val, uint32_t len)
{
  uint8_t *dest = dest_;

  for ( ; len != 0; len--) *dest++ = val;
}

void memsetw(void *dest_, uint16_t val, uint32_t len)
{
  uint16_t *dest = dest_;

  for ( ; len != 0; len--) *dest++ = val;
}

// Compare two strings. Should return -1 if
// str1 < str2, 0 if they are equal or 1 otherwise.
int strcmp(const char *str1, const char *str2)
{
  while (*str1 && *str2 && (*str1 == *str2)) {
    ++str1;
    ++str2;
  }

  if (*str1 == '\0' && *str2 == '\0')
    return 0;

  if (*str1 == '\0')
    return -1;
  else return 1;
}

int strncmp(const char *str1, const char *str2, size_t len)
{
  size_t n = 0;
  while (*str1 && *str2 && (*str1 == *str2) && n < len-1) {
    ++str1;
    ++str2;
    ++n;
  }

  if (*str1 == *str2)
    return 0;

  if (*str1 == '\0')
    return -1;
  else return 1;
}

// Copy the NULL-terminated string src into dest, and
// return dest.
char *strcpy(char *dest, const char *src)
{
  char *_dest = dest;
  while (*src)
    *dest++ = *src++;
  *dest = '\0';
  return _dest;
}

// Concatenate the NULL-terminated string src onto
// the end of dest, and return dest.
char *strcat(char *dest, const char *src)
{
  char *_dest = dest;
  while (*dest)
    ++dest;

  while (*src)
    *dest++ = *src++;
  *dest = '\0';
  return _dest;
}

unsigned strlen(const char *src)
{
  int i = 0;
  while (*src++)
    i++;
  return i;
}

char *strchr(const char *s, int c) {
  while (*s) {
    if (*s == (char)c)
      return (char*)s;
    ++s;
  }
  return NULL;
}

#endif /* !defined(HOSTED) */
