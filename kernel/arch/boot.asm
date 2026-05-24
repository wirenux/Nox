; Entry point handed to us by Limine.
; At this point: long mode is active, paging is on, we are at our VMA.
; We have no stack yet — set one up before calling any C code.

bits 64

global kernel_start     ; must match ENTRY() in linker.ld

extern kmain            ; defined in kernel/main.c
extern __bss_start      ; defined by linker.ld
extern __bss_end        ; defined by linker.ld

section .bss
align 16
    resb 16384          ; 16 KiB initial kernel stack
stack_top:

section .text
kernel_start:
    ; Set up the stack — work downward from stack_top
    lea  rsp, [stack_top]
    xor  rbp, rbp        ; mark the bottom of the call frame chain

    ; Zero BSS — Limine does NOT do this for us.
    ; If we skip this, our global variables contain garbage.
    lea  rdi, [__bss_start]
    lea  rcx, [__bss_end]
    sub  rcx, rdi
    shr  rcx, 3          ; byte count -> qword count
    xor  eax, eax
    rep  stosq

    ; Call the C kernel entry point — should never return.
    call kmain

    ; If kmain somehow returns, halt the CPU forever.
.halt:
    cli
    hlt
    jmp  .halt
