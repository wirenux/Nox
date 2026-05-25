bits 64
global ctx_switch
global thread_entry_stub
extern thread_trampoline_c

ctx_switch:
    mov  r10, rdi
    mov  r11, rsi

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r12
    push r13
    push r14
    push r15

    mov  [r10], rsp
    mov  rsp, [r11]

    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  r9
    pop  r8
    pop  rbp
    pop  rdi
    pop  rsi
    pop  rdx
    pop  rcx
    pop  rbx
    pop  rax

    ret

thread_entry_stub:
    pop  rdi
    pop  rsi
    call thread_trampoline_c
    cli
.hang:
    hlt
    jmp  .hang