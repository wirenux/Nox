; Generates 256 ISR stubs using a NASM macro.
; Each stub normalises the stack to a uniform cpu_frame_t layout,
; calls isr_dispatch(cpu_frame_t *frame), then restores and returns.
;
; Why stubs at all?
;   - Only some exceptions push an error code.  Stubs push a dummy 0
;     for the rest so the C struct layout is always identical.
;   - The CPU doesn't save general-purpose registers — we must.
;   - NASM macros let us generate all 256 with minimal repetition.

bits 64
extern isr_dispatch

; ── Macros ────────────────────────────────────────────────────────────

; Exceptions that push an error code: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30
; All others need a dummy pushed first.

%macro ISR_NOERR 1
isr_stub_%1:
    push 0              ; fake error code — keeps frame layout uniform
    push %1             ; vector number
    jmp  isr_common
%endmacro

%macro ISR_ERR 1
isr_stub_%1:
    ; CPU already pushed the real error code
    push %1             ; vector number
    jmp  isr_common
%endmacro

; ── Common handler ────────────────────────────────────────────────────

isr_common:
    ; Save all general-purpose registers in the order cpu_frame_t expects
    ; (reversed, because push decrements rsp first)
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; rsp now points at the base of a complete cpu_frame_t.
    ; Pass it as the first argument (System V ABI: rdi = arg1).
    mov  rdi, rsp
    call isr_dispatch

    ; Restore general-purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove vector + error_code from stack, then return
    add  rsp, 16
    iretq

; ── Stub definitions ──────────────────────────────────────────────────

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8    ; Double Fault
ISR_NOERR 9
ISR_ERR   10   ; Invalid TSS
ISR_ERR   11   ; Segment Not Present
ISR_ERR   12   ; Stack-Segment Fault
ISR_ERR   13   ; General Protection Fault
ISR_ERR   14   ; Page Fault
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17   ; Alignment Check
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21   ; Control Protection
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29   ; VMM Communication
ISR_ERR   30   ; Security Exception
ISR_NOERR 31

; IRQ stubs 32–47 (hardware, after PIC remapping)
%assign i 32
%rep 224
ISR_NOERR i
%assign i i+1
%endrep

; ── Stub pointer table ────────────────────────────────────────────────
; idt.c references this array to populate the IDT entries.
; One 8-byte pointer per vector, in order.

global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
    dq isr_stub_%+i
%assign i i+1
%endrep