#include "vmm.h"
#include "pmm.h"
#include "../include/limine.h"
#include "../lib/printf.h"
#include "../arch/cpu.h"
#include "../include/types.h"

page_table_t *kernel_pml4 = NULL;

// ── Helpers ───────────────────────────────────────────────────────────

// Zero a freshly allocated page table.
// We can't use memset yet — do it manually.
static void zero_table(page_table_t *t) {
    for (int i = 0; i < 512; i++)
        (*t)[i] = 0;
}

// Allocate one page from PMM and return it as a zeroed page table.
// Returns a VIRTUAL address (via HHDM) so we can dereference it.
static page_table_t *alloc_table(void) {
    uint64_t phys = pmm_alloc_page();
    if (!phys) {
        kprintf("VMM: FATAL: out of physical memory\n");
        for (;;) __asm__ volatile ("hlt");
    }
    page_table_t *virt = (page_table_t *)PHYS_TO_VIRT(phys);
    zero_table(virt);
    return virt;
}

// Given a page table entry, return a pointer to the next-level table.
// If the entry isn't present yet, allocate a new table and install it.
static page_table_t *get_or_create(page_table_t *table, int idx,
                                    uint64_t flags) {
    uint64_t entry = (*table)[idx];

    if (entry & PTE_PRESENT) {
        // Entry exists — return pointer to the existing next-level table
        return (page_table_t *)PHYS_TO_VIRT(PTE_GET_ADDR(entry));
    }

    // Entry missing — allocate a new table
    page_table_t *new_table = alloc_table();
    uint64_t new_phys = VIRT_TO_PHYS((uint64_t)new_table);

    // Install it. Intermediate entries get generous flags — the final
    // PT entry is where we enforce the real permissions.
    (*table)[idx] = new_phys | flags | PTE_PRESENT | PTE_WRITABLE;

    return new_table;
}

// ── Public API ────────────────────────────────────────────────────────

page_table_t *vmm_new_address_space(void) {
    return alloc_table();
}

void vmm_map_page(page_table_t *pml4, uint64_t virt,
                  uint64_t phys, uint64_t flags) {
    // Walk/create the 4 levels using the index bits of virt
    page_table_t *pdpt = get_or_create(pml4, PML4_IDX(virt), flags);
    page_table_t *pd   = get_or_create(pdpt, PDPT_IDX(virt), flags);
    page_table_t *pt   = get_or_create(pd,   PD_IDX(virt),   flags);

    // Install the final mapping in the PT
    (*pt)[PT_IDX(virt)] = (phys & 0x000FFFFFFFFFF000ull) | flags | PTE_PRESENT;
}

void vmm_unmap_page(page_table_t *pml4, uint64_t virt) {
    // Walk the tree — if any level is missing, nothing to unmap
    uint64_t e4 = (*pml4)[PML4_IDX(virt)];
    if (!(e4 & PTE_PRESENT)) return;

    page_table_t *pdpt = (page_table_t *)PHYS_TO_VIRT(PTE_GET_ADDR(e4));
    uint64_t e3 = (*pdpt)[PDPT_IDX(virt)];
    if (!(e3 & PTE_PRESENT)) return;

    page_table_t *pd = (page_table_t *)PHYS_TO_VIRT(PTE_GET_ADDR(e3));
    uint64_t e2 = (*pd)[PD_IDX(virt)];
    if (!(e2 & PTE_PRESENT)) return;

    page_table_t *pt = (page_table_t *)PHYS_TO_VIRT(PTE_GET_ADDR(e2));

    // Clear the entry and flush the TLB for this address
    (*pt)[PT_IDX(virt)] = 0;
    cpu_invlpg((void *)virt);
}

void vmm_switch(page_table_t *pml4) {
    uint64_t phys = VIRT_TO_PHYS((uint64_t)pml4);
    __asm__ volatile ("mov %0, %%cr3" : : "r"(phys) : "memory");
}

void vmm_init(struct limine_memmap_response *memmap,
              struct limine_kernel_address_response *kaddr) {
    // Allocate the kernel's PML4
    kernel_pml4 = vmm_new_address_space();

    uint64_t hhdm = pmm_get_hhdm();

    // Map every usable region at hhdm_base + physical_address.
    // This lets the kernel dereference any physical address by adding
    // hhdm to it — which PHYS_TO_VIRT() does automatically.
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];

        // Map all non-firmware regions — kernel needs to access them
        if (e->type != LIMINE_MEMMAP_USABLE &&
            e->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE &&
            e->type != LIMINE_MEMMAP_KERNEL_AND_MODULES &&
            e->type != LIMINE_MEMMAP_FRAMEBUFFER &&
            e->type != LIMINE_MEMMAP_ACPI_RECLAIMABLE)
            continue;

        uint64_t pages = (e->length + PAGE_SIZE - 1) / PAGE_SIZE;
        for (uint64_t p = 0; p < pages; p++) {
            uint64_t phys = e->base + p * PAGE_SIZE;
            uint64_t virt = hhdm + phys;
            vmm_map_page(kernel_pml4, virt, phys, PTE_KERNEL_RW);
        }
    }

    // Limine tells us where it loaded the kernel in physical memory
    // and what virtual address it linked it to.
    // Map every page of the kernel image at its VMA.
    uint64_t phys_base = kaddr->physical_base;
    uint64_t virt_base = kaddr->virtual_base;

    // Map 64 MiB — more than enough for any hobby kernel image.
    // A precise approach would parse the kernel ELF's program headers.
    uint64_t kernel_map_size = 64ull * 1024 * 1024;
    uint64_t pages = kernel_map_size / PAGE_SIZE;

    for (uint64_t p = 0; p < pages; p++) {
        uint64_t phys = phys_base + p * PAGE_SIZE;
        uint64_t virt = virt_base + p * PAGE_SIZE;
        vmm_map_page(kernel_pml4, virt, phys, PTE_KERNEL_RW);
    }

    // From this point the CPU uses our PML4.
    // If any mapping is wrong, we triple fault here.
    vmm_switch(kernel_pml4);

    kprintf("VMM: kernel address space active\n");
}