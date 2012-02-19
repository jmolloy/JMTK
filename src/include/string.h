#ifndef STRING_H
#define STRING_H

#if defined(HOSTED)
# include_next <string.h>
#else

#include "types.h"

int strlen(const char *s);

void memcpy(uint8_t *dest, const uint8_t *src, uint32_t len);

void memset(uint8_t *dest, uint8_t val, uint32_t len);
void memsetw(uint16_t *dest, uint16_t val, uint32_t len);

int strcmp(const char *str1, const char *str2);

char *strcpy(char *dest, const char *src);

char *strcat(char *dest, const char *src);

#endif /* defined(HOSTED) */

#endif
