#ifndef STDLIB_H
#define STDLIB_H

#include "types.h"

long int strtol(const char *nptr, char **endptr, int base);
long unsigned int strtoul(const char *nptr, char **endptr, int base);

uint64_t to_unix_timestamp(unsigned day_of_month, unsigned month_of_year,
                           unsigned year,
                           unsigned seconds, unsigned minutes, unsigned hours);
void from_unix_timestamp(uint64_t ts,
                         unsigned *day_of_month, unsigned *month_of_year,
                         unsigned *year,
                         unsigned *seconds, unsigned *minutes, unsigned *hours);

void utf16_to_utf8(uint8_t *outbuf, const uint16_t *inbuf);
void utf8_to_utf16(uint16_t *outbuf, const uint8_t *inbuf);

#endif
