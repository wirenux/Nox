#pragma once

// Initialize the serial port (COM1)
void serial_init(void);

// Send a single character
void serial_putc(char a);

// Send a full string
void serial_puts(const char *s);

// Clear the screen (term)
void serial_clear(void);