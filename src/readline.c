#include "hal.h"
#include "readline.h"
#include "stdio.h"
#include "string.h"

#define HISTORY_NUM 32
#define HISTORY_LINE_LEN 256
static char history[HISTORY_NUM][HISTORY_LINE_LEN];
int history_idx = -1, history_max = 0;

static void tab_complete(char *buf, size_t bufsz, size_t *bufidx,
                         readline_completer_t completer);

static void history_back(char *buf, size_t bufz, size_t *bufidx);
static void history_forward(char *buf, size_t bufz, size_t *bufidx);
static void history_add(char *buf, size_t bufsz);

static void kill_chars_forward(char *buf, size_t bufsz, size_t *bufidx,
                               int n);
static void kill_chars_backward(char *buf, size_t bufsz, size_t *bufidx,
                                size_t n);
static void kill_eol(char *buf, size_t bufsz, size_t *bufidx);
static void kill_word_backward(char *buf, size_t bufsz, size_t *bufidx);

static void insert_chars(char *buf, size_t bufsz, size_t *bufidx, int n,
                         char *chars);
static void move_backward(size_t *bufidx, size_t n);
static void move_forward(char *buf, size_t bufsz, size_t *bufidx, size_t n);

static void tab_complete(char *buf, size_t bufsz, size_t *bufidx,
                         readline_completer_t completer) {
  /* FIXME: Implement! */
  kprintf("\a");
}

static void history_back(char *buf, size_t bufsz, size_t *bufidx) {
  if (history_idx >= history_max-1) {
    return;
  }

  char *c = history[++history_idx];

  kill_chars_backward(buf, bufsz, bufidx, *bufidx);
  insert_chars(buf, bufsz, bufidx, strlen(c), c);
}

static void history_forward(char *buf, size_t bufsz, size_t *bufidx) {
  if (history_idx == 0) {
    kill_chars_backward(buf, bufsz, bufidx, *bufidx);
    history_idx = -1;
    return;
  } else if (history_idx < 0)
    return;

  char *c = history[--history_idx];

  kill_chars_backward(buf, bufsz, bufidx, *bufidx);
  insert_chars(buf, bufsz, bufidx, strlen(c), c);
}

static void history_add(char *buf, size_t bufsz) {
  memmove((uint8_t*)&history[1],
          (uint8_t*)&history[0], HISTORY_LINE_LEN * (HISTORY_NUM-1));

  strcpy(history[0], buf);
  ++history_max;
  if (history_max > HISTORY_NUM)
    history_max = HISTORY_NUM;

  history_idx = -1;
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

  case 'A':
    /* Cursor up */
    history_back(buf, bufsz, bufidx);
    return 0;

  case 'B':
    /* Cursor down */
    history_forward(buf, bufsz, bufidx);
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

  /* Escape (\033) handling. */
  int escape = 0;        /* Has an \033 been seen? */
  char escape_buf[8];    /* Buffer for handle_escape() */
  size_t escape_idx = 0; /* Index into escape_buf, for handle_escape() */

  /* Reset the input buffer to all '\0' to begin. */
  memset((uint8_t*)buf, 0, bufsz);

  while (read_console(&c, 1) != -1) {
    /* If we're handling an escape sequence, handle it and exit early. */
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
      history_add(buf, bufsz);
      return;

    case '\x08':
    case '\x7F':
      /* Backspace */
      kill_chars_backward(buf, bufsz, &bufidx, 1);
      break;

    case '\033':
      /* Escape detected - move to escape handling mode. */
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

    case '\x01':
      /* Ctrl-A - start of line. */
      move_backward(&bufidx, bufidx);
      break;

    case '\x05':
      /* Ctrl-E - end of line. */
      move_forward(buf, bufsz, &bufidx, strlen(buf) - bufidx);
      break;

    case '\x03':
      /* Ctrl-C - cancel. */
      buf[0] = '\0';
      kprintf("\r\n");
      return;

    default:
      /* No special action, but only insert if this is a printable character. */
      if (c >= ' ' && c <= '~')
        insert_chars(buf, bufsz, &bufidx, 1, &c);
    }
  }
  /* If we got here, read_console returned -1. */
  kprintf("readline: read failed!\n");
  buf[0] = '\0';
}
