#include "keyboard.h"
#include "pic.h"
#include "../arch/cpu.h"
#include "../lib/printf.h"
#include "../sched/sched.h"

static thread_t *waiting_thread = NULL;

// ── Scan code → ASCII table (US QWERTY, set 1) ───────────────────────
// Index = scan code. Value = ASCII character (0 = no mapping).
// Only covers the main key area — no function keys, no numpad yet.
static const char sc_to_ascii[128] = {
    0,    0,   '1', '2', '3', '4', '5', '6',  // 0x00–0x07
    '7', '8', '9', '0', '-', '=',  '\b', '\t', // 0x08–0x0F
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',   // 0x10–0x17
    'o', 'p', '[', ']', '\n',  0,  'a', 's',   // 0x18–0x1F
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   // 0x20–0x27
    '\'', '`',  0, '\\','z', 'x', 'c', 'v',   // 0x28–0x2F
    'b', 'n', 'm', ',', '.', '/',   0,  '*',   // 0x30–0x37
     0,  ' ',   0,   0,   0,   0,   0,   0,    // 0x38–0x3F
     0,   0,   0,   0,   0,   0,   0,  '7',    // 0x40–0x47
    '8', '9', '-', '4', '5', '6', '+', '1',    // 0x48–0x4F
    '2', '3', '0', '.',  0,   0,   0,   0,     // 0x50–0x57
     0,   0,   0,   0,   0,   0,   0,   0,     // 0x58–0x5F
     0,   0,   0,   0,   0,   0,   0,   0,     // 0x60–0x67
     0,   0,   0,   0,   0,   0,   0,   0,     // 0x68–0x6F
     0,   0,   0,   0,   0,   0,   0,   0,     // 0x70–0x77
     0,   0,   0,   0,   0,   0,   0,   0,     // 0x78–0x7F
};

// Shifted versions of the keys above
static const char sc_to_ascii_shift[128] = {
    0,    0,   '!', '@', '#', '$', '%', '^',   // 0x00–0x07
    '&', '*', '(', ')', '_', '+', '\b', '\t',  // 0x08–0x0F
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',   // 0x10–0x17
    'O', 'P', '{', '}', '\n',  0,  'A', 'S',   // 0x18–0x1F
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',   // 0x20–0x27
    '"', '~',   0,  '|', 'Z', 'X', 'C', 'V',  // 0x28–0x2F
    'B', 'N', 'M', '<', '>', '?',   0,  '*',   // 0x30–0x37
     0,  ' ',  0,   0,   0,   0,   0,   0,     // 0x38–0x3F
     0,   0,   0,   0,   0,   0,   0,  '7',    // 0x40–0x47
    '8', '9', '-', '4', '5', '6', '+', '1',    // 0x48–0x4F
    '2', '3', '0', '.',  0,   0,   0,   0,     // 0x50–0x57
     0,   0,   0,   0,   0,   0,   0,   0,     // 0x58–0x5F
     0,   0,   0,   0,   0,   0,   0,   0,     // 0x60–0x67
     0,   0,   0,   0,   0,   0,   0,   0,     // 0x68–0x6F
     0,   0,   0,   0,   0,   0,   0,   0,     // 0x70–0x77
     0,   0,   0,   0,   0,   0,   0,   0,     // 0x78–0x7F
};

// Scan codes for modifier keys
#define SC_LSHIFT  0x2A
#define SC_RSHIFT  0x36
#define SC_LSHIFT_REL (SC_LSHIFT | 0x80)
#define SC_RSHIFT_REL (SC_RSHIFT | 0x80)
#define SC_CAPS    0x3A

// ── Ring buffer ───────────────────────────────────────────────────────
#define KB_BUF_SIZE 256

#define FLAG_LSHIFT (1 << 0)
#define FLAG_RSHIFT (1 << 1)

static char     kb_buf[KB_BUF_SIZE];
static uint32_t kb_read  = 0;   // next position to read from
static uint32_t kb_write = 0;   // next position to write to

static int buf_empty(void) {
    return kb_read == kb_write;
}

static int buf_full(void) {
    return ((kb_write + 1) % KB_BUF_SIZE) == kb_read;
}

static void buf_push(char c) {
    if (buf_full()) return;  // drop if full
    kb_buf[kb_write] = c;
    kb_write = (kb_write + 1) % KB_BUF_SIZE;
}

static char buf_pop(void) {
    char c = kb_buf[kb_read];
    kb_read = (kb_read + 1) % KB_BUF_SIZE;
    return c;
}

// ── Modifier state ────────────────────────────────────────────────────
static int shift_state = 0;
static int caps_lock  = 0;

// ── IRQ handler ───────────────────────────────────────────────────────

void keyboard_irq_handler(void) {
    uint8_t sc = inb(0x60);

    // 1. Handle extended scan code prefix (Ignore/skip it for standard keys)
    if (sc == 0xE0) {
        return; 
    }

    // 2. Handle Key Releases (Bit 7 is set)
    if (sc & 0x80) {
        uint8_t released = sc & 0x7F;
        if (released == SC_LSHIFT) {
            shift_state &= ~FLAG_LSHIFT;
        } else if (released == SC_RSHIFT) {
            shift_state &= ~FLAG_RSHIFT;
        }
        return;
    }

    // 3. Handle Key Presses
    if (sc == SC_LSHIFT) {
        shift_state |= FLAG_LSHIFT;
        return;
    }
    if (sc == SC_RSHIFT) {
        shift_state |= FLAG_RSHIFT;
        return;
    }
    if (sc == SC_CAPS) {
        caps_lock = !caps_lock;
        return;
    }

    // Guard against out-of-bounds array reads
    if (sc >= 128) return;

    // 4. Determine if Shift logic applies
    int use_shift = (shift_state != 0); // Active if either shift is down
    char base     = sc_to_ascii[sc];

    if (caps_lock && base >= 'a' && base <= 'z') {
        use_shift = !use_shift;
    }

    char c = use_shift ? sc_to_ascii_shift[sc] : sc_to_ascii[sc];
    if (c == 0) return;

    buf_push(c);

    // Wake the waiting thread if there is one
    if (waiting_thread) {
        sched_unblock(waiting_thread);
        waiting_thread = NULL;
    }
}

// ── Public API ────────────────────────────────────────────────────────

void keyboard_init(void) {
    pic_unmask_irq(1);
    kprintf("keyboard: PS/2 driver ready\n");
}

int keyboard_has_char(void) {
    return !buf_empty();
}

char keyboard_getchar(void) {
    while (buf_empty()) {
        // Register ourselves as the waiter then block
        // The IRQ handler will unblock us when a key arrives
        waiting_thread = sched_current();
        sched_block();
        // When we wake up, loop back and check the buffer
    }
    return buf_pop();
}