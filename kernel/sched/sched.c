#include "sched.h"
#include "../mm/heap.h"
#include "../mm/vmm.h"
#include "../lib/printf.h"
#include "../arch/cpu.h"
#include "../drivers/pit.h"

#define THREAD_STACK_SIZE (16 * 1024)  // 16 KiB per thread
#define QUANTUM           10           // ticks before forced switch (100ms)

// Defined in sched.asm
extern void ctx_switch(uint64_t *save_rsp, uint64_t *load_rsp);

// ── State ─────────────────────────────────────────────────────────────
static thread_t *current  = NULL;  // currently running thread
static thread_t *run_head = NULL;  // head of the circular run queue
static uint64_t  next_id  = 0;
static uint64_t  ticks_on_cpu = 0; // how many ticks current thread has run

// ── Run queue (circular linked list) ─────────────────────────────────

static void queue_append(thread_t *t) {
    if (!run_head) {
        run_head = t;
        t->next  = t;  // points to itself — only element
        return;
    }
    // Walk to the last node (the one pointing back to head)
    thread_t *tail = run_head;
    while (tail->next != run_head)
        tail = tail->next;
    tail->next = t;
    t->next    = run_head;
}

static void queue_remove(thread_t *t) {
    if (!run_head) return;
    if (run_head == t && t->next == t) {
        // Only element
        run_head = NULL;
        return;
    }
    thread_t *prev = run_head;
    while (prev->next != t)
        prev = prev->next;
    prev->next = t->next;
    if (run_head == t)
        run_head = t->next;
    t->next = NULL;
}

// ── Thread creation ───────────────────────────────────────────────────

// This is the trampoline every thread starts in.
// It calls the thread's function, then cleans up when it returns.
void thread_trampoline_c(void (*func)(void *), void *arg) {
    // Re-enable interrupts — they were disabled during the first switch
    cpu_sti();
    func(arg);

    // Thread returned — mark dead and yield
    current->state = THREAD_DEAD;
    queue_remove(current);
    sched_yield();

    // Should never reach here
    for (;;) cpu_hlt();
}

thread_t *thread_create(void (*func)(void *), void *arg) {
    thread_t *t = (thread_t *)kmalloc(sizeof(thread_t));
    if (!t) return NULL;

    t->stack = (uint8_t *)kmalloc(THREAD_STACK_SIZE);
    if (!t->stack) { kfree(t); return NULL; }

    t->stack_size = THREAD_STACK_SIZE;
    t->state      = THREAD_READY;
    t->id         = next_id++;
    t->next       = NULL;

    // ── Build the fake initial stack frame ───────────────────────────
    // We set up the stack so that when ctx_switch restores it,
    // execution begins at thread_trampoline(func, arg).
    //
    // ctx_switch in sched.asm will pop r15..rax then ret.
    // 'ret' pops the return address — we put thread_trampoline there.
    // The two values after that are func and arg — trampoline reads
    // them as its own arguments from the stack (see sched.asm).

    uint64_t *sp = (uint64_t *)(t->stack + THREAD_STACK_SIZE);

    // Pushed deepest first (highest address) — popped last
    *--sp = (uint64_t)arg;               // for thread_entry_stub: pop rsi
    *--sp = (uint64_t)func;              // for thread_entry_stub: pop rdi
    *--sp = (uint64_t)thread_entry_stub; // ret address

    // Register saves — last push = lowest address = first popped by ctx_switch
    *--sp = 0;   // rax  (popped last  by ctx_switch)
    *--sp = 0;   // rbx
    *--sp = 0;   // rcx
    *--sp = 0;   // rdx
    *--sp = 0;   // rsi
    *--sp = 0;   // rdi
    *--sp = 0;   // rbp
    *--sp = 0;   // r8
    *--sp = 0;   // r9
    *--sp = 0;   // r12
    *--sp = 0;   // r13
    *--sp = 0;   // r14
    *--sp = 0;   // r15  (popped first by ctx_switch)

    t->saved_rsp = (uint64_t)sp;

    queue_append(t);
    return t;
}

// ── Scheduler ─────────────────────────────────────────────────────────

void sched_init(void) {
    // Create a thread_t to represent the current execution context
    // (the boot stack — kmain itself becomes the idle thread)
    thread_t *idle = (thread_t *)kmalloc(sizeof(thread_t));
    idle->stack      = NULL;  // we don't own the boot stack
    idle->stack_size = 0;
    idle->state      = THREAD_RUNNING;
    idle->id         = next_id++;
    idle->next       = NULL;

    current  = idle;
    queue_append(idle);

    kprintf("sched: initialized (idle thread id=%lu)\n", idle->id);
}

// Pick the next READY thread after current in the circular queue
static thread_t *pick_next(void) {
    if (!run_head) return NULL;

    thread_t *t = current->next;
    // Walk the queue until we find a READY thread
    thread_t *start = t;
    do {
        if (t->state == THREAD_READY)
            return t;
        t = t->next;
    } while (t != start);

    return NULL;  // nothing ready — stay on current
}

static void do_switch(thread_t *next) {
    thread_t *prev = current;
    current       = next;
    next->state   = THREAD_RUNNING;
    ticks_on_cpu  = 0;

    // ctx_switch saves prev->saved_rsp, loads next->saved_rsp
    ctx_switch(&prev->saved_rsp, &next->saved_rsp);
    // We return here when someone switches back to prev
}

void sched_tick(void) {
    ticks_on_cpu++;
    if (ticks_on_cpu < QUANTUM) return;

    thread_t *next = pick_next();
    if (!next || next == current) return;

    if (current->state == THREAD_RUNNING)
        current->state = THREAD_READY;

    do_switch(next);
}

void sched_yield(void) {
    cpu_cli();
    thread_t *next = pick_next();
    if (next && next != current) {
        if (current->state == THREAD_RUNNING)
            current->state = THREAD_READY;
        do_switch(next);
    }
    cpu_sti();
}

void sched_block(void) {
    cpu_cli();
    current->state = THREAD_BLOCKED;
    thread_t *next = pick_next();
    if (next) do_switch(next);
    cpu_sti();
}

void sched_unblock(thread_t *t) {
    cpu_cli();
    t->state = THREAD_READY;
    cpu_sti();
}

void sched_sleep(uint64_t ms) {
    uint64_t wake = pit_get_ticks() + (ms * 100 / 1000);
    while (pit_get_ticks() < wake)
        sched_yield();
}

thread_t *sched_current(void) {
    return current;
}