#include "hal.h"
#include "string.h"
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>

int c_read(console_t *obj, char *buf, int len) {
  fd_set set;
  FD_ZERO(&set);
  FD_SET(0, &set);
  struct timeval tv;
  memset(&tv, 0, sizeof(struct timeval));

  if (select(1, &set, NULL, NULL, &tv) > 0) {
    return (int)read(0, buf, len);
  }
  return 0;
}

int c_write(console_t *obj, const char *buf, int len) {
  return (int)write(1, buf, len);
}

console_t c = {
  .open = NULL,
  .close = NULL,
  .read = &c_read,
  .write = &c_write,
  .flush = NULL,
  .data = NULL
};

static struct termios orig;
int init_console() {
  struct termios t;
  tcgetattr(1, &t);
  tcgetattr(1, &orig);

  t.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ICANON);
  tcsetattr(1, TCSANOW, &t);

  register_console(&c);
  return 0;
}

int fini_console() {
  tcsetattr(1, TCSANOW, &orig);
  return 0;
}

static prereq_t p[] = { {"console",NULL}, {NULL,NULL} };
static module_t run_on_startup x = {
  .name = "hosted/console",
  .required = p,
  .load_after = NULL,
  .init = &init_console,
  .fini = &fini_console
};
