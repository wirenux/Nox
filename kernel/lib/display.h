#ifndef DISPLAY_H
#define DISPLAY_H
#include "../include/limine.h"
#include <stdint.h>

uint32_t fb_make_color(struct limine_framebuffer *fb, uint8_t r, uint8_t g, uint8_t b);
void fb_putpixel(struct limine_framebuffer *fb, uint64_t x, uint64_t y, uint32_t color);
void fb_fill_rect(struct limine_framebuffer *fb, uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color);
void fb_draw_char(struct limine_framebuffer *fb2, uint64_t px, uint64_t py, char c, uint32_t col, uint32_t bg);
void fb_draw_string(struct limine_framebuffer *fb2, uint64_t px, uint64_t py, const char *s, uint32_t col, uint32_t bg);

#endif