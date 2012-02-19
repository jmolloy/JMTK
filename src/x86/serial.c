#include "adt/ringbuf.h"
#include "hal.h"
#include "string.h"
#include "types.h"
#include "x86/io.h"

#define SERIAL_BASE_COM1 0x3f8
#define SERIAL_BASE_COM2 0x2f8
#define SERIAL_BASE_COM3 0x3e8
#define SERIAL_BASE_COM4 0x2e8

#define SERIAL_IRQ_COM1 IRQ(4)
#define SERIAL_IRQ_COM2 IRQ(3)
#define SERIAL_IRQ_COM3 IRQ(4)
#define SERIAL_IRQ_COM4 IRQ(3)

#define SERIAL_RXTX    0
#define SERIAL_INTEN   1
#define SERIAL_IIFIFO  2
#define SERIAL_LCTRL   3
#define SERIAL_MCTRL   4
#define SERIAL_LSTAT   5
#define SERIAL_MSTAT   6
#define SERIAL_SCRATCH 7

/* These alias RXTX and INTEN - when the DLAB is set in LCTRL, they
   act differently. */
#define SERIAL_BAUD_LO 0
#define SERIAL_BAUD_HI 1

#define SERIAL_LSTAT_RECV_BIT 0x01
#define SERIAL_LSTAT_SEND_BIT 0x20

#define SERIAL_BUFSZ 32

typedef struct serial_state {
  int base;
  char_ringbuf_t buf;
} serial_state_t;

void write_hex_byte(uint8_t x) {
  uint8_t n = x>>4;
  char c = (n >= 10) ? (n-10)+'A' : n+'0';
  write_console( &c, 1 );
  n = x&0xF;
  c = (n >= 10) ? (n-10)+'A' : n+'0';
  write_console( &c, 1 );
}

void write_hex_int(uint32_t x) {
  write_hex_byte( (x>>24) & 0xFF );
  write_hex_byte( (x>>16) & 0xFF );
  write_hex_byte( (x>>8) & 0xFF );
  write_hex_byte( x & 0xFF );
}

static uint8_t read_register(int base, int reg) {
  return inb(base+reg);
}
static void write_register(int base, int reg, uint8_t value) {
  outb(base+reg, value);
}

static uint8_t is_connected(int base) {
  /* Read the mstat register and look for clear to send and data set
     ready (0x30). The register appears to be 0xFF when the device isn't
     present. */
  uint8_t mstat = read_register(base, SERIAL_MSTAT);
  if ((mstat & 0x30) && (mstat != 0xFF))
    return 1;

  /* Hack for QEmu - QEmu doesn't change the mstat register based on if
     a serial port is connected or not. This can result in us reading
     from a nonconnected port which causes us to hang with infinite
     data. QEmu only uses one serial port, COM1. */
  if (base == SERIAL_BASE_COM1)
    return 1;

  /* Otherwise we aren't connected. */
  return 0;
}

static uint8_t is_data_ready(int base) {
  return (read_register(base, SERIAL_LSTAT) & SERIAL_LSTAT_RECV_BIT) != 0;
}

static uint8_t get_data_nonblock(int base) {
  return read_register(base, SERIAL_RXTX);
}

static void send_data(int base, uint8_t byte) {
  while ( (read_register(base, SERIAL_LSTAT) & SERIAL_LSTAT_SEND_BIT) == 0)
    ;
  write_register(base, SERIAL_RXTX, byte);
}

static uint8_t get_data_block(int base) {
  while (!is_data_ready(base))
    ;
  return get_data_nonblock(base);
}

static int read(console_t *obj, char *buf, int len) {
  if (len == 0) return 0;

  serial_state_t *state = (serial_state_t*)obj->data;

  int n = char_ringbuf_read(&state->buf, buf, len);
  if (n) return n;

  while (is_data_ready(state->base)) {
    uint8_t c = get_data_nonblock(state->base);
    char_ringbuf_write(&state->buf, (char*)&c, 1);
  }

  return char_ringbuf_read(&state->buf, buf, len);
}

static int write(console_t *obj, const char *buf, int len) {
  serial_state_t *state = (serial_state_t*)obj->data;

  for (int i = 0; i < len; ++i)
    send_data(state->base, buf[i]);
  return len;
}

static int open(console_t *obj) {
  int base = (int)obj->data;

  /* Disable all interrupts during init */
  write_register(base, SERIAL_INTEN, 0x00);
  /* Enable DLAB, to set the baud rate divisor. */
  write_register(base, SERIAL_LCTRL, 0x80);
  /* Set the divisor to 1, for 115200 baud. */
  write_register(base, SERIAL_BAUD_LO, 0x03);
  write_register(base, SERIAL_BAUD_HI, 0x00);
  /* Set to 8N1 - 8 bits, no parity, one stop bit */
  write_register(base, SERIAL_LCTRL, 0x03);
  /* Enable FIFO, clear them, with 14-byte threshold */
  write_register(base, SERIAL_IIFIFO, 0xc7);
  /* Enable IRQs, RTS/DSR set */
  write_register(base, SERIAL_MCTRL, 0x0b);
  /* Reenable all interrupts. */
  write_register(base, SERIAL_INTEN, 0x0C);
  
  return 0;
}

static int serial_int_handler(struct regs *regs, void *p) {
  serial_state_t *state = (serial_state_t*)p;
  
  uint8_t data = get_data_block(state->base);
  char_ringbuf_write(&state->buf, (char*)&data, 1);

  return 0;
}

static serial_state_t states[4];
static console_t consoles[4];
static char bufs[4][SERIAL_BUFSZ];
static int bases[4] = {SERIAL_BASE_COM1, SERIAL_BASE_COM2,
                       SERIAL_BASE_COM3, SERIAL_BASE_COM4};
static int irqs[4] = {SERIAL_IRQ_COM1, SERIAL_IRQ_COM2,
                       SERIAL_IRQ_COM3, SERIAL_IRQ_COM4};

static int register_serial() {
  /* FIXME: Make the interrupt handler look into the interrupt ident
     register to see which state it is! */
  for (int i = 0; i < /*4*/ 2; ++i) {
    if (!is_connected(bases[i]))
      continue;

    states[i].base = bases[i];
    states[i].buf = make_char_ringbuf(bufs[i], SERIAL_BUFSZ);

    consoles[i].open = &open;
    consoles[i].close = NULL;
    consoles[i].read = &read;
    consoles[i].write = &write;
    consoles[i].flush = NULL;
    consoles[i].data = (void*)&states[i];

    (void)register_console(&consoles[i]);

    (void)register_interrupt_handler(irqs[i], &serial_int_handler,
                                     (void*)&states[i]);
  }

  return 0;
}


static const char *prereqs[] = {"console", NULL};
static init_fini_fn_t x run_on_startup = {
  .name = "x86/serial",
  .prerequisites = prereqs,
  .fn = &register_serial
};
