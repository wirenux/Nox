#include "include/limine.h"
#include "include/types.h"

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

static inline uint32_t fb_make_color(struct limine_framebuffer *fb, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t red = 0, green = 0, blue = 0;
    if (fb->red_mask_size >= 8)
        red = ((uint32_t)r) << fb->red_mask_shift;
    else
        red = ((uint32_t)r >> (8 - fb->red_mask_size)) << fb->red_mask_shift;

    if (fb->green_mask_size >= 8)
        green = ((uint32_t)g) << fb->green_mask_shift;
    else
        green = ((uint32_t)g >> (8 - fb->green_mask_size)) << fb->green_mask_shift;

    if (fb->blue_mask_size >= 8)
        blue = ((uint32_t)b) << fb->blue_mask_shift;
    else
        blue = ((uint32_t)b >> (8 - fb->blue_mask_size)) << fb->blue_mask_shift;

    return red | green | blue;
}

static inline void fb_putpixel(struct limine_framebuffer *fb, uint64_t x, uint64_t y, uint32_t color) {
    if (!fb || x >= fb->width || y >= fb->height)
        return;

    uint8_t *base = (uint8_t *)(uintptr_t)fb->address;
    uint64_t bpp_bytes = fb->bpp / 8;
    uint8_t *pix = base + y * fb->pitch + x * bpp_bytes;

    if (bpp_bytes == 4) {
        *(uint32_t *)pix = color;
    } else if (bpp_bytes == 3) {
        pix[0] = color & 0xFF;
        pix[1] = (color >> 8) & 0xFF;
        pix[2] = (color >> 16) & 0xFF;
    } else if (bpp_bytes == 2) {
        *(uint16_t *)pix = (uint16_t)color;
    }
}

static inline void fb_fill_rect(struct limine_framebuffer *fb, uint64_t x, uint64_t y, uint64_t w, uint64_t h, uint32_t color) {
    if (!fb)
        return;

    uint64_t max_x = x + w;
    uint64_t max_y = y + h;
    if (x >= fb->width || y >= fb->height)
        return;

    if (max_x > fb->width) max_x = fb->width;
    if (max_y > fb->height) max_y = fb->height;

    for (uint64_t yy = y; yy < max_y; ++yy) {
        for (uint64_t xx = x; xx < max_x; ++xx) {
            fb_putpixel(fb, xx, yy, color);
        }
    }
}

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
    }

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
