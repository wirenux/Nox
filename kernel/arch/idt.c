#include "idt.h"
#include "gdt.h"
#include "../lib/printf.h"
#include "../drivers/serial.h"
#include "../drivers/pit.h"
#include "../drivers/pic.h"
#include "../sched/sched.h"

#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES];
static idt_pointer_t idt_ptr;

// Names for the 32 CPU exceptions — useful for crash messages
static const char *exception_names[32] = {
    "Division Error",              // 0
    "Debug",                       // 1
    "Non-Maskable Interrupt",      // 2
    "Breakpoint",                  // 3
    "Overflow",                    // 4
    "Bound Range Exceeded",        // 5
    "Invalid Opcode",              // 6
    "Device Not Available",        // 7
    "Double Fault",                // 8
    "Coprocessor Segment Overrun", // 9
    "Invalid TSS",                 // 10
    "Segment Not Present",         // 11
    "Stack-Segment Fault",         // 12
    "General Protection Fault",    // 13
    "Page Fault",                  // 14
    "Reserved",                    // 15
    "x87 Floating-Point",          // 16
    "Alignment Check",             // 17
    "Machine Check",               // 18
    "SIMD Floating-Point",         // 19
    "Virtualization",              // 20
    "Control Protection",          // 21
    "Reserved",                    // 22
    "Reserved",                    // 23
    "Reserved",                    // 24
    "Reserved",                    // 25
    "Reserved",                    // 26
    "Reserved",                    // 27
    "Hypervisor Injection",        // 28
    "VMM Communication",           // 29
    "Security Exception",          // 30
    "Reserved",                    // 31
};

// Forward declarations — 256 stubs defined in isr.asm
extern void *isr_stub_table[256];

static void idt_set_entry(int vector, void *handler, uint8_t type_attr) {
    uint64_t addr = (uint64_t)handler;
    idt[vector].offset_low  = (uint16_t)(addr & 0xFFFF);
    idt[vector].selector    = GDT_KERNEL_CODE;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[vector].reserved    = 0;
}

void idt_init(void) {
    // Wire all 256 vectors to their stubs
    for (int i = 0; i < 256; i++) {
        idt_set_entry(i, isr_stub_table[i],
                      IDT_PRESENT | IDT_RING0 | IDT_INTERRUPT);
    }

    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1);
    idt_ptr.base  = (uint64_t)&idt;

    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
}

// ── Exception / IRQ dispatcher ────────────────────────────────────────
// Called from every ISR stub in isr.asm with a pointer to the stack frame.

void isr_dispatch(cpu_frame_t *frame) {
    if (frame->vector < 32) {
        // CPU exception - crash screen (unchanged)
        kprintf("\n*** KERNEL EXCEPTION ***\n");
        kprintf("  #%lu  %s\n",
                frame->vector, exception_names[frame->vector]);
        kprintf("  error_code: 0x%016lx\n", frame->error_code);
        kprintf("  rip:        0x%016lx\n", frame->rip);
        kprintf("  rsp:        0x%016lx\n", frame->rsp);
        kprintf("  rax: 0x%016lx  rbx: 0x%016lx\n", frame->rax, frame->rbx);
        kprintf("  rcx: 0x%016lx  rdx: 0x%016lx\n", frame->rcx, frame->rdx);
        serial_puts("*** KERNEL EXCEPTION ***\n");
        __asm__ volatile ("cli");
        for (;;) __asm__ volatile ("hlt");

    } else if (frame->vector < 48) {
        // Hardware IRQ
        uint8_t irq = frame->vector - 32;

        switch (irq) {
            case 0: pit_tick(); sched_tick(); break;   // timer
            // case 1: kbd_handler(); break; // keyboard — later
            default: break;
        }

        pic_send_eoi(irq);   // must always send EOI for hardware IRQs
    }
}
