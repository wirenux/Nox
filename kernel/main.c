#include "include/limine.h"
#include "include/types.h"
#include "lib/printf.h"
#include "lib/display.h"
#include "drivers/serial.h"
#include "include/info.h"

__attribute__((used, section(".limine_requests")))
static volatile struct limine_bootloader_info_request bootloader_info_req = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
};

/* Terminal (deprecated Limine API) - convenient for printing text */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_terminal_request terminal_request = {
    .id = LIMINE_TERMINAL_REQUEST,
    .revision = 0,
};

void delay(uint64_t count) {
    for (uint64_t i = 0; i < count * 1000000; i++) {
        __asm__ volatile ("nop");
    }
}

void kmain(void) {
    struct limine_framebuffer *fb = NULL;

    // Initialize serial and print initial logs
    serial_init();
    serial_clear();
    serial_puts(nox_logo);
    serial_puts("Nox Kernel: Serial Initialized\n");

    // Fetch the primary framebuffer configuration from Limine
    if (framebuffer_request.response && framebuffer_request.response->framebuffer_count > 0) {
        fb = framebuffer_request.response->framebuffers[0];
    }

    if (fb) {
        // Draw the static text components first
        kprintf(fb, 0, 0, "%s", nox_logo);

        // Animation configuration
        int64_t side = 60;  // Starting size

        int64_t x = 20;
        int64_t y = 150;    // Shifted down slightly to start below the text logo
        int64_t dx = 4;     // X velocity
        int64_t dy = 4;     // Y velocity

        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 255;

        for (;;) {
            fb_fill_rect(fb, x, y, side, side, 0x000000);

            x += dx;
            y += dy;

            r = (r + 3) % 255;
            g = (g + 1) % 255;
            b = (b + 2) % 255;

            if (x <= 0) {
                x = 0;
                dx = -dx;
            } else if (x + side >= fb->width) {
                x = fb->width - side;
                dx = -dx;
            }

            if (y <= 0) {
                y = 0;
                dy = -dy;
            } else if (y + side >= fb->height) {
                y = fb->height - side;
                dy = -dy;
            }

            uint32_t current_color = fb_make_color(fb, r, g, b);
            fb_fill_rect(fb, x, y, side, side, current_color);

            delay(5);
        }
    }

    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}