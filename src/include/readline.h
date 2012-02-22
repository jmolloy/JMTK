#ifndef READLINE_H
#define READLINE_H

#include "types.h"

/* Callback for a completion function. The current line is stored in
   (buf, buflen) - action specifies what to do.
   - If action is -1, the function should return the number of possible
     completions casted to void*.
   - If action is >= 0, the function should return the completion string
     as a C-string casted to void*. */
typedef void* (*readline_completer_t)(const char *buf, size_t buflen,
                                      int action);

/* Read a line of input with prompt and line editing. This is similar
   to GNU readline's interface, but takes an output buffer and size as
   we can't use malloc.

   The optional completion function can be used to provide tab-completion. */
void readline(char *buf, size_t bufsz, const char *prompt,
              readline_completer_t c);

#endif
