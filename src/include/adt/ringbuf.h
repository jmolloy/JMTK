#ifndef RINGBUF_H
#define RINGBUF_H

typedef struct char_ringbuf102 {
  char *buffer, *buffer_start, *buffer_end;
  int buffer_length;
} char_ringbuf_t;

char_ringbuf_t make_char_ringbuf(char *buffer, int buffer_length);
int char_ringbuf_read(char_ringbuf_t *state, char *buf, int len);
void char_ringbuf_write(char_ringbuf_t *state, const char *buf, int len);

#endif
