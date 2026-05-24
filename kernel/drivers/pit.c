#include "pit.h"
#include "pic.h"
#include "../arch/cpu.h"

static volatile uint64_t ticks = 0;

void pit_init(uint32_t hz) {
    uint32_t divisor = PIT_BASE_HZ / hz;

    // Command byte:
    //   bits 7-6: channel 0
    //   bits 5-4: lobyte/hibyte access mode
    //   bits 3-1: mode 3 (square wave)
    //   bit 0:    binary (not BCD)
    outb(PIT_CMD, 0x36);

    // Send divisor low byte then high byte
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    // Unmask IRQ0 so the timer can actually fire
    pic_unmask_irq(0);
}

uint64_t pit_get_ticks(void) {
    return ticks;
}

void pit_tick(void) {
    ticks++;
}