#include "printf.h"
#include "display.h"

void fb_draw_char(struct limine_framebuffer *fb, uint64_t px, uint64_t py, char c, uint32_t col, uint32_t bg);

static uint64_t print_uint(struct limine_framebuffer *fb, uint64_t x, uint64_t y, unsigned long long val) {
    char buf[20];
    int i = 0;
    if (val == 0) { buf[i++] = '0'; }
    while (val > 0) {
        buf[i++] = (val % 10) + '0';
        val /= 10;
    }
    while (--i >= 0) {
        fb_draw_char(fb, x, y, buf[i], 0xFFFFFF, 0x000000);
        x += 9;
    }
    return x; // Return the new cursor position
}
void kprintf(struct limine_framebuffer *fb, uint64_t x, uint64_t y, const char *format, ...) {
    va_list args;
    va_start(args, format);

    for (const char *p = format; *p != '\0'; p++) {
        if (*p == '%') {
            p++;
            if (*p == 'd' || *p == 'u') {
                print_uint(fb, x, y, va_arg(args, unsigned int));
            } else if (*p == 's') {
                char *s = va_arg(args, char *);
                while (*s) {
                    if (*s == '\n') { y += 10; x = 10; }
                    else { fb_draw_char(fb, x, y, *s, 0xFFFFFF, 0x000000); x += 9; }
                    s++;
                }
            }
        } else if (*p == '\n') {
            y += 10;
            x = 10;
        } else {
            fb_draw_char(fb, x, y, *p, 0xFFFFFF, 0x000000);
            x += 9;
        }
    }
    va_end(args);
}