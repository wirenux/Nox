#include "include/limine.h"
#include "include/types.h"
#include "include/info.h"
#include "lib/printf.h"
#include "lib/display.h"
#include "drivers/serial.h"
#include "drivers/pic.h"
#include "drivers/pit.h"
#include "arch/gdt.h"
#include "arch/idt.h"
#include "arch/cpu.h"
#include "mm/pmm.h"
#include "mm/vmm.h"

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

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_kernel_address_request kaddr_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
};

void kmain(void) {
    gdt_init();
    idt_init();
    serial_init();
    serial_clear();

    serial_puts(nox_logo);
    serial_puts("Nox Kernel: Serial initialized\n");

    // Framebuffer
    struct limine_framebuffer *fb = NULL;
    if (framebuffer_request.response &&
        framebuffer_request.response->framebuffer_count > 0) {
        fb = framebuffer_request.response->framebuffers[0];
    }

    kprintf_init(fb);

    if (!memmap_request.response || !hhdm_request.response) {
        kprintf("FATAL: no memory map from Limine\n");
        for (;;) __asm__ volatile ("hlt");
    }

    pmm_init(memmap_request.response, hhdm_request.response->offset);
    if (!kaddr_request.response) {
        kprintf("FATAL: no kernel address response\n");
        for (;;) __asm__ volatile ("hlt");
    }

    vmm_init(memmap_request.response, kaddr_request.response);
    pmm_print_stats();

    uint64_t p1 = pmm_alloc_page();
    uint64_t p2 = pmm_alloc_page();
    uint64_t p3 = pmm_alloc_page();
    kprintf("alloc: %p %p %p\n", (void*)p1, (void*)p2, (void*)p3);

    // Free the middle one and reallocate — should get p2 back
    pmm_free_page(p2);
    uint64_t p4 = pmm_alloc_page();
    kprintf("realloc: %p (should == %p)\n", (void*)p4, (void*)p2);

    // Clean up
    pmm_free_page(p1);
    pmm_free_page(p3);
    pmm_free_page(p4);
    pmm_print_stats();

    // Hardware interrupts
    pic_init();         // remap PIC before enabling any IRQs
    pit_init(100);      // 100 Hz timer
    cpu_sti();          // interrupts live from this point on

    // Boot messages
    kprintf("%s", nox_logo);
    kprintf("GDT + IDT + PIC + PIT ready\n");

    if (fb) {
        kprintf("Framebuffer: %lu x %lu @ %p\n",
                fb->width, fb->height, (void *)fb->address);
        kprintf("BPP: %u\n", (unsigned)fb->bpp);
    } else {
        kprintf("WARNING: no framebuffer — serial only\n");
        serial_puts("WARNING: no framebuffer\n");
    }

    // Idle loop — hlt sleeps until the next IRQ (the PIT tick)
    // This is the correct pattern: don't spin, let the CPU rest
    uint64_t last_sec = 0;
    uint64_t seconds  = 0;

    for (;;) {
        cpu_hlt();

        uint64_t t = pit_get_ticks();

        // 100 ticks = 1 second at 100 Hz
        if (t - last_sec >= 100) {
            last_sec = t;
            seconds++;
            kprintf("uptime: %lu second(s)\n", seconds);
        }
    }
}