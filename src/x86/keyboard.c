#include "hal.h"
#include "string.h"
#include "types.h"
#include "x86/io.h"

#include "scantable.inc"

#define KB_STATUS_PORT 0x64
#define KB_DATA_PORT   0x60
#define KB_READY_BIT   0x01

#define KB_BUFSZ 32
#define KB_INTERRUPT_NUM IRQ(1)

typedef struct kb_state {
  uint32_t flags;
  uint8_t escaped;
  char *buffer, *buffer_start, *buffer_end;
  int buffer_length;
} kb_state_t;

static const char *try_scancode(uint32_t flag, uint8_t *table, int len,
                                uint8_t scancode, kb_state_t *state) {
  const char *s;
  if (!flag || state->flags & flag)
    if ( (s=lookup_scantable(table, len, scancode, state->escaped)) )
      return s;
  return NULL;
}

static const char *string_for_scancode(kb_state_t *state, uint8_t scancode) {
  const char *s;

  s = try_scancode(SCAN_SHIFT, scan_shift, scan_shift_len, scancode, state);
  if (s) return s;
  s = try_scancode(SCAN_CAPSLOCK, scan_caps, scan_caps_len, scancode, state);
  if (s) return s;
  s = try_scancode(SCAN_NUMLOCK, scan_numlock, scan_numlock_len, scancode, state);
  if (s) return s;
  return try_scancode(0, scan_default, scan_default_len, scancode, state);
}

static uint32_t flag_for_scancode(kb_state_t *state, uint8_t scancode) {
  return scan_flags[state->escaped ? 0x80|scancode : scancode];
}

static const char *process_scancode(kb_state_t *state, uint8_t scancode) {
  if (scancode == 0xe0 && !state->escaped) {
    state->escaped = 1;
    return NULL;
  }

  uint8_t is_break = scancode & 0x80;
  scancode &= 0x7F;

  uint32_t f;
  if ( (f=flag_for_scancode(state, scancode)) ) {
    int flags_to_toggle = SCAN_NUMLOCK | SCAN_SCROLLLOCK | SCAN_CAPSLOCK;
    if (!is_break) {
      state->flags ^= f & flags_to_toggle;
    }
    f &= ~flags_to_toggle;

    if (!is_break)
      state->flags |= f;
    else
      state->flags &= ~f;
    return NULL;
  }

  /* Only emit strings on key make, not break. */
  const char *str;
  if (!is_break) {
    str = string_for_scancode(state, scancode);
  }
  else
    str = NULL;

  state->escaped = 0;
  return str;
}

static char is_scancode_ready() {
  return (inb(KB_STATUS_PORT) & KB_READY_BIT) != 0;
}

static uint8_t get_scancode_nonblock() {
  return inb(KB_DATA_PORT);
}

static uint8_t get_scancode_block() {
  while (!is_scancode_ready())
    ;
  return get_scancode_nonblock();
}

static int read_from_buffer(kb_state_t *state, char *buf, int len) {
  if (state->buffer_start == state->buffer_end) return 0;

  int n = 0;
  while (state->buffer_start != state->buffer_end && n < len) {
    buf[n++] = *state->buffer_start++;

    if (state->buffer_start >= (state->buffer+state->buffer_length))
      state->buffer_start -= state->buffer_length;
  }
  return n;
}

static void write_to_buffer(kb_state_t *state, const char *buf, int len) {
  for (int i = 0; i < len; ++i) {
    *state->buffer_end++ = buf[i];

    if (state->buffer_end >= (state->buffer+state->buffer_length))
      state->buffer_end -= state->buffer_length;
  }
}

static int read_block(console_t *obj, char *buf, int len) {
  if (len == 0) return 0;

  kb_state_t *state = (kb_state_t*)obj->data;

  int n = read_from_buffer(state, buf, len);
  if (n) return n;

  const char *str = NULL;
  uint8_t sc;
  while (str == NULL) {
    sc = get_scancode_block();
    str = process_scancode(state, sc);
  }

  /* Special, faster case for a single character */
  if (str[1] == '\0') {
    *buf = str[0];
    return 1;
  }

  write_to_buffer(state, str, strlen(str));

  return read_from_buffer(state, buf, len);
}
  
static int read_nonblock(console_t *obj, char *buf, int len) {
  if (len == 0) return 0;
  kb_state_t *state = (kb_state_t*)obj->data;
  return read_from_buffer(state, buf, len);
}

static int kb_int_handler(struct regs *regs, void *p) {
  kb_state_t *state = (kb_state_t*)p;

  uint8_t sc = get_scancode_block();
  const char *str = process_scancode(state, sc);
  write_to_buffer(state, str, strlen(str));

  return 0;
}

static char kb_buffer[KB_BUFSZ];

static kb_state_t kb_state = {
  .flags = 0,
  .escaped = 0,
  .buffer_start = kb_buffer,
  .buffer_end = kb_buffer,
  .buffer = kb_buffer,
  .buffer_length = KB_BUFSZ
};

static console_t kb_console = {
  .open = NULL,
  .close = NULL,
  .read = &read_block,
  .write = NULL,
  .flush = NULL,
  .data = (void*)&kb_state
};

static int register_keyboard() {
  /* If we have interrupts set up, use them. Else fall back to the blocking
     read method. */
  if (register_interrupt_handler(KB_INTERRUPT_NUM, &kb_int_handler,
                                 (void*)&kb_state) != -1)
    kb_console.read = &read_nonblock;

  register_console(&kb_console, 1);

  /* Clear out whatever byte may be loitering in the KB buffer for
     initialisation */
  if (is_scancode_ready())
    (void)get_scancode_nonblock();

  return 0;
}

const char *prereqs[] = {"console", "x86/screen", "x86/serial", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "x86/keyboard",
  .prerequisites = prereqs,
  .fn = &register_keyboard
};
