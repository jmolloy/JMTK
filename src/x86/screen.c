#include "hal.h"
#include "string.h"
#include "types.h"
#include "x86/io.h"

/** Screen printing
    ===============

    The screen is probably one of the output mediums you'll want to get operating first. I'm not going to bother with an unnecessary introduction about what it is or how it is useful - I assume you know that already!

    When the X86 boots, the adapter is put in a known state - it is in *text mode*. This differs from the usual graphical mode, because instead of operating on pixels you operate on characters.

    The screen displays at a resolution of 80 x 25 - 80 characters wide and 25 high. The font size is 8px wide, 16px high. */

/** Each character is represented by a 16-bit number, with the low 8 bits being the ASCII character code, the next nibble (4 bits) being the foreground colour and the high nibble the background colour. { */

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

/** The video framebuffer is also stored at a known location - 0xB8000 physical.

    Because we have mapped low memory up high, we access this as 0xC00B8000. { */

static uint16_t *video_memory = (uint16_t*)0xC00B8000;
static int cursor_x = 0, cursor_y = 0;
static int c_back = C_BLACK;
static int c_fore = C_LIGHTGRAY;
static int c_bold = 0;

static int in_escape = 0;

/** To make our screen driver that bit cooler, we should handle colour escapes. These are terminal escape sequences used to set the foreground or background colour, and take the form ``\033 [ <number> ; <number>... m``;

       * ``\033`` is the escape character (equivalent to ``\e``) and defines the start of an escape sequence.
       * ``[`` is meaningless.
       * Following these is a sequence of numbers separated by semicolons. These represent actions to take:
          * ``0`` forces the colours and bold state to be reset to the default.
          * ``1`` sets bold mode to be on. Characters written after this should appear bold. This often does not change the font, but changes the colour palette to a brighter version instead.
          * ``3[0-9]`` sets the foreground colour to ``n - 30``.
          * ``4[0-9]`` sets the background colour to ``n - 40``.
       * ``m`` terminates the escape sequence, and instructs us that the sequence was a command to change colours. There are other letters for other commands such as "move cursor left". ``handle_colour_escape()`` should act on a colour number such as ``1`` or ``32`` and change the fore/back colour appropriately. { */

static char escape_buf[4];
static int escape_buf_idx = 0;
static int escape_nums[4];
static int escape_num_idx = 0;

static void handle_colour_escape(int e) {
  switch (e) {
  case 0:
    /* Reset */
    c_fore = C_LIGHTGRAY;
    c_back = C_BLACK;
    c_bold = 0;
    break;
  case 1:
    /* Bold */
    if (!c_bold)
      c_fore += 8;
    c_bold = 1;
    break;

  case 30: c_fore = (!c_bold) ? C_BLACK : C_DARKGRAY; break;
  case 31: c_fore = (!c_bold) ? C_RED : C_LIGHTRED; break;
  case 32: c_fore = (!c_bold) ? C_GREEN : C_LIGHTGREEN; break;
  case 33: c_fore = (!c_bold) ? C_BROWN : C_LIGHTBROWN; break;
  case 34: c_fore = (!c_bold) ? C_BLUE : C_LIGHTBLUE; break;
  case 36: c_fore = (!c_bold) ? C_MAGENTA : C_LIGHTMAGENTA; break;
  case 35: c_fore = (!c_bold) ? C_CYAN : C_LIGHTCYAN; break;
  case 37: c_fore = (!c_bold) ? C_LIGHTGRAY : C_WHITE; break;
  case 39: c_fore = C_LIGHTGRAY; break; /* Reset to default */

  case 40: c_back = C_BLACK; break;
  case 41: c_back = C_RED; break;
  case 42: c_back = C_GREEN; break;
  case 43: c_back = C_BROWN; break;
  case 44: c_back = C_BLUE; break;
  case 45: c_back = C_MAGENTA; break;
  case 46: c_back = C_CYAN; break;
  case 47: c_back = C_LIGHTGRAY; break;
  case 49: c_back = C_LIGHTGRAY; break; /* Reset to default */

  default: break;
  }
}

static void flush_escape_buf() {
  int acc = 0;
  for (int i = 0; i < escape_buf_idx; ++i) {
    acc *= 10;
    acc += escape_buf[i] - '0';
  }
  escape_nums[escape_num_idx++] = acc;
  escape_buf_idx = 0;
}

/** Now we get to actually parse the escape sequences. The function ``handle_escape()`` should be called when an escape character is seen and for every character succeeding until it returns ``0`` (meaning the escape sequence has finished).

    The function will put numerals it sees into a buffer until it is sure the number is finished (when it sees a ';' or 'm' character), at which point it will call ``flush_escape_buf()`` to convert that to a proper integer and store it in ``escape_nums``. { */

static int handle_escape(char c) {
  switch (c) {
  case '[':
    return 1;
  case '0': case '1': case '2': case '3':
  case '4': case '5': case '6': case '7':
  case '8': case '9':
    escape_buf[escape_buf_idx++] = c;
    return 1;
  case ';':
    flush_escape_buf();
    return 1;

  case 'm':
    flush_escape_buf();
    for (int i = 0; i < escape_num_idx; ++i)
      handle_colour_escape(escape_nums[i]);

    /* Fall through */

  default:
    escape_buf_idx = 0;
    escape_num_idx = 0;
    /* No longer in an escape */
    return 0;
  }
}

/** Time for a little bit of magic: ``update_cursor()`` will send the right commands to the VGA chip to update the cursor position on-screen. { */

static void update_cursor() {
  uint16_t loc = cursor_y * 80 + cursor_x;

  outb(0x3D4, 14);
  outb(0x3D5, loc >> 8);
  outb(0x3D4, 15);
  outb(0x3D5, loc & 0xFF);
}

/** The function ``scroll()`` will scroll the screen by one line if the cursor is off the bottom of it, by memcpying (which should actually be ``memmove``, not ``memcpy``...) the video memory up one line and resetting the bottom line to all spaces. { */

static void scroll() {
  if (cursor_y >= 25) {
    memcpy((uint8_t*)video_memory,
           (uint8_t*)&video_memory[80], 2 * 80 * 24);
    memsetw(&video_memory[80 * 24],
            MAKE_CHAR(' ', c_back, c_back), 80);
    cursor_y = 24;
  }
}

/** We get to more logic now; here we put a character to the screen. I hope you should be able to follow through its logic without too much explanation. { */

static void putc(char c) {
  if (in_escape) {
    in_escape = handle_escape(c);
    return;
  }

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
    break;

  case '\033':
    /* VT220 escape */
    in_escape = 1;
    return;

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

/** And finally we come to clearing the screen (just copying the space character through the entirety of the framebuffer) and the console write function, which just calls ``putc`` in a loop.

    Done. { */

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
  register_console(&c);

  return 0;
}

static prereq_t prereqs[] = { {"console",NULL}, {NULL,NULL} };
static module_t run_on_startup x = {
  .name = "x86/screen",
  .required = prereqs,
  .load_after = NULL,
  .init = &register_screen,
  .fini = NULL
};
