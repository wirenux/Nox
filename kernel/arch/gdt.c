#include "gdt.h"

// ── Access byte values ────────────────────────────────────────────────
//
//  Bit 7   Present          must be 1
//  Bit 6-5 DPL              0 = kernel, 3 = user
//  Bit 4   Descriptor type  1 = code/data
//  Bit 3   Executable       1 = code, 0 = data
//  Bit 2   Direction/Conform 0 = normal
//  Bit 1   RW               1 = readable(code) / writable(data)
//  Bit 0   Accessed         leave 0, CPU sets it

#define ACCESS_PRESENT    (1 << 7)
#define ACCESS_RING0      (0 << 5)
#define ACCESS_RING3      (3 << 5)
#define ACCESS_NONSYSTEM  (1 << 4)   // code or data (not TSS)
#define ACCESS_EXEC       (1 << 3)   // code segment
#define ACCESS_RW         (1 << 1)   // readable / writable

// Shorthand for the two segment types we need
#define ACCESS_CODE  (ACCESS_PRESENT | ACCESS_NONSYSTEM | ACCESS_EXEC | ACCESS_RW)
#define ACCESS_DATA  (ACCESS_PRESENT | ACCESS_NONSYSTEM | ACCESS_RW)

// ── Granularity / flag byte ──────────────────────────────────────────
//
//  Bit 7  G   Granularity  1 = limit in 4K pages
//  Bit 6  DB  Default size 0 — MUST be 0 when L=1
//  Bit 5  L   Long mode    1 = 64-bit code segment
//  Bit 4      Reserved     0
//  Bits 3-0   Limit 16:19  set to 0xF (max, ignored anyway in 64-bit)

#define GRAN_4K       (1 << 7)
#define GRAN_LONG     (1 << 5)   // L bit — marks a 64-bit code segment
#define GRAN_LIMIT    (0x0F)     // limit bits 16–19, maximised

#define GRAN_CODE  (GRAN_4K | GRAN_LONG | GRAN_LIMIT)
#define GRAN_DATA  (GRAN_4K | GRAN_LIMIT)

// ── Table ────────────────────────────────────────────────────────────

#define GDT_ENTRIES 5

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_pointer_t gdt_ptr;

// Build one descriptor.
// In 64-bit mode base/limit are ignored for code+data, but we fill
// them correctly anyway — good practice, and required for the TSS later.
static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran) {
    gdt[i].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt[i].base_low    = (uint16_t)(base  & 0xFFFF);
    gdt[i].base_mid    = (uint8_t)((base  >> 16) & 0xFF);
    gdt[i].access      = access;
    gdt[i].granularity = (gran & 0xF0) | ((limit >> 16) & 0x0F);
    gdt[i].base_high   = (uint8_t)((base  >> 24) & 0xFF);
}

// Defined in arch/gdt.asm
extern void gdt_load(gdt_pointer_t *ptr);

void gdt_init(void) {
    // 0 — Null descriptor (mandatory, must be all zeros)
    gdt_set_entry(0, 0, 0, 0, 0);

    // 1 — Kernel code  (ring 0, 64-bit)
    gdt_set_entry(1, 0, 0xFFFFF, ACCESS_RING0 | ACCESS_CODE, GRAN_CODE);

    // 2 — Kernel data  (ring 0)
    gdt_set_entry(2, 0, 0xFFFFF, ACCESS_RING0 | ACCESS_DATA, GRAN_DATA);

    // 3 — User code    (ring 3, 64-bit)
    gdt_set_entry(3, 0, 0xFFFFF, ACCESS_RING3 | ACCESS_CODE, GRAN_CODE);

    // 4 — User data    (ring 3)
    gdt_set_entry(4, 0, 0xFFFFF, ACCESS_RING3 | ACCESS_DATA, GRAN_DATA);

    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_ptr.base  = (uint64_t)&gdt;

    gdt_load(&gdt_ptr);
}