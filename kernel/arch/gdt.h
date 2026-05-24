#pragma once
#include "../include/types.h"

// A GDT entry - 8 bytes, packed to prevent compiler padding
typedef struct {
    uint16_t limit_low;   // limit bits 0–15
    uint16_t base_low;    // base  bits 0–15
    uint8_t  base_mid;    // base  bits 16–23
    uint8_t  access;      // present, DPL, type
    uint8_t  granularity; // flags (4 bits) + limit bits 16–19
    uint8_t  base_high;   // base  bits 24–31
} __attribute__((packed)) gdt_entry_t;

// Passed to lgdt — address + size of the GDT
typedef struct {
    uint16_t limit;       // sizeof(gdt) - 1
    uint64_t base;        // virtual address of the gdt array
} __attribute__((packed)) gdt_pointer_t;

// Segment selector constants - index * 8
// These are what you load into CS, DS, SS etc.
#define GDT_KERNEL_CODE  0x08
#define GDT_KERNEL_DATA  0x10
#define GDT_USER_CODE    0x18
#define GDT_USER_DATA    0x20

void gdt_init(void);