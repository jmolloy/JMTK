#ifndef STDIO_H
#define STDIO_H

#if defined(HOSTED)
# include_next <stdio.h>
#endif

#include "types.h"
#include <stdarg.h>

int ksnprintf(char *str, size_t size, const char *format, ...);
int ksprintf(char *str, const char *format, ...);
int kprintf(const char *format, ...);

int kvsnprintf(char *str, size_t size, const char *format, va_list ap);

/* Acts like snprintf, but instead of a printf-style format takes a mask
   of characters and prints that mask as applied to 'value'.
   
   The input is a string of characters, each representing one bit in 'value',
   either being '-' (ignore) or a lowercase alphabetic character.

   If the corresponding bit in 'value' is set, ksnprint_bitmask will print
   the uppercase character. If it is unset, it will print the lowercase
   character.

   For example:

   kprint_bitmask("ab-c", 0x9) = "Ab-C".

   If the bitmask is less than 64 bits wide, it will be laid over the 
   least significant bits of 'value'. */
int kprint_bitmask(const char *mask, uint64_t value);
int ksnprint_bitmask(char *str, size_t size, const char *mask, uint64_t value);


#endif
