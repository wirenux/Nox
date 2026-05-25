#pragma once
#include <stdint.h>
#include "pmm.h"

#define PTE_PRESENT   (1ull << 0)
#define PTE_WRITABLE  (1ull << 1)
#define PTE_USER      (1ull << 2)
#define PTE_NX        (1ull << 63)

// Convenience flag combinations
#define PTE_KERNEL_RW  (PTE_PRESENT | PTE_WRITABLE)
#define PTE_KERNEL_RO  (PTE_PRESENT)
#define PTE_USER_RW    (PTE_PRESENT | PTE_WRITABLE | PTE_USER)

// Extract the physical address from a page table entry
#define PTE_GET_ADDR(entry) ((entry) & 0x000FFFFFFFFFF000ull)

// Index extraction from a virtual address
#define PML4_IDX(va)  (((va) >> 39) & 0x1FF)
#define PDPT_IDX(va)  (((va) >> 30) & 0x1FF)
#define PD_IDX(va)    (((va) >> 21) & 0x1FF)
#define PT_IDX(va)    (((va) >> 12) & 0x1FF)

// A page table is just an array of 512 uint64_t entries
typedef uint64_t page_table_t[512];

extern page_table_t *kernel_pml4;

// Create a new empty address space (allocates and zeroes a PML4)
page_table_t *vmm_new_address_space(void);

// Map one 4 KiB page: virt → phys with given flags
// Creates intermediate tables as needed using the PMM
void vmm_map_page(page_table_t *pml4, uint64_t virt,
                  uint64_t phys, uint64_t flags);

// Unmap a virtual address — clears the PT entry and flushes TLB
void vmm_unmap_page(page_table_t *pml4, uint64_t virt);

// Switch to this address space — writes PML4's physical address to CR3
void vmm_switch(page_table_t *pml4);

// Build and switch to the kernel address space.
// Call once at boot after pmm_init().
void vmm_init(struct limine_memmap_response *memmap,
              struct limine_kernel_address_response *kaddr);