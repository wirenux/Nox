#include "pmm.h"
#include "../include/limine.h"
#include "../include/types.h"
#include "../lib/printf.h"

// ── Bitmap storage ────────────────────────────────────────────────────
//
// We store the bitmap inside the first usable physical memory region
// large enough to hold it.  No heap exists yet, so we can't kmalloc.
// The bitmap pointer is a virtual address (physical + hhdm_offset).

static uint8_t  *bitmap      = NULL;  // pointer to bitmap in memory
static uint64_t  bitmap_size = 0;     // size in bytes
static uint64_t  total_frames = 0;    // total frames tracked
static uint64_t  free_frames  = 0;    // frames currently free
static uint64_t  hhdm_base    = 0;    // stored HHDM offset

// ── Bitmap helpers ───────────────────────────────────────────────────

static inline void bitmap_set(uint64_t frame) {
    bitmap[frame / 8] |= (1 << (frame % 8));
}

static inline void bitmap_clear(uint64_t frame) {
    bitmap[frame / 8] &= ~(1 << (frame % 8));
}

static inline int bitmap_test(uint64_t frame) {
    return (bitmap[frame / 8] >> (frame % 8)) & 1;
}

// ── Public API ────────────────────────────────────────────────────────

uint64_t pmm_get_hhdm(void) {
    return hhdm_base;
}

void pmm_init(struct limine_memmap_response *memmap, uint64_t hhdm_offset) {
    hhdm_base = hhdm_offset;

    // ── Pass 1: find the highest physical address ─────────────────
    // We need this to know how large the bitmap must be.
    uint64_t highest_addr = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        uint64_t end = e->base + e->length;
        if (end > highest_addr)
            highest_addr = end;
    }

    // One bit per 4 KiB frame, rounded up to the nearest byte
    total_frames = highest_addr / PAGE_SIZE;
    bitmap_size  = (total_frames + 7) / 8;

    // ── Pass 2: find a usable region to store the bitmap ─────────
    // We need bitmap_size bytes of usable RAM. Pick the first region
    // that fits. We'll place the bitmap at the start of that region.
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->length >= bitmap_size) {
            // Store using virtual address so we can dereference it
            bitmap = (uint8_t *)(e->base + hhdm_offset);
            break;
        }
    }

    if (!bitmap) {
        // If this fires you have less than bitmap_size bytes of RAM — unlikely
        kprintf("PMM: FATAL: no region large enough for bitmap\n");
        for (;;) __asm__ volatile ("hlt");
    }

    // ── Pass 3: mark everything as used ──────────────────────────
    // Start pessimistic — assume nothing is free.
    // We use a simple loop rather than memset since lib/string isn't
    // wired up yet.  Initialise each byte of the bitmap to 0xFF (all used).
    for (uint64_t i = 0; i < bitmap_size; i++)
        bitmap[i] = 0xFF;

    free_frames = 0;

    // ── Pass 4: mark USABLE regions as free ──────────────────────
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE)
            continue;

        uint64_t frame_start = e->base / PAGE_SIZE;
        uint64_t frame_count = e->length / PAGE_SIZE;

        for (uint64_t f = frame_start; f < frame_start + frame_count; f++) {
            bitmap_clear(f);
            free_frames++;
        }
    }

    // ── Pass 5: re-mark the bitmap itself as used ─────────────────
    // The bitmap lives inside a usable region — we just freed it above.
    // Re-mark those frames so we don't hand them out as allocations.
    uint64_t bitmap_phys  = (uint64_t)bitmap - hhdm_offset;
    uint64_t bitmap_frames = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t bitmap_start  = bitmap_phys / PAGE_SIZE;

    for (uint64_t f = bitmap_start; f < bitmap_start + bitmap_frames; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            free_frames--;
        }
    }
}

uint64_t pmm_alloc_page(void) {
    // Scan the bitmap for the first free (0) bit
    for (uint64_t i = 0; i < bitmap_size; i++) {
        if (bitmap[i] == 0xFF)
            continue;  // all 8 frames in this byte are used — skip fast

        // At least one free frame in this byte — find which bit
        for (int bit = 0; bit < 8; bit++) {
            uint64_t frame = i * 8 + bit;
            if (frame >= total_frames)
                return 0;  // ran off the end

            if (!bitmap_test(frame)) {
                bitmap_set(frame);
                free_frames--;
                return frame * PAGE_SIZE;  // return physical address
            }
        }
    }

    return 0;  // out of memory
}

void pmm_free_page(uint64_t addr) {
    if (addr == 0) return;  // never free the null frame

    uint64_t frame = addr / PAGE_SIZE;
    if (frame >= total_frames) return;  // out of range

    if (bitmap_test(frame)) {
        // Frame was used — clear it
        bitmap_clear(frame);
        free_frames++;
    }
    // Freeing an already-free frame is a double-free bug.
    // A real kernel would panic here. For now, silently ignore.
}

void pmm_print_stats(void) {
    uint64_t used = total_frames - free_frames;
    kprintf("PMM: %lu MiB total, %lu MiB free, %lu MiB used\n",
            (total_frames * PAGE_SIZE) / (1024 * 1024),
            (free_frames  * PAGE_SIZE) / (1024 * 1024),
            (used         * PAGE_SIZE) / (1024 * 1024));
}