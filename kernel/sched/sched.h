#pragma once
#include <stdint.h>
#include <stddef.h>

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_DEAD
} thread_state_t;

typedef struct thread {
    uint64_t        saved_rsp;   // only register stored in struct
    uint8_t        *stack;       // base of stack allocation
    size_t          stack_size;
    thread_state_t  state;
    uint64_t        id;
    struct thread  *next;        // run queue linked list
} thread_t;

// Call once at boot — sets up the idle thread from the current stack
void sched_init(void);

// Create a new thread that will call func(arg) when scheduled
thread_t *thread_create(void (*func)(void *), void *arg);

// Called from PIT IRQ handler every tick — may switch threads
void sched_tick(void);

// Yield the CPU voluntarily — switch to next ready thread immediately
void sched_yield(void);

// Block the current thread — won't be scheduled until sched_unblock()
void sched_block(void);

// Unblock a thread — puts it back in the run queue
void sched_unblock(thread_t *t);

// Sleep for approximately ms milliseconds
void sched_sleep(uint64_t ms);

// Return the currently running thread
thread_t *sched_current(void);

void thread_trampoline_c(void (*func)(void *), void *arg);

extern void thread_entry_stub(void);