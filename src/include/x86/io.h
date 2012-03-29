#ifndef X86_IO_H
#define X86_IO_H

#include "types.h"

#define IRQ(n) (n+32)

#define CR0_PG  (1U<<31)  /* Paging enable */
#define CR0_WP  (1U<<16)  /* Write-protect - allow page faults in kernel mode */

static inline void outb(uint16_t port, uint8_t value) {
  __asm__ volatile ("outb %1, %0" : : "dN" (port), "a" (value));
}

static inline void outw(uint16_t port, uint16_t value) {
  __asm__ volatile ("outw %1, %0" : : "dN" (port), "a" (value));
}

static inline void outl(uint16_t port, uint32_t value) {
  __asm__ volatile ("outl %1, %0" : : "dN" (port), "a" (value));
}

static inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  __asm__ volatile("inb %1, %0" : "=a" (ret) : "dN" (port));
  return ret;
}

static inline uint16_t inw(uint16_t port) {
  uint16_t ret;
  __asm__ volatile ("inw %1, %0" : "=a" (ret) : "dN" (port));
  return ret;
}

static inline uint32_t inl(uint16_t port) {
  uint32_t ret;
  __asm__ volatile ("inl %1, %0" : "=a" (ret) : "dN" (port));
  return ret;
}

static inline uint32_t read_cr0() {
  uint32_t ret;
  __asm__ volatile("mov %%cr0, %0" : "=r" (ret));
  return ret;
}
static inline uint32_t read_cr2() {
  uint32_t ret;
  __asm__ volatile("mov %%cr2, %0" : "=r" (ret));
  return ret;
}
static inline uint32_t read_cr3() {
  uint32_t ret;
  __asm__ volatile("mov %%cr3, %0" : "=r" (ret));
  return ret;
}

static inline void write_cr0(uint32_t val) {
  __asm__ volatile("mov %0, %%cr0" : : "r" (val));
}
static inline void write_cr2(uint32_t val) {
  __asm__ volatile("mov %0, %%cr2" : : "r" (val));
}
static inline void write_cr3(uint32_t val) {
  __asm__ volatile("mov %0, %%cr3" : : "r" (val));
}


#endif
