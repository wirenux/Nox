#ifndef PRINTF_H
#define PRINTF_H

#include <stdarg.h>
#include <stdint.h>
#include "../include/limine.h"

void kprintf(struct limine_framebuffer *fb, uint64_t x, uint64_t y, const char *format, ...);

#endif