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

#define EPOCH 1970

static const uint64_t days_in_month[13] = {0 /*UNUSED*/,
  31 /*JAN*/, 28 /*FEB*/, 31 /*MAR*/, 30 /*APR*/, 31 /*MAY*/, 30 /*JUN*/,
  31 /*JUL*/, 31 /*AUG*/, 30 /*SEP*/, 31 /*NOV*/, 31 /*DEC*/
};

#define LEAPYEAR(y) ((y % 4 == 0) && (y % 100 != 0 || y % 400 == 0))

uint64_t to_unix_timestamp(unsigned day_of_month, unsigned month_of_year,
                           unsigned year,
                           unsigned seconds, unsigned minutes, unsigned hours) {

  uint64_t days = 0;

  /* Iteratively count up from the epoch year to the given year as
     each year may be a leap. */
  for (unsigned y = EPOCH; y < year; ++y)
    days += LEAPYEAR(y) ? 366 : 365;

  /* Same with months, because February could be 28 or 29. */
  for (unsigned m = 1; m < month_of_year; ++m)
    days += days_in_month[m] + ((LEAPYEAR(year) && m == 2) ? 1 : 0);

  days += day_of_month-1;

  return days * 86400 + hours * 3600 + minutes * 60 + seconds;
}

void from_unix_timestamp(uint64_t ts,
                         unsigned *day_of_month, unsigned *month_of_year,
                         unsigned *year,
                         unsigned *seconds, unsigned *minutes, unsigned *hours) {

  uint64_t ndays = ts / 86400;
  uint64_t time = ts % 86400;

  *hours = time / 3600;
  *minutes = (time % 3600) / 60;
  *seconds = time % 60;

  *year = EPOCH;
  while (ndays >= (LEAPYEAR(*year) ? 366 : 365)) {
    ndays -= (LEAPYEAR(*year) ? 366 : 365);
    ++*year;
  }
  
  unsigned month = 1;
  while (ndays >= days_in_month[month]) {
    ndays -= days_in_month[month];
    ++month;
  }
  *month_of_year = month;

  *day_of_month = ndays + 1;
}

void utf16_to_utf8(uint8_t *outbuf, const uint16_t *inbuf) {
  /* FIXME: Implement proper conversion. */
  unsigned i;
  for (i = 0; inbuf[i] != 0; ++i) {
    outbuf[i] = inbuf[i] & 0xFF;
  }
  outbuf[i] = 0;
}

void utf8_to_utf16(uint16_t *outbuf, const uint8_t *inbuf) {
  /* FIXME: Implement proper conversion. */
  unsigned i;
  for (i = 0; inbuf[i] != 0; ++i) {
    if (inbuf[i] >= 128) continue;
    outbuf[i] = inbuf[i];
  }
  outbuf[i] = 0;
}
