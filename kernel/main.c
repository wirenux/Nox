#include "include/limine.h"
#include "include/types.h"
#include "lib/printf.h"
#include "lib/display.h"

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

void kmain(void) {
    struct limine_framebuffer *fb = NULL;

    if (framebuffer_request.response && framebuffer_request.response->framebuffer_count > 0) {
        fb = framebuffer_request.response->framebuffers[0];
    }

    if (fb) {
        uint32_t color = fb_make_color(fb, 255, 0, 0); /* red */
        uint64_t side = (fb->width < fb->height ? fb->width : fb->height) / 3;
        uint64_t x = (fb->width - side) / 2;
        uint64_t y = (fb->height - side) / 2;
        fb_fill_rect(fb, x, y, side, side, color);
        /* draw persistent text on framebuffer */
        uint32_t fg = fb_make_color(fb, 255, 255, 255);
        uint32_t bg = fb_make_color(fb, 0, 0, 0);
        fb_draw_string(fb, 10, 10, "HELLO FROM KERNEL!", fg, bg);
    }

    kprintf(fb, 0, 0, "Hello, Kernel! Value: %d\nNext line.", 42);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
