#pragma once
#include <stdint.h>

// Write a byte to a hardware I/O port
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

// Read a byte from a hardware I/O port
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

// Write a word (2 bytes) to a port
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

// Read a word from a port
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

// Halt the CPU until the next interrupt
static inline void cpu_hlt(void) {
    __asm__ volatile ("hlt");
}

// Enable interrupts
static inline void cpu_sti(void) {
    __asm__ volatile ("sti" : : : "memory");
}

// Disable interrupts
static inline void cpu_cli(void) {
    __asm__ volatile ("cli" : : : "memory");
}

// Flush a single TLB entry for a virtual address
// Needed later when you modify page table entries
static inline void cpu_invlpg(void *addr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

// Read a model-specific register
static inline uint64_t cpu_rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

// Write a model-specific register
static inline void cpu_wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)(val & 0xFFFFFFFF);
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}