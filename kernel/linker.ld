/*
 * Nox — kernel/linker.ld
 *
 * Linker script for the Nox x86-64 higher-half kernel.
 *
 * Virtual memory layout
 * ─────────────────────
 *   0x0000000000000000 – 0x00007FFFFFFFFFFF   user space  (future)
 *   0xFFFF800000000000 – ...                  HHDM        (Limine maps all physical RAM here)
 *   0xFFFFFFFF80000000 – ...                  kernel image (this file controls layout from here)
 *
 * The kernel is compiled with -mcmodel=kernel, which allows the compiler to
 * generate RIP-relative addressing for symbols in the range:
 *   [-2 GiB, +2 GiB] relative to the instruction pointer.
 * Because our VMA starts at 0xFFFFFFFF80000000 (exactly -2 GiB from the top
 * of the 64-bit address space), every kernel symbol is reachable with a
 * signed 32-bit offset — no need for a full 64-bit absolute address.
 *
 * Physical vs virtual addresses
 * ──────────────────────────────
 * Limine loads the kernel ELF at a physical address it chooses, then sets up
 * a page table that maps the VMA below onto that physical location.
 * The ELF program headers carry both:
 *   p_vaddr  = virtual address  (what the CPU uses after paging is enabled)
 *   p_paddr  = physical address (where Limine actually places the bytes)
 * AT(ADDR(...) - KERNEL_VMA_OFFSET) below sets p_paddr so that Limine can
 * find each section in physical memory.
 */

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)       /* ELF machine type expected by Limine */

ENTRY(kernel_start)            /* must match the label in arch/boot.asm */

/*
 * KERNEL_VMA_OFFSET is added to every VMA to place the image in the
 * higher half.  Keep it in sync with -mcmodel=kernel's assumption.
 */
KERNEL_VMA_OFFSET = 0xFFFFFFFF80000000;

PHDRS
{
    /*
     * Two ELF program headers (segments):
     *   text  — executable + read-only data  (r-x)
     *   data  — writable data + BSS          (rw-)
     *
     * Separating them lets the CPU enforce W^X at the page-table level
     * once you map them with the correct permissions.
     */
    text  PT_LOAD FLAGS(5);   /* PF_R | PF_X */
    data  PT_LOAD FLAGS(6);   /* PF_R | PF_W */
}

SECTIONS
{
    /*
     * Start placing sections at the kernel VMA base.
     * ". = " sets the location counter (current virtual address).
     */
    . = KERNEL_VMA_OFFSET;

    /* ── TEXT segment ─────────────────────────────────────────────────── */

    .text : AT(ADDR(.text) - KERNEL_VMA_OFFSET)
    {
        /*
         * Keep the Limine requests array at a known, stable location.
         * The boot.asm entry stub references __limine_requests_start and
         * __limine_requests_end to zero-pad any unfilled response fields.
         */
        __limine_requests_start = .;
        KEEP(*(.limine_requests))
        __limine_requests_end = .;

        *(.text .text.*)        /* all code */
    } :text

    .rodata : AT(ADDR(.rodata) - KERNEL_VMA_OFFSET)
    {
        *(.rodata .rodata.*)    /* string literals, const globals */
    } :text

    /*
     * Exception-handling tables.  The kernel doesn't use C++ exceptions,
     * but Clang still emits these.  KEEP prevents --gc-sections from
     * discarding them, which would produce linker warnings.
     */
    .eh_frame_hdr : AT(ADDR(.eh_frame_hdr) - KERNEL_VMA_OFFSET)
    {
        KEEP(*(.eh_frame_hdr))
    } :text

    .eh_frame : AT(ADDR(.eh_frame) - KERNEL_VMA_OFFSET)
    {
        KEEP(*(.eh_frame))
    } :text

    /*
     * Align to 4 KiB page boundary before the writable segment.
     * This is essential: if .rodata and .data share a page, the hardware
     * cannot enforce read-only on the former without also making the
     * latter read-only.
     */
    . = ALIGN(0x1000);

    /* ── DATA segment ─────────────────────────────────────────────────── */

    .data : AT(ADDR(.data) - KERNEL_VMA_OFFSET)
    {
        *(.data .data.*)        /* initialised globals */
    } :data

    /*
     * Store a clear symbol pair around BSS so boot.asm can zero it
     * before calling kmain.  BSS is NOT written to the ELF file (it has
     * no CONTENTS flag), so Limine does not initialise it — we must.
     */
    .bss : AT(ADDR(.bss) - KERNEL_VMA_OFFSET)
    {
        __bss_start = .;

        *(.bss .bss.*)          /* uninitialised globals */
        *(COMMON)               /* old-style C tentative definitions */

        /*
         * Align the end of BSS to 8 bytes so the zeroing loop in
         * boot.asm can use 64-bit stores safely.
         */
        . = ALIGN(8);
        __bss_end = .;
    } :data

    /* ── Discard unwanted sections ────────────────────────────────────── */

    /*
     * Drop everything the linker would otherwise include by default.
     * These sections either assume a C runtime we don't have, or are
     * leftover DWARF/notes that bloat the image without helping us.
     *
     * Do NOT discard .debug_* here — leave those in so GDB can read them.
     * strip(1) can remove them later if you want a smaller binary.
     */
    /DISCARD/ :
    {
        *(.note .note.*)
        *(.comment)
        *(.interp)              /* no dynamic linker */
    }
}
