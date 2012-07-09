#ifndef RINGBUF_H
#define RINGBUF_H

/* Ring buffer storing characters
 *
 * This ADT exposes a circular buffer, each item being of @c char type. It is minimal
 * and not in any way threadsafe. It has no dependencies. */

/* A ring buffer storing characters. */
typedef struct char_ringbuf {
  char *buffer, *buffer_start, *buffer_end;
  int buffer_length;
} char_ringbuf_t;


/* Create a new character ring buffer, using 'buffer' as memory, which is 'len' bytes
   long. */
char_ringbuf_t make_char_ringbuf(char *buffer, int len);

/* Read len characters from a char ring buffer. */
int char_ringbuf_read(char_ringbuf_t *state, char *buf, int len);

/* Write len characters to a char ring buffer. This does not guarantee that all
   elements were written successfully. */
void char_ringbuf_write(char_ringbuf_t *state, const char *buf, int len);

#endif
