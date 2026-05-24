#pragma once
#include "../include/types.h"

// Master PIC ports
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21

// Slave PIC ports
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

// IRQ vector offsets after remapping
#define PIC_IRQ_MASTER_BASE 0x20   // IRQ0–7  -> vectors 32–39
#define PIC_IRQ_SLAVE_BASE  0x28   // IRQ8–15 -> vectors 40–47

#define PIC_EOI 0x20               // End Of Interrupt command

void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);