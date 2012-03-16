#ifndef READLINE_H
#define READLINE_H

#include "types.h"

/** @file readline.h Line editing and completion facilities */

/** Callback for a completion function.

    @param buf Buffer containing the current line of text to complete from
    @param buflen The length of @p buf
    @param action Specifies the action to perform.
     - If action is -1, the function should return the number of possible
       completions casted to void*.
     - If action is >= 0, the function should return the completion string
       as a C-string casted to void*. */
typedef void* (*readline_completer_t)(const char *buf, size_t buflen,
                                      int action);

/** Read a line of input with prompt and line editing. This is similar
    to GNU readline's interface, but takes an output buffer and size as
    we can't use malloc.

    The optional completion function can be used to provide tab-completion.

    This implementation provides:
      - Cursor movement
      - Tab completion
      - History up to a programmable number of entries
      - Ctrl-K: Kill to end of line
      - Ctrl-W: Kill last word
      - Ctrl-A: Move to start of line
      - Ctrl-R: Move to end of line
      - Ctrl-C: Cancel

    The implementation is reentrant except for the history, which is global.
    Because of this it is not recommended to call readline() from multiple
    threads in parallel.

    @param[out] buf The buffer to store the resulting line in
    @param bufsz The maximum size of @p buf
    @param[in] prompt The prompt to show, e.g. "$ "
    @param c The hook for tab completion, or NULL */
void readline(char *buf, size_t bufsz, const char *prompt,
              readline_completer_t c);

#endif
