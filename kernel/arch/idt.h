#pragma once
#include "../include/types.h"

// The CPU pushes this onto the stack when an exception fires.
// Our ISR stubs also push vector + error_code to complete the frame.
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;  // pushed by stub
    uint64_t vector;      // pushed by stub
    uint64_t error_code;  // pushed by CPU (or fake 0 by stub)
    uint64_t rip;         // pushed by CPU
    uint64_t cs;          // pushed by CPU
    uint64_t rflags;      // pushed by CPU
    uint64_t rsp;         // pushed by CPU
    uint64_t ss;          // pushed by CPU
} __attribute__((packed)) cpu_frame_t;

// IDT entry — 16 bytes
typedef struct {
    uint16_t offset_low;   // handler address bits 0–15
    uint16_t selector;     // GDT code segment selector
    uint8_t  ist;          // interrupt stack table offset (0 = none)
    uint8_t  type_attr;    // type + DPL + present
    uint16_t offset_mid;   // handler address bits 16–31
    uint32_t offset_high;  // handler address bits 32–63
    uint32_t reserved;     // must be zero
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_pointer_t;

// Type/attribute byte values
#define IDT_PRESENT      (1 << 7)
#define IDT_RING0        (0 << 5)
#define IDT_INTERRUPT    0xE   // disables interrupts on entry (what you want)
#define IDT_TRAP         0xF   // does not disable interrupts on entry

void idt_init(void);

// Called from isr.asm — dispatches to the right handler
void isr_dispatch(cpu_frame_t *frame);