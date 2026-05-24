#include "pic.h"
#include "../arch/cpu.h"

// A small delay needed between PIC init commands on real hardware.
// Writing to port 0x80 (POST diagnostic port) is the classic way —
// it's a no-op that takes ~1 µs.
static inline void io_wait(void) {
    outb(0x80, 0);
}

void pic_init(void) {
    // Save current masks so we can restore them after remapping
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    // ICW1 — start initialisation sequence, expect ICW4
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();

    // ICW2 — set vector offsets
    outb(PIC1_DATA, PIC_IRQ_MASTER_BASE); io_wait(); // IRQ0 → vector 32
    outb(PIC2_DATA, PIC_IRQ_SLAVE_BASE);  io_wait(); // IRQ8 → vector 40

    // ICW3 — tell master/slave how they are wired together
    outb(PIC1_DATA, 0x04); io_wait(); // master: slave on IRQ2 (bit 2)
    outb(PIC2_DATA, 0x02); io_wait(); // slave: cascade identity = 2

    // ICW4 — 8086 mode
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    // Restore masks — all IRQs masked by default (we unmask selectively)
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_send_eoi(uint8_t irq) {
    // IRQ8–15 came through the slave — notify slave first, then master
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t  bit;

    if (irq < 8) {
        port = PIC1_DATA;
        bit  = irq;
    } else {
        port = PIC2_DATA;
        bit  = irq - 8;
    }
    outb(port, inb(port) | (1 << bit));
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t  bit;

    if (irq < 8) {
        port = PIC1_DATA;
        bit  = irq;
    } else {
        port = PIC2_DATA;
        bit  = irq - 8;
    }
    outb(port, inb(port) & ~(1 << bit));
}