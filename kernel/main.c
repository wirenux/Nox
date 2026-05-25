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
#include "mm/heap.h"
#include "sched/sched.h"
#include "drivers/keyboard.h"

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

/* Test threads (file-scope) */
static void test_thread_a(void *arg) {
    (void)arg;
    for (;;) {
        kprintf("Thread A running (id=%lu)\n", sched_current()->id);
        sched_sleep(1000);  /* sleep 1 second */
    }
}

static void test_thread_b(void *arg) {
    (void)arg;
    for (;;) {
        kprintf("Thread B running (id=%lu)\n", sched_current()->id);
        sched_sleep(1500);  /* sleep 1.5 seconds */
    }
}

static void test_keyboard(void *arg) {
    (void)arg;
    kprintf("keyboard: type something!\n");
    for (;;) {
        char c = keyboard_getchar();  // blocks until key pressed
        kprintf("key: '%c' (0x%x)\n", c, (unsigned)c);
    }
}

void kmain(void) {
    // CPU structures
    gdt_init();
    idt_init();

    // Early output
    serial_init();
    serial_clear();
    serial_puts("Nox Kernel: Serial initialized\n");

    struct limine_framebuffer *fb = NULL;
    if (framebuffer_request.response &&
        framebuffer_request.response->framebuffer_count > 0) {
        fb = framebuffer_request.response->framebuffers[0];
    }
    kprintf_init(fb);
    kprintf("%s", nox_logo);

    // Memory management
    if (!memmap_request.response || !hhdm_request.response) {
        kprintf("FATAL: no memory map from Limine\n");
        for (;;) cpu_hlt();
    }

    pmm_init(memmap_request.response, hhdm_request.response->offset);
    pmm_print_stats();

    if (!kaddr_request.response) {
        kprintf("FATAL: no kernel address response\n");
        for (;;) cpu_hlt();
    }

    vmm_init(memmap_request.response, kaddr_request.response);
    heap_init();
    sched_init();

    // PMM self-test
    uint64_t p1 = pmm_alloc_page();
    uint64_t p2 = pmm_alloc_page();
    uint64_t p3 = pmm_alloc_page();
    kprintf("[PMM test] alloc: %p %p %p\n", (void*)p1, (void*)p2, (void*)p3);
    pmm_free_page(p2);
    uint64_t p4 = pmm_alloc_page();
    kprintf("[PMM test] realloc: %p (should == %p)\n", (void*)p4, (void*)p2);
    pmm_free_page(p1);
    pmm_free_page(p3);
    pmm_free_page(p4);

    // Heap self-test
    void *a = kmalloc(64);
    void *b = kmalloc(128);
    void *c = kmalloc(32);
    kprintf("[heap test] a=%p b=%p c=%p\n", a, b, c);
    kfree(b);
    void *d = kmalloc(100);
    kprintf("[heap test] d=%p (should be near b=%p)\n", d, b);
    kfree(a); kfree(c); kfree(d);
    heap_print_stats();  // should show one large free block

    // Hardware interrupts
    pic_init();
    pit_init(100);
    cpu_sti();

    keyboard_init();

    // Boot info
    kprintf("GDT + IDT + PIC + PIT ready\n");
    if (fb) {
        kprintf("Framebuffer: %lu x %lu @ %p  BPP: %u\n",
                fb->width, fb->height, (void *)fb->address,
                (unsigned)fb->bpp);
    }

    // thread_create(test_thread_a, NULL);
    // thread_create(test_thread_b, NULL);

    thread_create(test_keyboard, NULL);

    // Idle loop
    uint64_t last_sec = 0;
    uint64_t seconds  = 0;
    for (;;) {
        cpu_hlt();
        uint64_t t = pit_get_ticks();
        if (t - last_sec >= 100) {
            last_sec = t;
            seconds++;
            kprintf("uptime: %lu second(s)\n", seconds);
        }
    }
}