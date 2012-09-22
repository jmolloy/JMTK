#include "adt/ringbuf.h"
#include "hal.h"
#include "string.h"
#include "types.h"
#include "x86/io.h"

/**
   Serial I/O
   ==========

   While generally with desktop OSes most of the initial boot IO is done via the monitor and keyboard, the serial line can be extremely useful for headless boots, or more importantly getting textual data in and out of emulators (qemu's ``-serial`` option, for example).

   A serial device is standard on x86 machines, and is accessed via ``INB`` and ``OUTB`` instructions - its registers are located in the I/O space, not the memory address space. There are normally two serial controllers, and each controller can control two serial lines.

   The primary controller has its registers at 0x3f8 (for the first device) and 0x3e8 (for the second device). Similarly the second controller has registers at 0x2f8 and 0x2e8. */

/** Now, in this chapter I'm not going to go into all the gory details of the serial controller. It's mainly legacy and besides, that's what datasheets are for. With that said, here are a bunch of definitions! { */

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

/**
   You should get used to this. Most device drivers start with a shedload of
   #defines. Except the obfuscated ones, but they're unreadable anyway by
   design. We'll come back to some of the constants, but the important ones are:

     * ``SERIAL_RXTX`` .. ``SERIAL_SCRATCH``: These are offsets from the base
       register (for example 0x3f8) where to find specific registers.
     * ``SERIAL_RXTX``: This is the read/transmit buffer. Reading from this
       reads from the read buffer (referred to as 'rx') and writing writes to
       the transmit buffer ('tx').
     * ``SERIAL_LSTAT``: Line status register. This
       has two interesting bits: ``SERIAL_LSTAT_RECV_BIT`` and
       ``SERIAL_LSTAT_SEND_BIT``, which tell us if the device has data received
       (RECV_BIT) or is ready to transmit (SEND_BIT).

   Our serial driver isn't going to be stupid and block reading one byte at a
   time. If you have interrupts enabled, it should store characters received to
   a buffer so it can be read back in bulk at a later point.

   I've written an ADT (abstract data type) for this, a `ring buffer
   <http://en.wikipedia.org/Ring_buffer>`_ in
   ``src/include/adt/ringbuf.h``. Here's the header - implementing this is left
   as an exercise to the reader :) {src/include/adt/ringbuf.h,"",""} */

/** Let's get down to defining our serial driver. Each serial connection has a
    state, consisting of a buffer of received characters and the base register
    address. { */
#define SERIAL_BUFSZ 32

typedef struct serial_state {
  int base;
  char_ringbuf_t buf;
} serial_state_t;

/** Let's define some convenience functions for reading and writing
    registers. ``inb`` and ``outb`` are defined in ``src/include/x86/hal.h`` and
    provide access to the ``inb`` and ``outb`` assembly instructions that we can
    otherwise not use from plain C. { */

static uint8_t read_register(int base, int reg) {
  return inb(base+reg);
}
static void write_register(int base, int reg, uint8_t value) {
  outb(base+reg, value);
}

/** It's important to know if a line is actually connected or not. There is no
    fool proof way to do this (see the hack for qemu) but here is a decent go: { */

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

/** Knowing if there's data available is simple - read the LSTAT register and
    check if the RECV bit is set. If data is available, getting it is a matter of
    reading the RXTX register, and to send data you just wait (spin) until the SEND
    bit in LSTAT is set, then write to RXTX. { */

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

/** Now we can implement the base functions that we will register with the
    kernel console manager. read() simply reads from the state ringbuffer if data is
    available, else it will attempt to read at least one character from the line
    without blocking. { */

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

/** write() is even simpler - it merely sends each character down the line,
    synchronously. { */

static int write(console_t *obj, const char *buf, int len) {
  serial_state_t *state = (serial_state_t*)obj->data;

  for (int i = 0; i < len; ++i)
    send_data(state->base, buf[i]);
  return len;
}

/** open() is more complex. We need to set the device into a known state - the
    state we're aiming for is 115200 8N1, the most commonly used protocol. Note that
    serial is so primitive that there is no handshake to determine baud rate or
    protocol - you've got to hope you have both sides set up the same way...

    The sequence to set up the device involves a lot of constants and isn't
    interesting, so I'll skip explaining it. Check the datasheet or Google if
    you're seriously interested. { */

static int open(console_t *obj) {
  int base = ((serial_state_t*)obj->data)->base;

  read_register(base, SERIAL_INTEN);
  read_register(base, SERIAL_INTEN);
  read_register(base, SERIAL_INTEN);
  read_register(base, SERIAL_INTEN);
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

/** Now we have an (optional) IRQ handler. If we haven't set up interrupts yet
    (it's in a later chapter but affects this one), this will do nothing. But if we
    have, it will slurp data from the RXTX register and write it to the ring
    buffer. { */

static int serial_int_handler(struct regs *regs, void *p) {
  serial_state_t *state = (serial_state_t*)p;
  
  uint8_t data = get_data_block(state->base);
  char_ringbuf_write(&state->buf, (char*)&data, 1);

  return 0;
}

/** Now we get to the final registration code - here we create four serial
    states, four console instances and initialise them, registering them with the
    console manager. { */

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

static prereq_t prereqs[] = { {"console",NULL}, {NULL,NULL} };
static module_t x run_on_startup = {
  .name = "x86/serial",
  .required = prereqs,
  .load_after = NULL,
  .init = &register_serial,
  .fini = NULL
};

/** With this small amount of code, we should have a functioning serial
    device. Now, on to more complex things. */
