#include "stdlib.h"

static int digit_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static int isdigit_for_base(char c, int base) {
  switch (base) {
  case 2:
    return c == '0' || c == '1';
  case 8:
    return c >= '0' && c < '8';
  case 10:
    return c >= '0' && c <= '9';
  case 16:
    return (c >= '0' && c <= '9') ||
      (c >= 'a' && c <= 'f') ||
      (c >= 'A' && c <= 'F');
  default:
    return 0;
  }
}

long int strtol(const char *nptr, char **endptr, int base) {
  /* First, has it got a sign? */
  int negate = 0;
  if (*nptr && *nptr == '-') {
    negate = 1;
    /* Advance over it... */
    ++nptr;
  }

  /* ... Then call kstrtoul and convert to signed. We may
     corrupt data in doing so, but only if it would
     overflow a signed long which the user shouldn't do. */
  long int n = (long int)strtoul(nptr, endptr, base);

  return (negate) ? -n : n;
}

long unsigned int strtoul(const char *nptr, char **endptr, int base) {
  /* First, figure out the base. */
  if (base == 0 && nptr[0] && nptr[0] == '0' &&
      nptr[1] && (nptr[1] == 'x' || nptr[1] == 'X')) {
    base = 16;
  } else if (base == 0 && nptr[0] && nptr[0] == '0') {
    base = 8;
  } else if (base == 0 && nptr[0] && isdigit_for_base(nptr[0], 10)) {
    base = 10;
  }

  if (base == 0) {
    /* Bail out at this point. Failed to auto-detect
       the base. */
    if (endptr) *endptr = (char*)nptr;
    return 0;
  }

  /* Skip over '0x' if needed. */
  if (base == 16 && nptr[0] && nptr[0] == '0' &&
      nptr[1] && (nptr[1] == 'x' || nptr[1] == 'X')) {
    nptr += 2;
  }

  long unsigned int accum = 0;
  while (*nptr && isdigit_for_base(*nptr, base)) {
    accum *= base;
    accum += digit_value(*nptr++);
  }
  
  /* Success! */
  if (endptr) *endptr = (char*)nptr;
  return accum;
}
