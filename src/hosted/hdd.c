#include "stdio.h"
#include "hal.h"
#include "string.h"
#include "assert.h"
#include "kmalloc.h"

static FILE *stream;

int mock_hdd_read(block_device_t *obj, uint64_t offset, void *buf, uint64_t len) {
  fseek(stream, offset, SEEK_SET);
  return fread(buf, 1, len, stream);
}

int mock_hdd_write(block_device_t *obj, uint64_t offset, void *buf, uint64_t len) {
  fseek(stream, offset, SEEK_SET);
  return fwrite(buf, 1, len, stream);
}

void mock_hdd_flush(block_device_t *obj) {
  fflush(stream);
}

uint64_t mock_hdd_length(block_device_t *obj) {
  fseek(stream, 0, SEEK_END);
  return ftell(stream);
}

void mock_hdd_describe(block_device_t *obj, char *buf, unsigned bufsz) {
  strncpy(buf, "mock-hdd", bufsz);
}

static block_device_t mock_dev = {
  .read = &mock_hdd_read,
  .write = &mock_hdd_write,
  .flush = &mock_hdd_flush,
  .length = &mock_hdd_length,
  .describe = &mock_hdd_describe
};

int mock_hdd_init() {
  extern const char *getenv(const char *);
  const char *image = getenv("HDD_IMAGE");
  if (!image) {
    kprintf("hdd: No image loaded! (set env var HDD_IMAGE)\n");
    return 0;
  }

  stream = fopen(image, "r+");
  assert(stream && "Bad HDD image filename!");

  register_block_device(makedev(DEV_MAJ_HDA,0), &mock_dev);

  return 0;
}

static prereq_t p[] = { {"kmalloc", NULL}, {"console", NULL}, {"hosted/console",NULL}, {NULL,NULL} };

static module_t run_on_startup x = {
  .name = "hosted/hdd",
  .required = p,
  .load_after = NULL,
  .init = &mock_hdd_init,
  .fini = NULL
};
