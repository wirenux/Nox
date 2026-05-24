#include "serial.h"
#include "../lib/io.h"

#define COM1 0x3f8

void serial_init(void) {
    outb(COM1 + 1, 0x00);    // Disable interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB
    outb(COM1 + 0, 0x03);    // Set divisor 3 (38400 baud)
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, 1 stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

int is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char a) {
    while (is_transmit_empty() == 0);
    outb(COM1, a);
}

void serial_puts(const char *s) {
    while (*s) {
        serial_putc(*s++);
    }
}

void serial_clear(void) {
    serial_puts("\033[2J"); // Clear screen
    serial_puts("\033[H");  // Move cursor to top-left
}