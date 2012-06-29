#include "stdio.h"

#include <stdarg.h>
#include "string.h"
#include "stdlib.h"
#include "types.h"

#undef PRINTF_DEBUG

#ifdef PRINTF_DEBUG
# if !defined(HOSTED)
#  error Printf can only be debugged on a hosted build!
# endif
# include <stdio.h>
#endif

/* The maximum number of arguments able to be passed to snprintf. */
#define MAX_ARG_NUM 32
/* The size of the internal buffer for converting from integers. */
#define BUFSZ 32

/* Possible return values from 'find_required_args', which informs
   snprintf how many arguments to pull from the va_list and what
   their types are. */
#define TY_END      0 /* There are no more arguments to process. */
#define TY_UNDO     1 /* The previous TY_INDIRECT argument has an
                         absolute argument reference so was reported
                         erroneously. Remove it. */
#define TY_INT      2 /* Type 'int' */
#define TY_DOUBLE   3 /* Type 'double' */
#define TY_INDIRECT 4 /* Type 'int', an indirect reference ('*' in the
                         format string), can be undone with TY_UNDO */
#define TY_PTR      5 /* Type 'void*' */

/* An argument type. Just a union of all known argument types.
   It is expected that the type be known by the format specifier
   so no tagging is done. */
typedef union arg {
  double d;
  int i;
  unsigned int u;
  void *p;
} arg_t;

/* State structure, for convert() and friends. */
typedef struct pf_state {
  uint8_t alternate_form;       /* '#' flag set? */
  uint8_t zero_pad;             /* '0' set? */
  uint8_t left_justify;         /* '-' set? */
  uint8_t pos_sign_prefix_space;/* ' ' set? */
  uint8_t pos_sign_prefix_plus; /* '+' set? */
  int32_t precision;            /* Precision value defaults to '1' */
  int32_t min_field_width;      /* Minimum field width, defaults to '0' */
  uint8_t transliterate_hex;    /* Write hex as 'A-F' instead of 'a-f'. */
} pf_state_t;

static int isdigit(char c) {
  return c >= '0' && c <= '9';
}

/* Concatenates a character onto a string.
   - The string may be full, in which case it does nothing.
   - If it concatenates then it will increment *n. */
static char *cat_char(char *str, int size, int *n, char c) {
  if (*n < size) {
    *str++ = c;
    (*n)++;
  }
  return str;
}

/* convert_int - given a buffer (buf,bufsz) and a 64-bit unsigned integer 'i',
   perform a conversion to string.

   Radix obviously specifies the radix of the transformation. '0' is not supported
   unlike strtol().

   If 'issigned' is set, 'i' is reinterpreted as a 64-bit signed integer.

   This function cares about the precision setting in 'state'. */
static void convert_int(char *buf, int bufsz, uint64_t i, int radix,
                        pf_state_t state, int issigned) {
#ifdef PRINTF_DEBUG
  printf("printf: convert_int(buf=%p, bufsz=%d, i=%llu, state)\n",
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
  tmpbuf[j] = '\0';
  
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

/* pad_str - Given a null-terminated string in srcbuf, write it to (buf[*offs],bufsz)
   based on the padding requirements in 'state', and increment *offs with the number
   of characters written.

   - issigned should be set if the input string could have a minus sign that may need
     to be dealt with specially during zero-padding.
   - prefix, if non-NULL, specifies a prefix to be put onto the front of the string.
     An example of this is "0x" for hex strings. */
static void pad_str(char *buf, int bufsz, int *offs, const char *srcbuf, 
                    int issigned, pf_state_t state, const char *prefix) {
#ifdef PRINTF_DEBUG
  printf("printf: pad_str(buf=%p, bufsz=%d, offs=%d, srcbuf=\"%s\", issigned=%d, {min_w: %d, left_justify: %d})\n",
         buf, bufsz, *offs, srcbuf, issigned, state.min_field_width,
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

    for (int i = 0; srcbuf[i] != '\0'; ++i)
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

    for (i = 0; srcbuf[i] != '\0'; ++i)
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

/* parse_direct_or_indirect_int - At **pformat is either '*' or a digit.
   - If '*', it may be followed by a number 'n' then '$'. If so, return the
     (n-1)'th argument to snprintf. If it is not, then return the next argument
     to snprintf.
   - If a digit, maximally munch all digits following, convert to an integer
     then return. */
static int parse_direct_or_indirect_int(const char **pformat, arg_t *args,
                                        int *thisarg) {
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

/* convert - The main conversion function. *format points at a '%'.
   - str contains the output string. The original buffer was 'size' large
     (but *str is the current insert position inside the buffer).
   - *n is the offset inside the output buffer of the current insert position,
     or: (str - start_str).
   - args is an array of arguments passed to snprintf.
   - *thisarg is the next argument to process. */
static const char *convert(char *str, unsigned size, const char *format, int *n, arg_t *args, int *thisarg) {
  pf_state_t s;
  memset((uint8_t*)&s, 0, sizeof(pf_state_t));
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
      convert_int(buf, BUFSZ, args[(*thisarg)++].i, 10, s, 1);
      pad_str(str, size, n, buf, 1, s, NULL);
      return format+1;
    case 'o':
      /* If precision is given, zero_pad must be disabled. */
      if (s.precision != 1) s.zero_pad = 0;
      convert_int(buf, BUFSZ, args[(*thisarg)++].u, 8, s, 0);
      pad_str(str, size, n, buf, 0, s, s.alternate_form ? "0" : NULL);
      return format+1;
    case 'u':
      /* If precision is given, zero_pad must be disabled. */
      if (s.precision != 1) s.zero_pad = 0;
      convert_int(buf, BUFSZ, (unsigned int)args[(*thisarg)++].i, 10, s, 0);
      pad_str(str, size, n, buf, 0, s, NULL);
      return format+1;
    case 'x':
      /* If precision is given, zero_pad must be disabled. */
      if (s.precision != 1) s.zero_pad = 0;
      convert_int(buf, BUFSZ, args[(*thisarg)++].u, 16, s, 0);
      pad_str(str, size, n, buf, 0, s, s.alternate_form ? "0x" : NULL);
      return format+1;
    case 'X':
      /* If precision is given, zero_pad must be disabled. */
      if (s.precision != 1) s.zero_pad = 0;
      s.transliterate_hex = 1;
      convert_int(buf, BUFSZ, args[(*thisarg)++].u, 16, s, 0);
      pad_str(str, size, n, buf, 0, s, s.alternate_form ? "0X" : NULL);
      return format+1;
    case 'c':
      s.zero_pad = 0;
      buf[0] = (char)args[(*thisarg)++].i;
      buf[1] = '\0';
      pad_str(str, size, n, buf, 0, s, NULL);
      return format+1;
    case 's': {
      s.zero_pad = 0;
      char *in = (char*)args[(*thisarg)++].p;
      if (!in) in = "<null>";
      pad_str(str, size, n, in, 0, s, NULL);
      return format+1;
    }
    case 'p': /* Equivalent to %#x */
      s.zero_pad = 0;
      convert_int(buf, BUFSZ, (uintptr_t)args[(*thisarg)++].p, 16, s, 0);
      pad_str(str, size, n, buf, 0, s, "0x");
      return format+1;
    case 'n': /* Write out the number of arguments converted. */
      s.zero_pad = 0;
      convert_int(buf, BUFSZ, (uintptr_t)*thisarg, 10, s, 0);
      pad_str(str, size, n, buf, 0, s, NULL);
      return format+1;

    case 'e': case 'E':
    case 'f': case 'F':
    case 'g': case 'G':
    case 'a': case 'A':
      pad_str(str, size, n, "???", 0, s, NULL);
      (*thisarg)++;
      return format+1;

    default: /* Shouldn't get here, but we don't have panic available so just try not to infinite loop. */
      return format;
    }
  }
  return format;
}

/* find_required_args - Given a pointer into a format string **pformat,
   return a description of the next argument type snprintf should load
   from its variadic argument list.
   - prev_ty is the previous value that find_required_args returned,
     or undefined if it has never been called before.
   - This function may return TY_END, meaning there are no more arguments
     to load based on the format string.
   - A TY_INDIRECT return value (signifying a possible indirect argument
     load based on a '*' specifier) may be immediately followed by a
     TY_UNDO, if that '*' specifier was then followed by '<int>$', which
     instead specifies an absolute, random-access argument to load
     instead of a new argument. */
static int find_required_args(const char **pformat, int prev_ty) {
  const char *format = *pformat;

  while (*format) {
    /* If we're not about to look at a conversion specifier carry on,
       unless we were previously in an INDIRECT or UNDO state, in
       which case we may have been called half way through a 
       specifier already. */
    if (*format != '%' && prev_ty != TY_INDIRECT && prev_ty != TY_UNDO) {
      ++format;
      continue;
    }
    /* Now we don't care about prev_ty any more. */
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


int kvsnprintf(char *str, size_t size, const char *format, va_list ap) {
  int n = 0;
  arg_t args[MAX_ARG_NUM];

  const char *fmt_copy = format;
  const char *fmt_copy_2;
  int nargs = 0, ty = 0;

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
  return n;
}
