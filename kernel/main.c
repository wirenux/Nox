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

    kprintf_init(fb);

    if (framebuffer_request.response &&
        framebuffer_request.response->framebuffer_count > 0) {
        fb = framebuffer_request.response->framebuffers[0];
    }


    kprintf("%s", nox_logo);
    kprintf("Nox kernel booting...\n");
    kprintf("Framebuffer: %lu x %lu @ %p\n",
            fb->width, fb->height, (void *)fb->address);
    kprintf("BPP: %u\n", (unsigned)fb->bpp);

    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}