/**
   The kernel console
   ==================

   After two rather long chapters - one explaining the intricacies of how this
   project is structured, the other how to build bare metal - I'm happy to say
   that this part of the walkthrough is rather short.

   The initial aim with a kernel should be getting output out of it as quickly
   as possible - so you know what's actually going on. There are several ways of
   having the kernel communicate with the outside world - you can print to the
   screen (the most common way), send characters down a serial line (very useful
   in headless situations), or perhaps write to a logfile (perhaps less useful
   until the kernel has booted sufficiently!)

   Our aim for this chapter is to build the mechanism that allow us to write to
   one of these devices, and to read from input devices (keyboard, serial).

   The idea is to create a multiplexer - modules can register with this
   multiplexer and say "I know how to output data". Then, when the user wants to
   log some information, this is sent to all registered modules, broadcast
   style.

   Similarly modules can register that they can receive input, so when input is
   required, it can be gained from a number of sources.

   In "hal.h", which we introduced in the first chapter, there is a structure
   for this and associated helper functions - ``console_t`` and
   friends. {src/include/hal.h,"typedef struct console","int read_console"} */

/** So let's start defining our console multiplexer. The ``console_t`` structure
    is a doubly-linked-list, so to register a new console all we have to do is
    push the given object onto the front of that list.

    Note we also follow our usual practice of using stuff before we've defined it -
    in this case spinlocks, which will be used once we have multithreading to
    synchronise calls to the console functions. { */

#include "hal.h"

/* The first console in a linked list. */
static console_t *consoles = NULL;

/* Lock for all console operations. */
static spinlock_t lock = SPINLOCK_RELEASED;

/* Registers a new console - declared in hal.h */
int register_console(console_t *c) {
  spinlock_acquire(&lock);

  if (consoles)
    consoles->prev = c;
  c->next = consoles;
  c->prev = NULL;
  
  consoles = c;

  /* If an open() function was provided, call it. */
  if (c->open)
    c->open(c);

  spinlock_release(&lock);
  return 0;
}

/** Similarly for unregistering a console - just search through the list of
    consoles and remove the offending item. Again we require thread
    synchronisation. { */

/* Unregisters a console - declared in hal.h */
void unregister_console(console_t *c) {
  spinlock_acquire(&lock);

  console_t *prev = NULL;
  console_t *this = consoles;

  /* Scan through the linked list looking for 'c'. */
  while (this) {
    if (this == c) {
      if (this->next)
        this->next->prev = prev;
      if (this->prev)
        this->prev->next = this->next;
      if (!prev)
        consoles = c;

      /* Found - call flush() then close() if they exist. */
      if (this->flush)
        this->flush(this);
      if (this->close)
        this->close(this);
      break;
    }
    prev = this;
    this = this->next;
  }

  spinlock_release(&lock);
}

/** Then we get to define writing and reading from the console. Writing is a
    simple broadcast operation - take the input and write it to all registered
    consoles. { */

/* Writes to a console - declared in hal.h */
void write_console(const char *buf, int len) {
  spinlock_acquire(&lock);
  console_t *this = consoles;
  while (this) {
    if (this->write)
      this->write(this, buf, len);
    this = this->next;
  }
  spinlock_release(&lock);
}
 
/** Reading is slightly different - the ``read()`` functions defined in
    ``console_t`` are supposed to be non-blocking. So we cycle through all
    registered consoles trying to find one for whom ``read()`` is defined and
    returns a number of bytes greater than zero (0 return value means no data was
    available). { */

/* Reads from a console - declared in hal.h */
int read_console(char *buf, int len) {
  if (len == 0) return 0;

  spinlock_acquire(&lock);
  console_t *this = consoles;
  while (this) {
    if (this->read) {
      int n = this->read(this, buf, len);
      if (n > 0) {
        spinlock_release(&lock);
        return n;
      }
    }
    this = this->next;
    if (!this) this = consoles;
  }
  spinlock_release(&lock);
  return -1;
}

/** Finally we define the function that will clean up any consoles active at
    time of shutdown and flush their contents, and define the ``module_t`` structure
    that will register us as a module! { */

/* Flush and close all consoles. */
static int shutdown_console() {
  console_t *this = consoles;
  while (this) {
    if (this->flush)
      this->flush(this);
    if (this->close)
      this->close(this);
    this = this->next;
  }
  return 0;
}

static module_t x run_on_startup = {
  .name = "console",
  .required = NULL,
  .load_after = NULL,
  .init = NULL,
  .fini = &shutdown_console,
};

/** That's it! But it currently doesn't do anything, so you should swiftly
    advance onto one of the next chapters to implement an output method. */
