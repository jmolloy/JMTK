#ifndef RINGBUF_H
#define RINGBUF_H

/** @file ringbuf.h A ring buffer ADT */

/** @addtogroup ringbuf Ring buffer storing characters
 *
 *  This ADT exposes a circular buffer, each item being of @c char type. It is minimal
 *  and not in any way threadsafe. It has no dependencies.
 *
 *  @{ */

/** A ring buffer storing characters. */
typedef struct char_ringbuf {
  char *buffer, *buffer_start, *buffer_end;
  int buffer_length;
} char_ringbuf_t;


/** Create a new character ring buffer.
    @param[in] buffer Pointer to the memory to use for the buffer
    @param len Length of the memory pointed to by @p buffer */
char_ringbuf_t make_char_ringbuf(char *buffer, int len);

/** Read @p len characters from a char ring buffer.
    @param state The buffer to read from
    @param[out] buf Pointer to an output buffer of length @p len
    @param len Length of the output buffer @p buf
    @return The number of characters read. This can be zero if the buffer was
    empty. */
int char_ringbuf_read(char_ringbuf_t *state, char *buf, int len);

/** Write @p len characters to a char ring buffer. This does not guarantee that all
    elements were written successfully.
    @param state The buffer to write to
    @param[in] buf Pointer to a buffer of length @p len
    @param len Length of the input buffer @p buf */
void char_ringbuf_write(char_ringbuf_t *state, const char *buf, int len);

/** @} */

#endif
