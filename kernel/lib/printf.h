#pragma once
#include <stdarg.h>
#include "../include/limine.h"

// Call once at boot with the Limine framebuffer pointer.
// After this, kprintf() works with no arguments for fb/x/y.
void kprintf_init(struct limine_framebuffer *fb);

// Drop-in formatted print. Uses persistent cursor.
// Supports: %d %u %lu %ld %x %X %p %s %c %%
// Width: %08x style zero-padding works for %x/%X
void kprintf(const char *fmt, ...);
void kvprintf(const char *fmt, va_list args);

void kprintf_clear(void);
