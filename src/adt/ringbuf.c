#include "adt/ringbuf.h"

char_ringbuf_t make_char_ringbuf(char *buffer, int buffer_length) {
  char_ringbuf_t s;
  s.buffer = s.buffer_start = s.buffer_end = buffer;
  s.buffer_length = buffer_length;
  return s;
}

int char_ringbuf_read(char_ringbuf_t *state, char *buf, int len) {
  if (state->buffer_start == state->buffer_end) return 0;

  int n = 0;
  while (state->buffer_start != state->buffer_end && n < len) {
    buf[n++] = *state->buffer_start++;

    if (state->buffer_start >= (state->buffer+state->buffer_length))
      state->buffer_start -= state->buffer_length;
  }
  return n;
}

void char_ringbuf_write(char_ringbuf_t *state, const char *buf, int len) {
  for (int i = 0; i < len; ++i) {
    *state->buffer_end++ = buf[i];

    /* If we are about to run off the end of the buffer, reset to the 
       start. */
    if (state->buffer_end >= (state->buffer+state->buffer_length))
      state->buffer_end -= state->buffer_length;
  }
}
