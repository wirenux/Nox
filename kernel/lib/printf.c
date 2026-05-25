#include "printf.h"
#include "display.h"
#include "string.h"
#include "../include/types.h"

#define FONT_W  8
#define FONT_H  16
#define COLOR_FG 0xFFFFFF
#define COLOR_BG 0x000000

// Persistent cursor — survives across kprintf calls
static uint64_t cur_x = 0;
static uint64_t cur_y = 0;

// We need the fb to know when to wrap/scroll — store it on first use
static struct limine_framebuffer *cur_fb = NULL;

static void scroll_up(void) {
    if (!cur_fb) return;

    uint8_t *base     = (uint8_t *)(uintptr_t)cur_fb->address;
    uint64_t row_bytes = cur_fb->pitch;
    uint64_t font_rows = FONT_H * row_bytes;

    // Shift everything up by one text row
    memmove(base,
            base + font_rows,
            (cur_fb->height - FONT_H) * row_bytes);

    // Clear the last text row
    memset(base + (cur_fb->height - FONT_H) * row_bytes,
           0,
           FONT_H * row_bytes);
}

static void newline(void) {
    cur_x = 0;
    cur_y += FONT_H;
    if (cur_fb && cur_y + FONT_H > cur_fb->height) {
        // Instead of resetting to top, scroll up one line
        scroll_up();
        cur_y -= FONT_H;  // stay on the last line
    }
}

static void put_char(char c) {
    if (!cur_fb) return;

    if (c == '\n') {
        newline();
        return;
    }
    if (c == '\r') {
        cur_x = 0;
        return;
    }
    if (c == '\t') {
        // Align to next 4-character tab stop
        cur_x = (cur_x + 4 * FONT_W) & ~(4 * FONT_W - 1);
        return;
    }
    
    if (c == '\b') {
        if (cur_x >= FONT_W) {
            cur_x -= FONT_W;
        } else if (cur_y >= FONT_H) {
            // Go up a line — approximate, good enough for a terminal
            cur_y -= FONT_H;
            cur_x = (cur_fb->width / FONT_W) * FONT_W - FONT_W;
        }
        // Draw a space to blank the character
        fb_draw_char8x16(cur_fb, cur_x, cur_y, ' ', COLOR_FG, COLOR_BG);
        return;
    }

    // Wrap if we hit the right edge
    if (cur_x + FONT_W > cur_fb->width)
        newline();

    fb_draw_char8x16(cur_fb, cur_x, cur_y, c, COLOR_FG, COLOR_BG);
    cur_x += FONT_W;
}

static void put_string(const char *s) {
    while (*s) put_char(*s++);
}

// Prints an unsigned 64-bit integer in base 10
static void put_uint(uint64_t val) {
    char buf[20];  // max uint64 is 18446744073709551615 = 20 digits
    int  i = 0;

    if (val == 0) {
        put_char('0');
        return;
    }
    // Build digits in reverse
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    // Print in correct order
    while (--i >= 0)
        put_char(buf[i]);
}

// Prints a signed 64-bit integer
static void put_int(int64_t val) {
    if (val < 0) {
        put_char('-');
        // Cast carefully: -INT64_MIN would overflow
        put_uint((uint64_t)(-(val + 1)) + 1);
    } else {
        put_uint((uint64_t)val);
    }
}

// Prints an unsigned 64-bit integer in hex
// width: minimum digits (0-padded), 0 = no padding
static void put_hex(uint64_t val, int width, bool prefix) {
    static const char digits[] = "0123456789abcdef";
    char buf[16];
    int  i = 0;

    if (val == 0) {
        buf[i++] = '0';
    } else {
        while (val > 0) {
            buf[i++] = digits[val & 0xF];
            val >>= 4;
        }
    }
    // Zero-pad to requested width
    while (i < width)
        buf[i++] = '0';

    if (prefix) { put_char('0'); put_char('x'); }

    while (--i >= 0)
        put_char(buf[i]);
}

// --------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------

// Call this once before any kprintf, when you have the framebuffer.
void kprintf_init(struct limine_framebuffer *fb) {
    cur_fb = fb;
    cur_x  = 0;
    cur_y  = 0;
}

void kvprintf(const char *fmt, va_list args) {
    if (!cur_fb) return;

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            put_char(*p);
            continue;
        }

        p++;  // skip '%'

        // Parse optional zero-padding width: %08x etc.
        int width = 0;
        bool zero_pad = false;
        if (*p == '0') {
            zero_pad = true;
            p++;
            while (*p >= '0' && *p <= '9') {
                width = width * 10 + (*p - '0');
                p++;
            }
        }
        (void)zero_pad; // used via width below

        switch (*p) {
            case 'd':
                put_int((int64_t)va_arg(args, int));
                break;
            case 'u':
                put_uint((uint64_t)va_arg(args, unsigned int));
                break;
            case 'l':
                // %lu or %ld
                p++;
                if (*p == 'u') put_uint(va_arg(args, uint64_t));
                else if (*p == 'd') put_int(va_arg(args, int64_t));
                break;
            case 'x':
                put_hex((uint64_t)va_arg(args, unsigned int), width, false);
                break;
            case 'X':
                put_hex((uint64_t)va_arg(args, unsigned int), width, false);
                break;
            case 'p':
                // Pointer: always 0x + 16 hex digits
                put_hex((uint64_t)va_arg(args, void *), 16, true);
                break;
            case 's':
                put_string(va_arg(args, const char *));
                break;
            case 'c':
                put_char((char)va_arg(args, int));
                break;
            case '%':
                put_char('%');
                break;
            default:
                // Unknown specifier — print literally
                put_char('%');
                put_char(*p);
                break;
        }
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);

    va_end(args);
}

void kprintf_clear(void) {
    if (!cur_fb) return;

    uint8_t *base = (uint8_t *)(uintptr_t)cur_fb->address;
    uint64_t size = cur_fb->pitch * cur_fb->height;

    // Zero the entire framebuffer
    memset(base, 0, size);

    // Reset cursor to top-left
    cur_x = 0;
    cur_y = 0;
}
