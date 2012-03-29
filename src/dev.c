#include "hal.h"
#include "stdio.h"
#include "stdlib.h"
#include "assert.h"

#define MAX_DEVS 64

static block_device_t *block_devs[MAX_DEVS];
static unsigned num_block_devs = 0;

static const char *major_strs[] = {
  "null",
  "zero",
  "hda", "hdb", "hdc", "hdd",
  "sda", "sdb", "sdc", "sdd"
};

static void get_identifier_str(dev_t id, char *buf, unsigned bufsz) {
  if (minor(id) != 0)
    ksnprintf(buf, bufsz, "%s%d", major_strs[major(id)], minor(id));
  else
    ksnprintf(buf, bufsz, major_strs[major(id)]);
}

int register_block_device(dev_t id, block_device_t *dev) {
  assert(num_block_devs < MAX_DEVS && "Too many block devices registered!");
  
  dev->id = id;
  block_devs[num_block_devs++] = dev;

  char buf[64];
  buf[0] = '\0';
  if (dev->describe)
    dev->describe(dev, buf, 64);

  char identifier[8];
  get_identifier_str(id, identifier, 8);

  kprintf("dev: %s = %s\n", identifier, buf);

  return 0;
}

block_device_t *get_block_device(dev_t id) {
  for (unsigned i = 0; i < num_block_devs; ++i) {
    if (block_devs[i]->id == id)
      return block_devs[i];
  }
  return NULL;
}
