#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "../lib/printf.h"

// The heap lives at a fixed virtual address in the kernel's address space.
// This range is reserved — nothing else should be mapped here.
// 1 GiB ceiling is more than enough for a learning kernel.
#define HEAP_START  0xFFFFFFFF90000000ull
#define HEAP_MAX    0xFFFFFFFFC0000000ull

// Minimum allocation size — keeps the list from fragmenting into
// thousands of tiny blocks
#define HEAP_MIN_ALLOC 32

// Sits immediately before every allocation.
// The user pointer is (block_header_t *)header + 1.
// sizeof(block_header_t) must itself be a multiple of 8 for alignment.

typedef struct block_header {
    size_t             size;   // size of the DATA region (not including header)
    int                free;   // 1 = free, 0 = used
    struct block_header *next; // next block in the list (NULL = last)
    struct block_header *prev; // previous block (NULL = first)
    uint64_t           magic;  // 0xCAFEBABECAFEBABE — detects corruption
} block_header_t;

#define HEAP_MAGIC 0xCAFEBABECAFEBABEull


static block_header_t *heap_head = NULL;  // first block in the list
static uint64_t        heap_end  = HEAP_START; // current top of mapped heap


// Expand the heap by mapping more physical pages into the heap VMA.
// Returns 1 on success, 0 on failure.
static int heap_grow(size_t min_bytes) {
    // Round up to whole pages
    size_t pages = (min_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    // Always grow by at least 16 pages (64 KiB) to amortise cost
    if (pages < 16) pages = 16;

    if (heap_end + pages * PAGE_SIZE > HEAP_MAX) {
        kprintf("heap: out of virtual address space\n");
        return 0;
    }

    // Get the kernel's PML4 — we stored it in vmm_init, expose it
    extern page_table_t *kernel_pml4;

    for (size_t i = 0; i < pages; i++) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) {
            kprintf("heap: out of physical memory\n");
            return 0;
        }
        vmm_map_page(kernel_pml4, heap_end, phys, PTE_KERNEL_RW);
        heap_end += PAGE_SIZE;
    }

    return 1;
}

// Initialise a block header at a given address
static block_header_t *make_block(void *addr, size_t size,
                                   block_header_t *prev,
                                   block_header_t *next) {
    block_header_t *b = (block_header_t *)addr;
    b->size  = size;
    b->free  = 1;
    b->prev  = prev;
    b->next  = next;
    b->magic = HEAP_MAGIC;
    return b;
}

// Check a header hasn't been corrupted — catches buffer overflows early
static void check_magic(block_header_t *b) {
    if (b->magic != HEAP_MAGIC) {
        kprintf("heap: CORRUPTION detected at %p\n", (void *)b);
        kprintf("heap: magic = 0x%016lx expected 0x%016lx\n",
                b->magic, HEAP_MAGIC);
        __asm__ volatile ("cli");
        for (;;) __asm__ volatile ("hlt");
    }
}

// Split block b into two: one of exactly `size` bytes, and a remainder.
// Only splits if the remainder would be large enough to be useful.
static void split_block(block_header_t *b, size_t size) {
    size_t min_split = sizeof(block_header_t) + HEAP_MIN_ALLOC;
    if (b->size < size + min_split)
        return;  // not worth splitting

    // Place the new header immediately after size bytes of data
    uint8_t *new_addr = (uint8_t *)(b + 1) + size;
    block_header_t *new_block = make_block(new_addr,
                                            b->size - size - sizeof(block_header_t),
                                            b, b->next);
    if (b->next)
        b->next->prev = new_block;

    b->next = new_block;
    b->size = size;
}

// Merge b with its next neighbour if both are free.
// Call repeatedly or chain with prev to fully coalesce.
static void coalesce(block_header_t *b) {
    // Merge with next
    if (b->next && b->next->free) {
        check_magic(b->next);
        b->size += sizeof(block_header_t) + b->next->size;
        b->next  = b->next->next;
        if (b->next)
            b->next->prev = b;
    }
    // Merge with prev
    if (b->prev && b->prev->free) {
        check_magic(b->prev);
        b->prev->size += sizeof(block_header_t) + b->size;
        b->prev->next  = b->next;
        if (b->next)
            b->next->prev = b->prev;
    }
}

// == Public API ==

void heap_init(void) {
    // Map the initial heap pages
    if (!heap_grow(PAGE_SIZE * 16)) {
        kprintf("heap: FATAL: failed to map initial pages\n");
        for (;;) __asm__ volatile ("hlt");
    }

    // Place the first (free) block covering all mapped space
    size_t initial_size = heap_end - HEAP_START - sizeof(block_header_t);
    heap_head = make_block((void *)HEAP_START, initial_size, NULL, NULL);

    kprintf("heap: initialized at 0x%016lx\n", (uint64_t)HEAP_START);
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Align size to 8 bytes — keeps all allocations naturally aligned
    size = (size + 7) & ~7ull;
    if (size < HEAP_MIN_ALLOC) size = HEAP_MIN_ALLOC;

    // First-fit scan
    block_header_t *b = heap_head;
    while (b) {
        check_magic(b);
        if (b->free && b->size >= size) {
            split_block(b, size);
            b->free = 0;
            return (void *)(b + 1);  // pointer past the header
        }
        b = b->next;
    }

    // No free block large enough — grow the heap and try again
    if (!heap_grow(size + sizeof(block_header_t)))
        return NULL;

    // Add a new free block covering the newly mapped region.
    // Find the last block first.
    block_header_t *last = heap_head;
    while (last->next) last = last->next;

    uint8_t *new_addr = (uint8_t *)(last + 1) + last->size;
    size_t   new_size = heap_end - (uint64_t)new_addr - sizeof(block_header_t);
    block_header_t *new_block = make_block(new_addr, new_size, last, NULL);
    last->next = new_block;

    // Try allocation again — should succeed now
    return kmalloc(size);
}

void *kzalloc(size_t size) {
    void *ptr = kmalloc(size);
    if (!ptr) return NULL;

    // Zero the memory — no memset yet, do it manually
    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < size; i++)
        p[i] = 0;

    return ptr;
}

void kfree(void *ptr) {
    if (!ptr) return;

    // The header lives immediately before the user pointer
    block_header_t *b = (block_header_t *)ptr - 1;
    check_magic(b);

    if (b->free) {
        kprintf("heap: WARNING: double free at %p\n", ptr);
        return;
    }

    b->free = 1;
    coalesce(b);
}

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr)       return kmalloc(new_size);
    if (!new_size)  { kfree(ptr); return NULL; }

    block_header_t *b = (block_header_t *)ptr - 1;
    check_magic(b);

    // Already big enough — return as-is
    if (b->size >= new_size) return ptr;

    // Allocate a new block, copy, free the old one
    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;

    uint8_t *src = (uint8_t *)ptr;
    uint8_t *dst = (uint8_t *)new_ptr;
    for (size_t i = 0; i < b->size; i++)
        dst[i] = src[i];

    kfree(ptr);
    return new_ptr;
}

void heap_print_stats(void) {
    size_t free_bytes = 0, used_bytes = 0, blocks = 0;
    block_header_t *b = heap_head;
    while (b) {
        check_magic(b);
        blocks++;
        if (b->free) free_bytes += b->size;
        else         used_bytes += b->size;
        b = b->next;
    }
    kprintf("heap: %lu blocks, %lu bytes used, %lu bytes free\n",
            blocks, used_bytes, free_bytes);
}