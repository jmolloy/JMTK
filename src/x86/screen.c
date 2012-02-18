#include "hal.h"
#include "string.h"
#include "types.h"
#include "x86/io.h"

#define C_BLACK           0
#define C_BLUE            1
#define C_GREEN           2
#define C_CYAN            3
#define C_RED             4
#define C_MAGENTA         5
#define C_BROWN           6
#define C_LIGHTGRAY       7
#define C_DARKGRAY        8
#define C_LIGHTBLUE       9
#define C_LIGHTGREEN     10
#define C_LIGHTCYAN      11
#define C_LIGHTRED       12
#define C_LIGHTMAGENTA   13
#define C_LIGHTBROWN     14
#define C_WHITE          15

#define MAKE_CHAR(c, fore, back) (c | (back<<12) | (fore<<8))

static uint16_t *video_memory = (uint16_t*)0xC00B8000;
static int cursor_x = 0, cursor_y = 0;
static int c_back = C_BLACK;
static int c_fore = C_LIGHTGRAY;

static void update_cursor() {
  uint16_t loc = cursor_y * 80 + cursor_x;

  outb(0x3D4, 14);
  outb(0x3D5, loc >> 8);
  outb(0x3D4, 15);
  outb(0x3D5, loc & 0xFF);
}

static void scroll() {
  if (cursor_y >= 25) {
    memcpy((uint8_t*)video_memory,
           (uint8_t*)&video_memory[80], 2 * 80 * 24);
    memsetw(&video_memory[80 * 24],
            MAKE_CHAR(' ', c_back, c_back), 80);
    cursor_y = 24;
  }
}

static void putc(char c) {
  int back = c_back;
  int fore = c_fore;

  switch (c) {
  case 0x08: /* Backspace */
    --cursor_x;
    if (cursor_x > 80) { /* Wrapped? */
      cursor_x = 0;
      if (cursor_y > 0)
        --cursor_y;
    }
    break;

  case '\t':
    cursor_x = (cursor_x+8) & ~7;
    break;

  case '\r':
    cursor_x = 0;
    break;

  case '\n':
    cursor_x = 0;
    ++cursor_y;

  default:
    /* Is the character printable? */
    if (c >= ' ') {
      video_memory[cursor_y*80 + cursor_x] = MAKE_CHAR(c, fore, back);
      ++cursor_x;
    }
    break;
  }

  if (cursor_x >= 80) {
    cursor_x -= 80;
    ++cursor_y;
  }
  scroll();
  update_cursor();
}

static void cls() {
  memsetw(video_memory, MAKE_CHAR(' ', c_fore, c_back), 80 * 25);
  cursor_x = cursor_y = 0;
  update_cursor();
}

static int write(console_t *obj, const char *buf, int len) {
  for (int i = 0; i < len; ++i)
    putc(buf[i]);
  return len;
}

console_t c = {
  .open  = NULL,
  .close = NULL,
  .read  = NULL,
  .write = &write,
  .flush = NULL,
  .data  = NULL
};
static int register_screen() {
  cls();
  register_console(&c, 0);

  return 0;
}

static const char *prereqs[] = {"console", NULL};
static init_fini_fn_t run_on_startup x = {
  .name = "x86/screen",
  .prerequisites = prereqs,
  .fn = &register_screen
};
