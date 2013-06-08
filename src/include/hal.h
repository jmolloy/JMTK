/**#2
   Hardware abstraction
   ====================

   The most important thing about our entire kernel is the interface between all
   of the core modules. This will be defined globally as the HAL or hardware
   abstraction layer.

   This allows target-agnostic parts of the kernel to link with and use the
   target-specific parts they need. All functions are going to be declared
   up-front, and then defined as "weak" in hal.c with stubbed, "do-nothing"
   implementations. A weak function is an ELF (executable and linking format,
   the canonical POSIX object file format) concept - it defines a function but
   allows it to be overridden at link time if another function exists with
   normal "global" linkage. The idea behind this is that when you add a new file
   with a *real* implementation of a function, that function is "global" and so
   overrides the "weak" implementation in hal.c without any intervention. So
   with this base, we can build a kernel that can have functionality added
   easily.

   This file contains interfaces for many parts of the kernel, and so explaining
   all of it at this point would require also explaining many concepts that will
   appear later, so I'll leave it undocumented for now and come back to pieces
   in later chapters.

 **/

#ifndef HAL_H
#define HAL_H

#include "types.h"

typedef struct spinlock {
  volatile unsigned val;
  volatile unsigned interrupts;
} spinlock_t;

/* Platform specific hal.h's are required to define the following:
     - void abort(), with attribute "noreturn".
     - void far_call(void *fn, uintptr_t stack), which must call 'fn' with
       the given stack.
     - typedef jmp_buf, which must have type "array of length 1", e.g.:
         typedef jmp_buf_impl jmp_buf[1];
       This trick was taken from the GNU header setjmp.h, and means jmp_buf
       can be treated as a pointer yet still have static storage.
     - struct regs, which is implementation defined and is passed to 
       interrupt handlers and the debugger. */
#if defined(X86)
# include "x86/hal.h"
#elif defined(HOSTED)
# include "hosted/hal.h"
#else
# error Unknown target!
#endif

/* Call to send the system into a panic. */
void panic(const char *message) __attribute__((noreturn));
void assert_fail(const char *cond, const char *file, int line)
  __attribute__((noreturn));
void kmain(int argc, char **argv);

/**#3

   That said, the first thing we are going to do is write code to load modules,
   so let's look at a small part - the functions and structures that define the
   interface between modules.

   This interface defines a structure ``module_t`` that we can use to
   define a function (for module initialisation or teardown) that must be run at
   startup or shutdown. The "name" field gives the name of the module, and
   "required" gives a (NULL-terminated) list of the functions this one
   requires before it can run. "fn" is the actual function itself.
   
   "load_after" gives the concept of a soft dependency. Names in this list are not
   required in order for this function to run, but if they are available to load
   then they must be run before this module is run. An example of using this is the
   console module - it does not require both the "screen" and "serial" modules but if
   either is available they must be loaded before console.

   A module makes an instance of one of these structs, and marks it 
   "run_on_startup". You'll see that the definition of
   this macro does some attribute magic - the same attribute magic I mentioned
   earlier - the variable this gets applied to gets put in the special
   '.modules' section.

   A module has a name, which must be unique, and a list of modules it requires.
   These are hard requirements - if a module in the required list is not
   available then the module loader will panic. There is also a soft requirement
   list, which will not cause a panic if it isn't available but if it *is*
   available, it must be loaded *before* this module. It also has two function
   pointers, one for the initialisation function and one for the termination
   function. Both are optional. {*/

/*******************************************************************************
 * Initialisation / finalisation function registration
 ******************************************************************************/

/* Module initialisation state - only really used by main.c */
enum module_state {
  MODULE_NOT_INITIALISED,
  MODULE_PREREQS_RESOLVED,
  MODULE_INIT_RUN,
  MODULE_FINI_RUN
};

/* A prerequisite of a module. A client should only fill in the 'name' member -
   the 'module' member is filled in by main.c. */
typedef struct prereq {
  const char *name;
  struct module *module;
} prereq_t;

/* Structure defining a function to be run either on startup or shutdown. */
typedef struct module {
  /* Members that should be initialised statically. */
  const char *name;        /* A unique identifier for this function.*/
  prereq_t *required;      /* Either NULL or a NULL-terminated list of
                              module names that are prerequisites of this. */
  prereq_t *load_after;    /* Either NULL or a NULL-terminated list of 
                              module names that are not hard prerequisites
                              but if available then this module must be loaded
                              after them */
  int (*init)(void);       /* The startup function to run. Returns 0 on success */
  int (*fini)(void);       /* The shutdown function to run. Returns 0 on success */

  uintptr_t state;
  uintptr_t padding[2];    /* Ensure size is a multiple of pointer size. */
} module_t;

/* Mark an instance of a module_t object as 'run_on_startup' to
   have it run on startup, e.g.:
  
     static module_t x run_on_startup = {...}; */
#define run_on_startup __attribute__((__section__("modules"),used))

/**#cut*/

/*******************************************************************************
 * Console
 *******************************************************************************/

typedef struct console {
  /* Initialise a console object. */
  int (*open)(struct console *obj);
  /* Finish a console object. */
  int (*close)(struct console *obj);
  /* Read from a console - if no data is available, return zero without blocking.
     Else return as much data as is available, up to 'len'. Return the
     number of bytes read, or -1 on error. */
  int (*read)(struct console *obj, char *buf, int len);
  /* Write to a console - return the number of bytes written, or -1 on
     failure. */
  int (*write)(struct console *obj, const char *buf, int len);
  /* Flush the write buffer of a console. */
  void (*flush)(struct console *obj);

  /* Intrusive linked list, for HAL's use only. */
  struct console *prev, *next;
  /* Implementation dependent data. */
  void *data;
} console_t;

/* Register 'c' as a new console. Returns zero on success. */
int register_console(console_t *c);
/* Unregister 'c' from the set of consoles. */
void unregister_console(console_t *c);
/* Write to all consoles. */
void write_console(const char *buf, int len);
/* Read from the default console into a buffer. If there is no data 
   available, this call will block. Returns the number of bytes actually
   read, or -1 on error. */
int read_console(char *buf, int len);

/*******************************************************************************
 * Interrupt handling
 ******************************************************************************/

/* Callback type for an interrupt handler. Takes a pointer to the target
   specific struct 'regs' containing the register contents when the interrupt
   was taken. Return nonzero if any changes you made to 'r' should be 
   reflected when the handler returns. 'p' is opaque data passed through
   from register_interrupt_handler. */
struct regs;
typedef int (*interrupt_handler_t)(struct regs *r, void *p);

/* Register a new interrupt handler on the given target specific interrupt
   identifer 'num'. There can be multiple handlers per interrupt identifier.
   Returns -1 on failure. */
int register_interrupt_handler(int num, interrupt_handler_t handler, void *p);
/* Unregisters the given interrupt handler from the given intterupt identifer.
   Returns -1 on failure. */
int unregister_interrupt_handler(int num, interrupt_handler_t handler, void *p);

/* Allows maskable interrupts to happen. */
void enable_interrupts();

/* Disallows maskable interrupts from happening. */
void disable_interrupts();

/* Reads the current interrupt state. 1 is enabled, 0 is disabled. */
int get_interrupt_state();

/* Sets the current interrupt state - 1 is enabled, 0 is disabled. */
void set_interrupt_state(int enable);

/*******************************************************************************
 * Debugging
 ******************************************************************************/

/* The maximum number of supported cores. */
#define MAX_CORES 256

/* The state of one core when debugging. */
typedef struct core_debug_state {
  struct regs *registers;
} core_debug_state_t;

/* A handler function for a debugger command. Receives the command given
   in cmd and can handle as appropriate, and the state of all cores
   in the system. The 'core' parameter is the current processor that
   the user is interested in. */
typedef void (*debugger_fn_t)(const char *cmd, core_debug_state_t *states, int core);

/* Cause a debug or breakpoint trap. */
void trap();

/* Invoke the debugger, from a debug/breakpoint trap interrupt handler. */
void debugger_trap(struct regs *regs);
/* Invoke the debugger, from an interrupt handler that is reporting abnormal 
   behaviour (i.e. not a trap or breakpoint exception).

   Description holds an implementation-defined string describing the error. */
void debugger_except(struct regs *regs, const char *description);

/* Registers a function for use in the debugger. */
int register_debugger_handler(const char *name, const char *help,
                              debugger_fn_t fn);

/* Perform a backtrace.
  
   If called with *data=0, return the IP location the current function was called
   from, and save implementation specific information in 'data'.

   Iteratively call until the return value is 0, at which point the backtrace
   is complete. */
uintptr_t backtrace(uintptr_t *data, struct regs *regs);

/* Set an instruction breakpoint. Returns -2 if this is not possible on the
   target, or -1 if it is supported but an error occurred.

   Otherwise the return value is an implementation defined number that
   can be passed to unset_insn_breakpoint. */
int set_insn_breakpoint(uintptr_t loc);
/* Removes an instruction breakpoint previously set with set_insn_breakpoint.
   Returns -2 if not supported, -1 on error or 0 on success. */
int unset_insn_breakpoint(int id);

/* Set a data read breakpoint. Returns -2 if this is not possible on the
   target, or -1 if it is supported but an error occurred.

   Otherwise the return value is an implementation defined number that
   can be passed to unset_read_breakpoint. */
int set_read_breakpoint(uintptr_t loc);
/* Removes a data read breakpoint previously set with set_read_breakpoint.
   Returns -2 if not supported, -1 on error or 0 on success. */
int unset_read_breakpoint(int id);

/* Set a data write breakpoint. Returns -2 if this is not possible on the
   target, or -1 if it is supported but an error occurred.

   Otherwise the return value is an implementation defined number that
   can be passed to unset_write_breakpoint. */
int set_write_breakpoint(uintptr_t loc);
/* Removes a data write breakpoint previously set with set_write_breakpoint.
   Returns -2 if not supported, -1 on error or 0 on success. */
int unset_write_breakpoint(int id);

/* Returns a string giving the name of the symbol the address 'addr' is in,
   and stores the offset from that symbol in '*offs'.

   Returns NULL if not supported or a symbol is unavailable. */
const char *lookup_kernel_symbol(uintptr_t addr, int *offs);

/* Given an implementation defined structure 'regs' (as given to an interrupt
   handler), fill in the given array of name-value pairs.

   names and values will have enough space to hold up to 'max' entries. The
   function will store names of registers and their equivalent values into these
   arrays, and return the number of registers saved.

   Returns -1 on failure. */
int describe_regs(struct regs *regs, int max, const char **names,
                  uintptr_t *values);

/********************************************************************************
 * Multiple processors
 *******************************************************************************/

/* Returns an implementation defined ID for the current processor.
   Returns -1 if not implemented. IDs are expected to be sequential
   starting from zero. It is not defined which CPU is zero. */
int get_processor_id();

/* Returns the number of processors in the system. Returns -1 if not
   implemented. */
int get_num_processors();

/* Returns a pointer to an array of processor IDs representing the processors
   in the system. */
int *get_all_processor_ids();

/* Returns an implementation defined value that can be passed to
   register_interrupt_handler to handle an inter-processor message/interrupt.

   Returns -1 if not implemented. */
int get_ipi_interrupt_num();

/* Given a struct regs from an interrupt handler registered to
   get_ipi_interrupt_num, returns the value that was passed to the
   send_ipi function. */
void *get_ipi_data(struct regs *r);

#define IPI_ALL -1
#define IPI_ALL_BUT_THIS -2

/* Sends an inter-processor interrupt - a message between cores. The value in
   'data' will be available to the receiving core via get_ipi_data.

   The special value -1 (IPI_ALL) for proc_id will send IPIs to all cores in the system.

   The value -2 (IPI_ALL_BUT_THIS) will send IPIs to all cores but this one. */
void send_ipi(int proc_id, void *data);

/********************************************************************************
 * Peripherals
 *******************************************************************************/

uint64_t get_timestamp();
void set_timestamp(uint64_t ts);

/* Registers a callback to be fired in 'num_millis' milliseconds. If periodic
   is nonzero, the callback should be fired every 'num_millis' milliseconds,
   else it should only be called once.

   Returns -1 on error, otherwise an ID to be passed to unregister_callback. */
int register_callback(uint32_t num_millis, int periodic, void (*cb)(void*),
                      void *data);
/* Unregisters a callback registered with register_callback. */
int unregister_callback(void (*cb)(void*));

/*******************************************************************************
 * Memory management
 ******************************************************************************/

#define PAGE_WRITE   1 /* Page is writable */
#define PAGE_EXECUTE 2 /* Page is executable */
#define PAGE_USER    4 /* Page is useable by user mode code (else kernel only) */
#define PAGE_COW     8 /* Page is marked copy-on-write. It must be copied if
                          written to. */

#define PAGE_REQ_NONE     0 /* No requirements on page location */
#define PAGE_REQ_UNDER1MB 1 /* Require that the returned page be < 0x100000 */
#define PAGE_REQ_UNDER4GB 2 /* Require that the returned page be < 0x10000000 */

/* Returns the (default) page size in bytes. Not all pages may be this size
   (large pages etc.) */
unsigned get_page_size();

/* Rounds an address up so that it is page-aligned. */
uintptr_t round_to_page_size(uintptr_t x);

/* Allocate a physical page of the size returned by get_page_size().
   This should only be used between calling init_physical_memory_early() and
   init_physical_memory(). */
uint64_t early_alloc_page();

/* Allocate a physical page of the size returned by get_page_size(), returning
   the address of the page in the physical address space. Returns ~0ULL on
   failure.

   'req' is one of the 'PAGE_REQ_*' flags, indicating a requirement on the
   address of the returned page. */
uint64_t alloc_page(int req);
/* Mark a physical page as free. Returns -1 on failure. */
int free_page(uint64_t page);

uint64_t alloc_pages(int req, size_t num);
int free_pages(uint64_t pages, size_t num);

/* Creates a new address space based on the current one and stores it in
   'dest'. If 'make_cow' is nonzero, all pages marked WRITE are modified so
   that they are copy-on-write. */
int clone_address_space(address_space_t *dest, int make_cow);

/* Switches address space. Returns -1 on failure. */
int switch_address_space(address_space_t *dest);

/* Returns the current address space. */
address_space_t *get_current_address_space();

/* Maps 'num_pages' * get_page_size() bytes from 'p' in the physical address
   space to 'v' in the current virtual address space.

   Returns zero on success or -1 on failure. */
int map(uintptr_t v, uint64_t p, int num_pages,
        unsigned flags);
/* Unmaps 'num_pages' * get_page_size() bytes from 'v' in the current virtual address
   space. Returns zero on success or -1 on failure. */
int unmap(uintptr_t v, int num_pages);

/* If 'v' has a V->P mapping associated with it, return 'v'. Else return
   the next page (multiple of get_page_size()) which has a mapping associated
   with it. */
uintptr_t iterate_mappings(uintptr_t v);

/* If 'v' is mapped, return the physical page it is mapped to
   and fill 'flags' with the mapping flags. Else return ~0ULL. */
uint64_t get_mapping(uintptr_t v, unsigned *flags);

/* Return 1 if 'v' is mapped, else 0, or -1 if not implemented. */
int is_mapped(uintptr_t v);

/* A range of memory, with a start and a size. */
typedef struct range {
  uint64_t start;
  uint64_t extent;
} range_t;

/* Initialise the virtual memory manager.
   
   Returns 0 on success or -1 on failure. */
int init_virtual_memory();

/* The possible stages of physical memory management initialisation. */
#define PMM_INIT_START 0
#define PMM_INIT_EARLY 1
#define PMM_INIT_FULL  2

/* Variable giving the current stage of physical memory management
   initialisation. */
extern unsigned pmm_init_stage;

/* Initialise the physical memory manager (stage 1), passing in a set
   of ranges and the maximum extent of physical memory
   (highest address + 1).

   The set of ranges will be copied, not mutated.

   Before calling this function, pmm_init_stage must be PMM_INIT_START.
   After calling this function, pmm_init_stage will be PMM_INIT_EARLY. */
int init_physical_memory_early(range_t *ranges, unsigned nranges,
                               uint64_t extent);

/* Initialise the physical memory manager (stage 2). This should be 
   done after the virtual memory manager is set up.
   
   Before calling this function, pmm_init_stage must be PMM_INIT_EARLY.
   After calling this function, pmm_init_stage will be PMM_INIT_FULL. */
int init_physical_memory();

/* Initialise the copy-on-write page reference counts. */
int init_cow_refcnts(range_t *ranges, unsigned nranges);

/* Increment the reference count of a copy-on-write page. */
void cow_refcnt_inc(uint64_t p);

/* Decrement the reference count of a copy-on-write page. */
void cow_refcnt_dec(uint64_t p);

/* Return the reference count of a copy-on-write page. */
unsigned cow_refcnt(uint64_t p);

/* Handle a page fault potentially caused by a copy-on-write access.

   'addr' is the address of the fault. 'error_code' is implementation 
   defined. Returns true if the fault was copy-on-write related and was
   handled, false if it still needs handling. */
bool cow_handle_page_fault(uintptr_t addr, uintptr_t error_code);

/*******************************************************************************
 * Devices
 ******************************************************************************/

/* Device ID type is a pure 32-bit number, made up of minor and major
   parts. */
typedef uint32_t dev_t;

/* A character device - a streaming device. */
typedef struct char_device {
  /* Read from a character device - if no data is available, block until some is.
     Else return as much data as is available, up to 'len'. Return the
     number of bytes read, or -1 on error. */
  int (*read)(struct char_device *obj, char *buf, uint64_t len);
  /* Write - return the number of bytes written, or -1 on
     failure. */
  int (*write)(struct char_device *obj, char *buf, uint64_t len);
  /* Flush the write buffer, if applicable. */
  void (*flush)(struct char_device *obj);
  /* Register a callback which will fire when the device has data available. */
  int (*register_callback)(struct char_device *obj, void (*cb)(void*),
                           void *cb_param);
  /* Unregister a callback. Both the callback pointer and its associated 
     parameter must match a previously registered callback. */
  int (*unregister_callback)(struct char_device *obj, void (*cb)(void*),
                             void *cb_param);

  /* Return a string describing the device. */
  void (*describe)(struct char_device *obj, char *buf, unsigned bufsz);

  /* The device ID */
  dev_t id;

  /* Implementation dependent */
  void *data;
} char_device_t;

/* A block device - a random access device. */
typedef struct block_device {
  /* Read up to 'len' bytes from 'offset' bytes on the device into 'buf'.
     Return -1 on failure, or the number of bytes read. */
  int (*read)(struct block_device *obj, uint64_t offset, void *buf, uint64_t len);
  /* Write - return the number of bytes written, or -1 on
     failure. */
  int (*write)(struct block_device *obj, uint64_t offset, void *buf, uint64_t len);
  /* Flush the write buffer, if applicable. */
  void (*flush)(struct block_device *obj);

  /* Return the size of the device, in bytes. */
  uint64_t (*length)(struct block_device *obj);

  /* Return a string describing the device. */
  void (*describe)(struct block_device *obj, char *buf, unsigned bufsz);

  /* The device ID */
  dev_t id;

  /* Implementation dependent */
  void *data;
} block_device_t;

/* Known device major numbers. */
#define DEV_MAJ_NULL 0
#define DEV_MAJ_ZERO 1
#define DEV_MAJ_HDA  2
#define DEV_MAJ_HDB  3
#define DEV_MAJ_HDC  4
#define DEV_MAJ_HDD  5
#define DEV_MAJ_SDA  6
#define DEV_MAJ_SDB  7
#define DEV_MAJ_SDC  8
#define DEV_MAJ_SDD  9

static inline unsigned minor(dev_t x) {
  return x & 0xFFFF;
}
static inline unsigned major(dev_t x) {
  return (x >> 16) & 0xFFFF;
}
static inline dev_t makedev(unsigned major, unsigned minor) {
  return ((major&0xFFFF) << 16) | (minor & 0xFFFF);
}

/* Registers 'dev' as a character device with ID 'id'. Returns 0 on success
   or -1 on failure. */
int register_char_device(dev_t id, char_device_t *dev);
/* Registers 'dev' as a block device with ID 'id'. Returns 0 on success
   or -1 on failure. */
int register_block_device(dev_t id, block_device_t *dev);
/* Unregisters the device associated with ID 'id', and returns an opaque
   pointer to the char_device_t or block_device_t. */
void *unregister_device(dev_t id);
/* Returns a character device object for the given id. If 'id' is not a
   registered character device, it returns NULL. */
char_device_t *get_char_device(dev_t id);
/* Returns a block device object for the given id. If 'id' is not a
   registered block device, it returns NULL. */
block_device_t *get_block_device(dev_t id);

/* Registers a function which will be called whenever a new block device
   is registered. Returns 0 on success, 1 on failure. */
int register_block_device_listener(void (*callback)(dev_t));

/********************************************************************************
 * Threading
 *******************************************************************************/

#define SPINLOCK_RELEASED {.val=0, .interrupts=0};
#define SPINLOCK_ACQUIRED {.val=1, .interrupts=0};

/* Initialise a spinlock to the released state. */
void spinlock_init(spinlock_t *lock);
/* Returns a new, initialised spinlock. */
spinlock_t *spinlock_new();
/* Acquire 'lock', blocking until it is available. */
void spinlock_acquire(spinlock_t *lock);
/* Release 'lock'. Nonblocking. */
void spinlock_release(spinlock_t *lock);

typedef struct semaphore {
  volatile unsigned val;

  spinlock_t queue_lock;
  struct thread *queue_head;
} semaphore_t;

/* Initialise a semaphore to the value zero. */
void semaphore_init(semaphore_t *s);
/* Returns a new semaphore, initialised to the value zero. */
semaphore_t *semaphore_new();
/* Reduce the semaphore by one - pend/wait/acquire. This may be a blocking 
   operation. */
void semaphore_wait(semaphore_t *s);
/* Increase the semaphore by one - post/signal/release. */
void semaphore_signal(semaphore_t *s);

typedef semaphore_t mutex_t;

/* Initialise a mutex to released. */
static inline void mutex_init(mutex_t *s) {
  semaphore_init(s);
  semaphore_signal(s);
}
/* Returns a new mutex, initialised to released. */
static inline mutex_t *mutex_new() {
  mutex_t *m = semaphore_new();
  semaphore_signal(m);
  return m;
}
/* Acquire the mutex. Blocking operation. */
static inline void mutex_acquire(mutex_t *s) {
  semaphore_wait(s);
}
/* Release the mutex. */
static inline void mutex_release(mutex_t *s) {
  semaphore_signal(s);
}

/* A readers-writers lock: multiple readers, one writer. */
typedef struct rwlock {
  semaphore_t r, w;
  spinlock_t lock;
  volatile unsigned readcount, writecount;
} rwlock_t;

/* Initialise a readers-writers lock. */
void rwlock_init(rwlock_t *l);
/* Acquires a rwlock for reading. */
void rwlock_read_acquire(rwlock_t *l);
/* Releases a rwlock from reading. */
void rwlock_read_release(rwlock_t *l);
/* Acquires a rwlock for writing. */
void rwlock_write_acquire(rwlock_t *l);
/* Releases a rwlock from writing. */
void rwlock_write_release(rwlock_t *l);

/* Saves the current location and register state for jumping back to
   with longjmp(). It returns 0 if returning directly, and nonzero
   if returning via longjmp(). */
int setjmp(jmp_buf buf) __attribute__((returns_twice));
/* Jumps to a location saved with setjmp(), causing it to return seemingly
   with 'val', which must be nonzero. */
void longjmp(jmp_buf buf, int val);

/* Given 'buf', copy all relevant state into 'r' so it can be used
   with the debugging functions. */
void jmp_buf_to_regs(struct regs *r, jmp_buf buf);

#endif // HAL_H
