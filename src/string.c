#include "string.h"
#include "types.h"

#if !defined(HOSTED)

// Copy len bytes from src to dest.
void memcpy(uint8_t *dest, const uint8_t *src, uint32_t len)
{
  for(; len != 0; len--) *dest++ = *src++;
}

void memmove(uint8_t *dest, const uint8_t *src, uint32_t len) {
  if (dest < src) {
    memcpy(dest, src, len);
  } else {
    dest += len;
    src += len;
    for(; len != 0; len--) *--dest = *--src;
  }
}

// Write len copies of val into dest.
void memset(uint8_t *dest, uint8_t val, uint32_t len)
{
  for ( ; len != 0; len--) *dest++ = val;
}

void memsetw(uint16_t *dest, uint16_t val, uint32_t len)
{
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

int strlen(const char *src)
{
  int i = 0;
  while (*src++)
    i++;
  return i;
}

#endif /* !defined(HOSTED) */
