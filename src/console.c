#include "hal.h"

static console_t *consoles = NULL;

int register_console(console_t *c) {
  if (consoles)
    consoles->prev = c;
  c->next = consoles;
  c->prev = NULL;
  
  consoles = c;

  if (c->open)
    c->open(c);
  return 0;
}

void unregister_console(console_t *c) {
  console_t *prev = NULL;
  console_t *this = consoles;

  while (this) {
    if (this == c) {
      if (this->next)
        this->next->prev = prev;
      if (this->prev)
        this->prev->next = this->next;
      if (!prev)
        consoles = c;

      if (this->flush)
        this->flush(this);
      if (this->close)
        this->close(this);
      break;
    }
    prev = this;
    this = this->next;
  }
}

void write_console(const char *buf, int len) {
  console_t *this = consoles;
  while (this) {
    if (this->write)
      this->write(this, buf, len);
    this = this->next;
  }
}

int read_console(char *buf, int len) {
  if (len == 0) return 0;

  console_t *this = consoles;
  while (this) {
    if (this->read) {
      int n = this->read(this, buf, len);
      if (n > 0) return n;
    }
    this = this->next;
    if (!this) this = consoles;
  }
  return -1;
}

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

static init_fini_fn_t x run_on_startup = {
  .name = "console",
  .prerequisites = NULL,
  .fn = NULL
};

static init_fini_fn_t y run_on_shutdown = {
  .name = "console",
  .prerequisites = NULL,
  .fn = &shutdown_console
};
