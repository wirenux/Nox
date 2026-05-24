#pragma once
#include "../include/types.h"

#define PIT_CHANNEL0 0x40   // Channel 0 data port — connected to IRQ0
#define PIT_CMD      0x43   // Mode/command register
#define PIT_BASE_HZ  1193182

// Call pit_init(100) for 100 Hz — one tick every 10ms
void pit_init(uint32_t hz);

// Read the current tick count — incremented by IRQ0 handler
uint64_t pit_get_ticks(void);

// Called from isr_dispatch when vector == 32 (IRQ0)
void pit_tick(void);