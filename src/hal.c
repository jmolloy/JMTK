#include "hal.h"

#define weak __attribute__((__weak__))

static uint64_t timestamp;
unsigned pmm_init_stage = PMM_INIT_START;

void panic(const char *message) weak;
void panic(const char *message) {
  for(;;);
}

void assert_fail(const char *cond, const char *file, int line) weak;
void assert_fail(const char *cond, const char *file, int line) {
  for(;;);
}

void kmain(int argc, char **argv) weak;
void kmain(int argc, char **argv) {
  trap();
}

int register_console(console_t *c) weak;
int register_console(console_t *c) {
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
void enable_interrupts() weak;
void enable_interrupts() {
}
void disable_interrupts() weak;
void disable_interrupts() {
}
int get_interrupt_state() weak;
int get_interrupt_state() {
  return 1;
}
void set_interrupt_state(int enable) weak;
void set_interrupt_state(int enable) {
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
int register_debugger_handler(const char *name, const char *help,
                              debugger_fn_t fn) weak;
int register_debugger_handler(const char *name, const char *help,
                              debugger_fn_t fn) {
  return -1;
}
uintptr_t backtrace(uintptr_t *data, struct regs *regs) weak;
uintptr_t backtrace(uintptr_t *data, struct regs *regs) {
  return 0;
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
                  uintptr_t *values) weak;
int describe_regs(struct regs *regs, int max, const char **names,
                  uintptr_t *values) {
  return -1;
}

int get_processor_id() weak;
int get_processor_id() {
  return -1;
}
int get_num_processors() weak;
int get_num_processors() {
  return -1;
}
int *get_all_processor_ids() weak;
int *get_all_processor_ids() {
  return NULL;
}
int get_ipi_interrupt_num() weak;
int get_ipi_interrupt_num() {
  return -1;
}
void *get_ipi_data(struct regs *r) weak;
void *get_ipi_data(struct regs *r) {
  return NULL;
}
void send_ipi(int proc_id, void *data) weak;
void send_ipi(int proc_id, void *data) {
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

uint64_t alloc_page(int req) weak;
uint64_t alloc_page(int req) {
  return ~0ULL;
}
int free_page(uint64_t page) weak;
int free_page(uint64_t page) {
  return -1;
}
int clone_address_space(address_space_t *dest, int make_cow) weak;
int clone_address_space(address_space_t *dest, int make_cow) {
  return -1;
}
int switch_address_space(address_space_t *dest) weak;
int switch_address_space(address_space_t *dest) {
  return -1;
}
address_space_t *get_current_address_space() weak;
address_space_t *get_current_address_space() {
  return NULL;
}
int map(uintptr_t v, uint64_t p, int num_pages, unsigned flags) weak;
int map(uintptr_t v, uint64_t p, int num_pages, unsigned flags) {
  return -1;
}
int unmap(uintptr_t v, int num_pages) weak;
int unmap(uintptr_t v, int num_pages) {
  return -1;
}
uintptr_t iterate_mappings(uintptr_t v) weak;
uintptr_t iterate_mappings(uintptr_t v) {
  return ~0UL;
}
uint64_t get_mapping(uintptr_t v, unsigned *flags) weak;
uint64_t get_mapping(uintptr_t v, unsigned *flags) {
  return ~0ULL;
}
int is_mapped(uintptr_t v) weak;
int is_mapped(uintptr_t v) {
  return -1;
}

int init_virtual_memory(range_t *ranges, unsigned nranges) weak;
int init_virtual_memory(range_t *ranges, unsigned nranges) {
  return -1;
}

int init_physical_memory(range_t *ranges, unsigned nranges, uint64_t extent) weak;
int init_physical_memory(range_t *ranges, unsigned nranges, uint64_t extent) {
  return -1;
}

int init_cow_refcnts(range_t *ranges, unsigned nranges) weak;
int init_cow_refcnts(range_t *ranges, unsigned nranges) {
  return -1;
}

void cow_refcnt_inc(uint64_t p) weak;
void cow_refcnt_inc(uint64_t p) {
}

void cow_refcnt_dec(uint64_t p) weak;
void cow_refcnt_dec(uint64_t p) {
}

unsigned cow_refcnt(uint64_t p) weak;
unsigned cow_refcnt(uint64_t p) {
  return ~0U;
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
int register_block_device_listener(void (*callback)(dev_t)) weak;
int register_block_device_listener(void (*callback)(dev_t)) {
  return -1;
}

int setjmp(jmp_buf buf) weak;
int setjmp(jmp_buf buf) {
  return -1;
}
void longjmp(jmp_buf buf, int val) weak;
void longjmp(jmp_buf buf, int val) {
  for(;;);
}
