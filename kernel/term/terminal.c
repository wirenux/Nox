#include "terminal.h"
#include "../drivers/keyboard.h"
#include "../drivers/serial.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include "../mm/heap.h"
#include "../mm/pmm.h"
#include "../sched/sched.h"

#define INPUT_MAX 256

// ── Built-in commands ─────────────────────────────────────────────────

static void cmd_help(void) {
    kprintf("Available commands:\n");
    kprintf("  help      — show this message\n");
    kprintf("  clear     — clear the screen\n");
    kprintf("  mem       — show memory stats\n");
    kprintf("  uptime    — show tick count\n");
    kprintf("  echo ...  — print arguments\n");
    kprintf("  halt      — halt the kernel\n");
}

static void cmd_clear(void) {
    // Re-use the framebuffer clear — reset cursor to top
    // We call kprintf_clear() which we need to add to printf.c
    extern void kprintf_clear(void);
    kprintf_clear();
}

static void cmd_mem(void) {
    pmm_print_stats();
    heap_print_stats();
}

static void cmd_uptime(void) {
    extern uint64_t pit_get_ticks(void);
    uint64_t ticks   = pit_get_ticks();
    uint64_t seconds = ticks / 100;
    uint64_t minutes = seconds / 60;
    uint64_t hours   = minutes / 60;
    kprintf("uptime: %lu:%02lu:%02lu (%lu ticks)\n",
            hours, minutes % 60, seconds % 60, ticks);
}

static void cmd_echo(const char *args) {
    if (!args || *args == '\0') {
        kprintf("\n");
        return;
    }
    kprintf("%s\n", args);
}

static void cmd_halt(void) {
    kprintf("Halting Nox. Goodbye.\n");
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}

// ── Command dispatcher ────────────────────────────────────────────────

// Split "cmd args" into command and argument pointer
static void dispatch(char *line) {
    // Skip leading spaces
    while (*line == ' ') line++;
    if (*line == '\0') return;

    // Find first space to split command from args
    char *args = line;
    while (*args && *args != ' ') args++;
    if (*args == ' ') {
        *args = '\0';   // null-terminate command
        args++;         // args points to remainder
        while (*args == ' ') args++;  // skip spaces
    } else {
        args = NULL;    // no arguments
    }

    // Match command
    if (strcmp(line, "help")   == 0) { cmd_help();         return; }
    if (strcmp(line, "clear")  == 0) { cmd_clear();        return; }
    if (strcmp(line, "mem")    == 0) { cmd_mem();          return; }
    if (strcmp(line, "uptime") == 0) { cmd_uptime();       return; }
    if (strcmp(line, "echo")   == 0) { cmd_echo(args);     return; }
    if (strcmp(line, "halt")   == 0) { cmd_halt();         return; }

    kprintf("unknown command: %s\n", line);
    kprintf("type 'help' for a list of commands\n");
}

// ── Prompt ────────────────────────────────────────────────────────────

static void print_prompt(void) {
    kprintf("nox> ");
}

// ── Main terminal loop ────────────────────────────────────────────────

void terminal_run(void *arg) {
    (void)arg;

    kprintf("\n");
    kprintf("Nox Terminal ready. Type 'help' for commands.\n");
    kprintf("\n");

    char line[INPUT_MAX];
    int  pos = 0;

    print_prompt();

    for (;;) {
        char c = keyboard_getchar();

        if (c == '\n') {
            // Submit line
            kprintf("\n");
            line[pos] = '\0';
            if (pos > 0)
                dispatch(line);
            pos = 0;
            print_prompt();

        } else if (c == '\b') {
            // Backspace — erase last character
            if (pos > 0) {
                pos--;
                // Overwrite the character on screen with a space
                kprintf("\b \b");
            }

        } else if (c >= 32 && c < 127) {
            // Printable character
            if (pos < INPUT_MAX - 1) {
                line[pos++] = c;
                // Echo to screen
                char echo[2] = { c, '\0' };
                kprintf("%s", echo);
            }
        }
    }
}