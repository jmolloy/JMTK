#include "hal.h"
#include "readline.h"
#include "stdio.h"
#include "string.h"

#define LINEBUF_SZ 128

static void tab_complete(char *buf, size_t bufsz, size_t *bufidx,
                         readline_completer_t completer) {
  kprintf("\a");
}

static void kill_chars_forward(char *buf, size_t bufsz, size_t *bufidx,
                                int n) {
  /* Start blatting through the buffer, until we reach the first NUL. */
  size_t i;
  for (i = *bufidx; buf[i] != '\0' && i+n < bufsz; ++i)
    buf[i] = buf[i+n];

  size_t num = i - *bufidx;
  size_t num_left = num;

  i = *bufidx;
  while (buf[i] != '\0') {
    write_console(&buf[i++], 1);
    --num_left;
  }

  while (num_left--)
    write_console(" ", 1);

  /* Then spool back to the insert location. */
  while (num--)
    write_console("\x08", 1);
}

static void kill_chars_backward(char *buf, size_t bufsz, size_t *bufidx,
                                size_t n) {
  size_t i;

  /* Only delete up to the number of chars in the buffer! */
  n = (n > *bufidx) ? *bufidx : n;

  /* Spool back to the start of where we're going to edit (and
     where the cursor will end up afterward. */
  for (i = 0; i < n; ++i)
    write_console("\x08", 1); /* Emit backspace. */

  *bufidx -= n;

  kill_chars_forward(buf, bufsz, bufidx, n);
}

static void kill_eol(char *buf, size_t bufsz, size_t *bufidx) {
  size_t i;
  for (i = 0; buf[i+*bufidx] != '\0'; ++i)
    ;

  kill_chars_forward(buf, bufsz, bufidx, i);
}

static void kill_word_backward(char *buf, size_t bufsz, size_t *bufidx) {
  if (!*bufidx) return;

  size_t i = *bufidx - 1;
  /* Step over any initial whitespace. */
  while (i && buf[i] == ' ')
    --i;
  while (i && buf[i] != ' ')
    --i;

  /* Then kill to i+1, to avoid killing the last whitespace too. */
  kill_chars_backward(buf, bufsz, bufidx, *bufidx-i-1);
}

/* Insert characters at the given insert point, shifting the remaining
   text right. */
static void insert_chars(char *buf, size_t bufsz, size_t *bufidx, int n,
                         char *chars) {
  /* Shift the contents of the buffer n to the right. */
  int i = *bufidx;

  /* Spool to the end of the string... */
  while (buf[i])
    ++i;

  /* Try and advance n places off the end of the string. */
  int j;
  for (j = 0; j < n && i < (int)bufsz; ++j)
    ++i;

  size_t end = i;

  /* Now reverse through the string, copying from location i-1.*/
  for (; i > (int)*bufidx; --i)
    buf[i] = buf[i-n];

  /* Now overwrite the characters into the string. */
  for (j = 0; j < n && i < (int)bufsz; ++j)
    buf[i++] = chars[j];

  /* Now we just need to update the terminal.
     Write up to the end of the string, then spool back to
     the new insert position. */
  write_console(&buf[*bufidx], end - *bufidx);
  for (j = 0; j < (int) (end - *bufidx - n); ++j)
    write_console("\x08", 1);
  *bufidx += n;
}

static void move_backward(size_t *bufidx, size_t n) {
  while (n-- && *bufidx) {
    write_console("\x08", 1);
    (*bufidx)--;
  }
}

static void move_forward(char *buf, size_t bufsz, size_t *bufidx, size_t n) {
  while (n-- && *bufidx < bufsz) {
    write_console(&buf[(*bufidx)++], 1);
  }
}

static size_t handle_escape(char *buf, size_t bufsz, size_t *bufidx, char c,
                            char *escape_buf, size_t *escape_bufidx) {
  switch (c) {
  case '[': /* Completely ignore '['. */
    return 1;

  case 'D':
    /* Cursor left */
    move_backward(bufidx, 1);
    return 0;

  case 'C':
    /* Cursor right */
    move_forward(buf, bufsz, bufidx, 1);
    return 0;

  default:
    kprintf("Unknown escape char: @@%d@@\n", c);
    return 0;
  }

  return 1;
}

void readline(char *buf, size_t bufsz, const char *prompt,
              readline_completer_t completer) {

  /* Firstly, emit the prompt. */
  kprintf(prompt);
  
  /* Holds the current cursor position. */
  size_t bufidx = 0;
  char c;
  int escape = 0;
  char escape_buf[8];
  size_t escape_idx = 0;

  memset((uint8_t*)buf, 0, bufsz);

  while (read_console(&c, 1) != -1) {
    if (escape) {
      escape = handle_escape(buf, bufsz, &bufidx, c, escape_buf, &escape_idx);
      continue;
    }
    switch (c) {
    case '\t':
      tab_complete(buf, bufsz, &bufidx, completer);
      break;

    case '\r': case '\n':
      kprintf("\r\n");
      return;

    case '\x08':
    case '\x7F':
      /* Backspace */
      kill_chars_backward(buf, bufsz, &bufidx, 1);
      break;

    case '\033':
      escape = 1;
      break;

    case '\x0b':
      /* Ctrl-K - kill to end of line. */
      kill_eol(buf, bufsz, &bufidx);
      break;

    case '\x17':
      /* Ctrl-W - kill last word. */
      kill_word_backward(buf, bufsz, &bufidx);
      break;

    default:
      /* Is this a printable character? */
      if (c >= ' ' && c <= '~')
        insert_chars(buf, bufsz, &bufidx, 1, &c);
    }
  }
  /* If we got here, read_console returned -1. */
  kprintf("readline: read failed!\n");
  buf[0] = '\0';
}
  
