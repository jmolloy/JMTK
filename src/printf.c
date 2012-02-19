#include "printf.h"

#include <stdarg.h>
#include <stdio.h>
#include "string.h"
#include "stdlib.h"
#include "types.h"

#undef PRINTF_DEBUG

#define MAX_ARG_NUM 32
#define BUFSZ 32

#define TY_END 0
#define TY_UNDO 1
#define TY_INT 2
#define TY_DOUBLE 3
#define TY_INDIRECT 4
#define TY_PTR 5

typedef union arg {
  double d;
  int i;
  void *p;
} arg_t;

typedef struct pf_state {
  uint8_t alternate_form; /* '#' flag set? */
  uint8_t zero_pad;
  uint8_t left_justify;
  uint8_t pos_sign_prefix_space, pos_sign_prefix_plus;
  int32_t precision;
  int32_t min_field_width;
  uint8_t transliterate_hex;
} pf_state_t;

static int isdigit(char c) {
  return c >= '0' && c <= '9';
}

static char *cat_char(char *str, int size, int *n, char c) {
  if (*n < size) {
    *str++ = c;
    (*n)++;
  }
  return str;
}

static void convert_uint(char *buf, int bufsz, uint64_t i, int radix, pf_state_t state, int issigned) {
#ifdef PRINTF_DEBUG
  printf("printf: convert_uint(buf=%p, bufsz=%d, i=%llu, state)\n",
         buf, bufsz, i);
#endif
  static char chars[] = {'0','1','2','3','4','5','6','7','8','9','a',
                         'b','c','d','e','f'};
  static char trans_chars[] = {'0','1','2','3','4','5','6','7','8',
                               '9','A','B','C','D','E','F'};

  int isnegative = 0;
  if (issigned && (int64_t)i < 0) {
    isnegative = 1;
    i = (uint64_t) (-(int64_t)i);
  }

  char tmpbuf[32];
  int j = 0;

  while (i != 0) {
    tmpbuf[j++] = (state.transliterate_hex ? trans_chars : chars)[i % radix];
    i /= radix;
  }

  /* String value is in tmpbuf, reversed. */

  i = 0;
  if (isnegative)
    buf[i++] = '-';

  for (int k = 0; k < state.precision-j; ++k)
    buf[i++] = '0';

  while (j)
    buf[i++] = tmpbuf[--j];
  buf[i] = '\0';
}

static void pad_str(char *buf, int bufsz, int *offs, const char *srcbuf, int srcbufsz, int issigned, pf_state_t state, const char *prefix) {
#ifdef PRINTF_DEBUG
  printf("printf: pad_str(buf=%p, bufsz=%d, offs=%d, srcbuf=\"%s\", srcbufsz=%d, issigned=%d, {min_w: %d, left_justify: %d})\n",
         buf, bufsz, *offs, srcbuf, srcbufsz, issigned, state.min_field_width,
         state.left_justify);
#endif
  int isnegative = issigned && srcbuf[0] == '-';

  /* Easy case, when no padding is specified. */
  if (!state.min_field_width) {
    if (issigned && !isnegative) {
      if (state.pos_sign_prefix_plus)
        buf = cat_char(buf, bufsz, offs, '+');
      else if (state.pos_sign_prefix_space)
        buf = cat_char(buf, bufsz, offs, ' ');
    }
    
    while (prefix && *prefix)
      buf = cat_char(buf, bufsz, offs, *prefix++);

    for (int i = 0; srcbufsz ? (i < srcbufsz) : (srcbuf[i] != '\0'); ++i)
      buf = cat_char(buf, bufsz, offs, srcbuf[i]);

    return;
  }

  /* Next easy case - left justification. */
  if (state.left_justify) {
    int i;
    if (issigned && !isnegative) {
      if (state.pos_sign_prefix_plus) {
        --state.min_field_width;
        cat_char(buf, bufsz, offs, '+');
      } else if (state.pos_sign_prefix_space) {
        --state.min_field_width;
        cat_char(buf, bufsz, offs, ' ');
      }
    }

    while (prefix && *prefix) {
      buf = cat_char(buf, bufsz, offs, *prefix++);
      --state.min_field_width;
    }

    for (i = 0; srcbufsz ? (i < srcbufsz) : (srcbuf[i] != '\0'); ++i)
      buf = cat_char(buf, bufsz, offs, srcbuf[i]);

    for (; i < state.min_field_width; ++i)
      buf = cat_char(buf, bufsz, offs, ' ');

    return;
  }

  /* Slightly harder case - right justification. */
  /* "If precision is specified, no more than the number specified are 
     written. If a precision is given, no null byte need be present." */
  int len = (!issigned && state.precision > 1) ? state.precision : (int)strlen(srcbuf);
  char padchar = state.zero_pad ? '0' : ' ';

  char addchar = 0;
  if (issigned && !isnegative) {
    if (state.pos_sign_prefix_plus)
      addchar = '+';
    else if (state.pos_sign_prefix_space)
      addchar = ' ';
  }

  int i = 0;
  if (state.zero_pad) {
    /* If zero-padding, the sign and prefix must go right at the
       start of the string. */
    if (issigned && isnegative) {
      buf = cat_char(buf, bufsz, offs, '-');
      ++i;
      ++srcbuf; /* Step over the '-' in the source buffer. */
      --len;
    }
    if (addchar) {
      buf = cat_char(buf, bufsz, offs, addchar);
      ++i;
    }
    while (prefix && *prefix) {
      buf = cat_char(buf, bufsz, offs, *prefix++);
      ++i;
    }

    for (; i < state.min_field_width-len; ++i)
      buf = cat_char(buf, bufsz, offs, padchar);
  } else {
    /* But if space-padding, put the spaces before the prefix,
       addchar, and sign. */
    if (prefix)
      state.min_field_width -= strlen(prefix);
    if (addchar) state.min_field_width--;

    for (; i < state.min_field_width-len; ++i)
      buf = cat_char(buf, bufsz, offs, padchar);

    if (addchar)
      buf = cat_char(buf, bufsz, offs, addchar);

    while (prefix && *prefix)
      buf = cat_char(buf, bufsz, offs, *prefix++);
  }    

  for (int j = 0; j < len && srcbuf[j]; ++j)
    buf = cat_char(buf, bufsz, offs, srcbuf[j]);
}

static int parse_direct_or_indirect_int(const char **pformat, arg_t *args, int *thisarg) {
  const char *format = *pformat;
  char buf[16];
  int i = 0; 

  if (*format == '*') {
    
    while (isdigit(format[1]))
      buf[i++] = *++format;
    buf[i] = '\0';

    *pformat = format;

    /* Did we find a number? */
    if (i > 0) {
      /* Value is in the i-1'th va_list argument, and we expect
         to see a '$' which we should skip over. */
      if (*format && format[1] == '$')
        (*pformat)++;
      return args[strtol(buf, NULL, 10)-1].i;
    } else {
      /* Value is in the next va_list argument */
      return (int)args[(*thisarg)++].i;
    }

  } else {

    /* Direct number. */
    buf[i++] = *format;
        
    while (isdigit(format[1]))
      buf[i++] = *++format;

    buf[i] = '\0';
    *pformat = format;
    return (int)strtol(buf, NULL, 10);

  }    
}

static const char *convert(char *str, unsigned size, const char *format, int *n, arg_t *args, int *thisarg) {
  pf_state_t s;
  memset(&s, 0, sizeof(pf_state_t));
  s.precision = 1;
  
  char buf[BUFSZ];

  while (*++format) {
    switch (*format) {

    case '%': /* %% = simple escaped % */
      return cat_char(str, size, n, '%');
    
    case '#': /* Alternate form flag */
      s.alternate_form = 1;
      break;

    case '0': /* Zero pad */
      s.zero_pad = 1;
      break;

    case '-': /* Left justify */
      s.left_justify = 1;
      break;

    case ' ': /* Prefix any positive signed conversion with a space */
      s.pos_sign_prefix_space = 1;
      break;

    case '+': /* Prefix any positive signed conversion with a + */
      s.pos_sign_prefix_plus = 1;
      break;

    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9': case '*': {
      /* Number starting with nonzero or '*' is the minimum field width. */
      s.min_field_width = parse_direct_or_indirect_int(&format, args, thisarg);
      break;
    }
    
    case '.': /* Precision */
      ++format;
      s.precision = parse_direct_or_indirect_int(&format, args, thisarg);
      break;
    
    case 'd': case 'i':
      /* If precision is given, zero_pad must be disabled. */
      if (s.precision != 1) s.zero_pad = 0;
      convert_uint(buf, BUFSZ,
                   (uint64_t)((int64_t)args[(*thisarg)++].i),
                   10, s, 1);
      pad_str(str, size, n, buf, 0, 1, s, NULL);
      return format+1;
    case 'o':
      /* If precision is given, zero_pad must be disabled. */
      if (s.precision != 1) s.zero_pad = 0;
      convert_uint(buf, BUFSZ, args[(*thisarg)++].i, 8, s, 0);
      pad_str(str, size, n, buf, 0, 0, s, s.alternate_form ? "0" : NULL);
      return format+1;
    case 'u':
      /* If precision is given, zero_pad must be disabled. */
      if (s.precision != 1) s.zero_pad = 0;
      convert_uint(buf, BUFSZ, (unsigned int)args[(*thisarg)++].i, 10, s, 0);
      pad_str(str, size, n, buf, 0, 0, s, NULL);
      return format+1;
    case 'x':
      /* If precision is given, zero_pad must be disabled. */
      if (s.precision != 1) s.zero_pad = 0;
      convert_uint(buf, BUFSZ, args[(*thisarg)++].i, 16, s, 0);
      pad_str(str, size, n, buf, 0, 0, s, s.alternate_form ? "0x" : NULL);
      return format+1;
    case 'X':
      /* If precision is given, zero_pad must be disabled. */
      if (s.precision != 1) s.zero_pad = 0;
      s.transliterate_hex = 1;
      convert_uint(buf, BUFSZ, args[(*thisarg)++].i, 16, s, 0);
      pad_str(str, size, n, buf, 0, 0, s, s.alternate_form ? "0X" : NULL);
      return format+1;
    case 'c':
      s.zero_pad = 0;
      buf[0] = (char)args[(*thisarg)++].i;
      buf[1] = '\0';
      pad_str(str, size, n, buf, 1, 0, s, NULL);
      return format+1;
    case 's':
      s.zero_pad = 0;
      pad_str(str, size, n, (char*)args[(*thisarg)++].p, 0, 0, s, NULL);
      return format+1;
    case 'p': /* Equivalent to %#x */
      s.zero_pad = 0;
      convert_uint(buf, BUFSZ, (uint64_t)args[(*thisarg)++].p, 16, s, 0);
      pad_str(str, size, n, buf, 0, 0, s, "0x");
      return format+1;
    case 'n': /* Write out the number of arguments converted. */
      s.zero_pad = 0;
      convert_uint(buf, BUFSZ, (uint64_t)*thisarg, 10, s, 0);
      pad_str(str, size, n, buf, 0, 0, s, NULL);
      return format+1;

    case 'e': case 'E':
    case 'f': case 'F':
    case 'g': case 'G':
    case 'a': case 'A':
      pad_str(str, size, n, "???", 3, 0, s, NULL);
      (*thisarg)++;
      return format+1;

    default: /* Shouldn't get here, but we don't have panic available so just try not to infinite loop. */
      return format;
    }
  }
  return format;
}

static int find_required_args(const char **pformat, int prev_ty) {
  const char *format = *pformat;

  while (*format) {
    if (*format != '%' && prev_ty != TY_INDIRECT && prev_ty != TY_UNDO) {
      ++format;
      continue;
    }
    prev_ty = TY_END;

    while (*++format) {
      *pformat = format;
      switch (*format) {
      default: break;
      case '*': /* Indirect argument. */
        return TY_INDIRECT;
      case '$': /* Previous indirect argument was a random access not an iteration. */
        return TY_UNDO;
      case 'd': case 'i':
      case 'o': case 'u': case 'x': case 'X':
      case 'c':
      case 'n':
        return TY_INT;
      case 'e': case 'E':
      case 'f': case 'F':
      case 'g': case 'G':
      case 'a': case 'A':
        return TY_DOUBLE;
      case 's':
      case 'p':
        return TY_PTR;
      }
    }
  }

  return TY_END;
}

int ksnprintf(char *str, unsigned size, const char *format, ...) {
  int n = 0;
  arg_t args[MAX_ARG_NUM];

  const char *fmt_copy = format;
  const char *fmt_copy_2;
  int nargs = 0, ty;

  va_list ap;
  va_start(ap, format);
  while ( (ty=find_required_args(&fmt_copy, ty)) != TY_END) {
#ifdef PRINTF_DEBUG
    printf ("printf: find_required_args() -> %d\n", ty);
#endif
    switch (ty) {
    case TY_INDIRECT:
      /* Indirect argument - prepare for a possible UNDO */
      fmt_copy_2 = fmt_copy;
      if (find_required_args(&fmt_copy_2, TY_INDIRECT) == TY_UNDO)
        /* Going to be undone - do nothing! */
        break;
      /* Else fall through */

    case TY_INT:
      args[nargs++].i = va_arg(ap, int);
      break;

    case TY_DOUBLE:
      args[nargs++].d = va_arg(ap, double);
      break;

    case TY_PTR:
      args[nargs++].p = va_arg(ap, void*);

    default: break;
    }
  }
  va_end(ap);

  int thisarg = 0;
  
  /* Ensure there is room for the NULL byte on the end. */
  --size;

  while (*format && (unsigned)n < size) {
    if (*format == '%') {
      int oldn = n;
      format = convert(str, size, format, &n, args, &thisarg);
      str += n-oldn;
    } else {
      *str++ = *format++;
      ++n;
    }
  }
  *str = '\0';
  return n+1;
}
