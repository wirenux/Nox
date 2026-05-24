#pragma once
#include <stdint.h>
#include "../include/limine.h"

#define PAGE_SIZE 4096

// Convert a physical address to a virtual one using the HHDM.
// The PMM works with physical addresses externally, but needs
// virtual addresses internally to read/write the bitmap.
#define PHYS_TO_VIRT(addr) ((void *)((uint64_t)(addr) + pmm_get_hhdm()))
#define VIRT_TO_PHYS(addr) ((uint64_t)(addr) - pmm_get_hhdm())

// Called once at boot — parses the Limine memory map and builds the bitmap
void pmm_init(struct limine_memmap_response *memmap, uint64_t hhdm_offset);

// Allocate one free 4 KiB page frame.
// Returns the PHYSICAL address of the frame, or 0 on failure.
uint64_t pmm_alloc_page(void);

// Free a previously allocated page frame.
// addr must be a physical address returned by pmm_alloc_page().
void pmm_free_page(uint64_t addr);

// Return the HHDM offset (needed by the PHYS_TO_VIRT macro)
uint64_t pmm_get_hhdm(void);

// Debug — print memory stats to screen
void pmm_print_stats(void);