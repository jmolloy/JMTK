/*include "slre.h"*/

#ifdef STANDALONE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define vsnprintf kvsnprintf
#endif

typedef int (*instream_t)(char *buf, unsigned bufsz);
typedef void (*outstream_t)(char *buf);

#define CHECK_STR "// CHECK:"
#define CHECK_NOT_STR "// CHECK-NOT:"

#define BUFSZ 512

static int is_whitespace(char c) {
  return c == '\n' || c == '\t' || c == ' ' || c == '\r';
}

static const char *strip_whitespace(const char *c) {
  while (*c != '\0' && is_whitespace(*c))
    ++c;
  return c;
}

static void diag(outstream_t out, const char *str, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, str);

  int n = vsnprintf(buf, 511, str, ap);
  buf[n] = '\0';

  out(buf);
}

static int colcheck(const char *check, const char *in) {
  while (1) {
    switch (check[0]) {
    case '\n': case '\r': case '\t': case ' ':
      in = strip_whitespace(in);
      ++check;
      break;

    case '\0':
      return 1;

    default:
      if (check[0] != in[0] || in[0] == '\0')
        return 0;
      ++check;
      ++in;
    }
  }

  return 0;
}

static int linecheck(const char *check, const char *in) {
  for (int col = 0; in[col] != '\0'; ++col) {
    if (colcheck(check, &in[col]) == 1)
      return col+1;
  }
  return 0;
}

int filecheck(const char *check_fname, instream_t check_in,
              const char *in_fname, instream_t in,
              outstream_t out, outstream_t err) {

  char checkbuf[BUFSZ];
  char inbuf[BUFSZ];
  char check_not_buf[BUFSZ];

  unsigned check_linum = 0;
  unsigned in_linum = 0;

  /* For every line in 'check_in', attempt to find a matching line in 'in'. */
  while (check_in(checkbuf, BUFSZ) != -1) {
    const char *checkbuf2 = strip_whitespace(checkbuf);
    ++check_linum;
    checkbuf[BUFSZ-1] = '\0';

    if (!strncmp(checkbuf2, CHECK_NOT_STR, strlen(CHECK_NOT_STR))) {

      strcpy(check_not_buf, strip_whitespace(checkbuf2 + strlen(CHECK_NOT_STR)));
      continue;

    } else if (!strncmp(checkbuf2, CHECK_STR, strlen(CHECK_STR))) {
      
      const char *check = strip_whitespace(checkbuf2 + strlen(CHECK_STR));

      unsigned saved_in_linum = in_linum;
      int col = 0, not_col = 0;
      while (in(inbuf, BUFSZ) != -1) {
        ++in_linum;
        inbuf[BUFSZ-1] = '\0';

        col = linecheck(check, inbuf);
        if (col > 0)
          break;

        if (check_not_buf[0] != '\0') {
          not_col = linecheck(check_not_buf, inbuf);
          if (not_col > 0)
            break;
        }
      }

      if (col == 0 && not_col == 0) {
        diag(err,
             "%s:%d: expected string not found in input: %s",
             check_fname, check_linum, check);
        diag(err,
             "%s:%d:%d: scanning from here",
             in_fname, saved_in_linum, col);
        return 1;
      } else if (not_col > 0) {
        diag(err,
             "%s:%d: CHECK-NOT string found",
             check_fname, check_linum);
        diag(err,
             "%s:%d:%d: here",
             in_fname, saved_in_linum, not_col);
        return 1;
      }

      check_not_buf[0] = '\0';
    }
  }

  return 0;
}

#ifdef STANDALONE
static void usage() {
  fprintf(stderr, "filecheck: Compare standard input against a set of CHECK patterns\n");
  fprintf(stderr, "filecheck: usage: echo <input> | filecheck <check-file>\n");
}

static FILE *checkstream;

static int line_in(FILE *stream, char *buf, unsigned bufsz) {
  int i = 0;
  while (i < (int)bufsz) {
    char c;
    int ret = fread(&c, 1, 1, stream);

    if (ret != 1) {
      buf[i] = '\0';
      return (i == 0) ? -1 : i;
    }
    if (c == '\n') {
      buf[i] = '\0';
      return i;
    }
    if (c == '\00')
      continue;

    buf[i++] = c;
  }
  buf[bufsz-1] = '\0';
  return bufsz-1;
}

static int check_in(char *buf, unsigned bufsz) {
  return line_in(checkstream, buf, bufsz);
}
static int in(char *buf, unsigned bufsz) {
  return line_in(stdin, buf, bufsz);
}
static void out(char *buf) {
  fprintf(stdout, "%s", buf);
  fprintf(stdout, "\n");
}
static void err(char *buf) {
  fprintf(stderr, "%s", buf);
  fprintf(stderr, "\n");
}

int main(int argc, char **argv) {
  if (argc != 2) {
    usage();
    return 2;
  }

  if (!(checkstream = fopen(argv[1], "r"))) {
    fprintf(stderr, "filecheck: unable to open file '%s' for reading.\n", argv[1]);
    return 2;
  }

  return filecheck(argv[1], &check_in,
                   "<stdin>", &in,
                   &out, &err);
}
#endif
