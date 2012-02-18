#include "hal.h"

#define weak __attribute__((__weak__))

static uint64_t timestamp;

void panic(const char *message) weak;
void panic(const char *message) {
}

int register_console(console_t *c, int default_for_reading) weak;
int register_console(console_t *c, int default_for_reading) {
  return -1;
}
void unregister_console(console_t *c) weak;
void unregister_console(console_t *c) {
}

void write_console(const char *buf, int len) weak;
void write_console(const char *buf, int len) {
}
int read_console(char *buf, int len) weak;
int read_console(char *buf, int len) {
  return -1;
}

int register_interrupt_handler(int num, interrupt_handler_t handler,
                               void *p) weak;
int register_interrupt_handler(int num, interrupt_handler_t handler,
                               void *p) {
  return -1;
}
int unregister_interrupt_handler(int num, interrupt_handler_t handler,
                                 void *p) weak;
int unregister_interrupt_handler(int num, interrupt_handler_t handler,
                                 void *p) {
  return -1;
}

void trap() weak;
void trap() {
}
void debugger_trap(struct regs *regs) weak;
void debugger_trap(struct regs *regs) {
}
void debugger_except(struct regs *regs, const char *description) weak;
void debugger_except(struct regs *regs, const char *description) {
}
uintptr_t backtrace(uintptr_t *data) weak;
uintptr_t backtrace(uintptr_t *data) {
  return -1;
}
int set_insn_breakpoint(uintptr_t loc) weak;
int set_insn_breakpoint(uintptr_t loc) {
  return -1;
}
int unset_insn_breakpoint(int id) weak;
int unset_insn_breakpoint(int id) {
  return -1;
}
int set_read_breakpoint(uintptr_t loc) weak;
int set_read_breakpoint(uintptr_t loc) {
  return -1;
}
int unset_read_breakpoint(int id) weak;
int unset_read_breakpoint(int id) {
  return -1;
}
int set_write_breakpoint(uintptr_t loc) weak;
int set_write_breakpoint(uintptr_t loc) {
  return -1;
}
int unset_write_breakpoint(int id) weak;
int unset_write_breakpoint(int id) {
  return -1;
}
const char *lookup_kernel_symbol(uintptr_t addr, int *offs) weak;
const char *lookup_kernel_symbol(uintptr_t addr, int *offs) {
  return NULL;
}
int describe_regs(struct regs *regs, int max, const char **names,
                  uintptr_t **values) weak;
int describe_regs(struct regs *regs, int max, const char **names,
                  uintptr_t **values) {
  return -1;
}

uint64_t get_timestamp() {
  return timestamp;
}
void set_timestamp(uint64_t ts) {
  timestamp = ts;
}

int register_callback(uint32_t num_millis, int periodic, void (*cb)(void*),
                      void *data) weak;
int register_callback(uint32_t num_millis, int periodic, void (*cb)(void*),
                      void *data) {
  return -1;
}

int unregister_callback(void (*cb)(void*)) weak;
int unregister_callback(void (*cb)(void*)) {
  return -1;
}

uint64_t alloc_page() weak;
uint64_t alloc_page() {
  return ~0ULL;
}
int free_page(uint64_t page) weak;
int free_page(uint64_t page) {
  return -1;
}
int clone_address_space(address_space_t *dest, address_space_t *src) weak;
int clone_address_space(address_space_t *dest, address_space_t *src) {
  return -1;
}
address_space_t *get_current_address_space() weak;
address_space_t *get_current_address_space() {
  return NULL;
}
int map(address_space_t *a, uintptr_t v, uint64_t p, int num_pages,
        unsigned flags) weak;
int map(address_space_t *a, uintptr_t v, uint64_t p, int num_pages,
        unsigned flags) {
  return -1;
}
int unmap(address_space_t *a, uintptr_t v, int num_pages) weak;
int unmap(address_space_t *a, uintptr_t v, int num_pages) {
  return -1;
}
uintptr_t iterate_mappings(address_space_t *a, uintptr_t v) weak;
uintptr_t iterate_mappings(address_space_t *a, uintptr_t v) {
  return ~0UL;
}
uint64_t get_mapping(address_space_t *a, uintptr_t v, unsigned *flags) weak;
uint64_t get_mapping(address_space_t *a, uintptr_t v, unsigned *flags) {
  return ~0ULL;
}
int is_mapped(address_space_t *a, uintptr_t v) weak;
int is_mapped(address_space_t *a, uintptr_t v) {
  return -1;
}

int register_char_device(dev_t id, char_device_t *dev) weak;
int register_char_device(dev_t id, char_device_t *dev) {
  return -1;
}
int register_block_device(dev_t id, block_device_t *dev) weak;
int register_block_device(dev_t id, block_device_t *dev) {
  return -1;
}
void *unregister_device(dev_t id) weak;
void *unregister_device(dev_t id) {
  return NULL;
}
char_device_t *get_char_device(dev_t id) weak;
char_device_t *get_char_device(dev_t id) {
  return NULL;
}
block_device_t *get_block_device(dev_t id) weak;
block_device_t *get_block_device(dev_t id) {
  return NULL;
}

int register_filesystem(const char *ident,
                        int (*probe)(dev_t dev, filesystem_t *fs)) weak;
int register_filesystem(const char *ident,
                        int (*probe)(dev_t dev, filesystem_t *fs)) {
  return -1;
}
int unregister_filesystem(const char *ident) weak;
int unregister_filesystem(const char *ident) {
  return -1;
}

int thread_setjmp(struct thread_target_state *buf) weak;
int thread_setjmp(struct thread_target_state *buf) {
  return -1;
}
void thread_longjmp(struct thread_target_state *buf, int val) weak;
void thread_longjmp(struct thread_target_state *buf, int val) {
}
