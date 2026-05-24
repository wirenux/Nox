; kernel/arch/gdt.asm
;
; void gdt_load(gdt_pointer_t *ptr)   [rdi = first argument, System V ABI]
;
; Loads the new GDT and reloads every segment register.
; CS cannot be changed with a plain mov — the only ways in 64-bit mode
; are a far jump or a far return.  We use the far return trick:
;   push the new CS selector, push the return address, then retfq.
; The CPU pops both and resumes execution at .reload_cs with CS updated.

bits 64
global gdt_load

GDT_KERNEL_CODE equ 0x08
GDT_KERNEL_DATA equ 0x10

gdt_load:
    ; Load the GDT register
    lgdt [rdi]

    ; ── Reload data segment registers ───────────────────────────────
    ; In 64-bit mode DS, ES, FS, GS are mostly ignored, but SS is
    ; checked by the CPU on interrupts.  Reload all of them cleanly.
    mov ax, GDT_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; ── Reload CS via far return ─────────────────────────────────────
    ; A normal 'jmp 0x08:.reload_cs' would work too, but retfq is
    ; more instructive about what's actually happening:
    ;   the CPU reads [rsp] as RIP and [rsp+8] as CS, then jumps.
    push GDT_KERNEL_CODE      ; new CS selector
    lea  rax, [rel .reload_cs]; address to return to
    push rax
    retfq                     ; far return — pops RIP then CS

.reload_cs:
    ret