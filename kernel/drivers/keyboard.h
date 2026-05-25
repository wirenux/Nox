#pragma once
#include <stdint.h>

// Initialise the keyboard driver — unmasks IRQ1
void keyboard_init(void);

// Called from isr_dispatch when vector == 33 (IRQ1)
void keyboard_irq_handler(void);

// Read one character from the keyboard buffer.
// Blocks (yields) until a key is available.
char keyboard_getchar(void);

// Returns 1 if there is a character waiting, 0 if the buffer is empty
int keyboard_has_char(void);