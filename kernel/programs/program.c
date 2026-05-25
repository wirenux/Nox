#include "program.h"

#include "../lib/printf.h"
#include "../lib/string.h"

extern void nox_app_hi_main(void);

static const program_t programs[] = {
    { "hi", "classic stdio hello-world program", nox_app_hi_main },
};

static const size_t program_count = sizeof(programs) / sizeof(programs[0]);

const program_t *program_find(const char *name) {
    for (size_t i = 0; i < program_count; i++) {
        if (strcmp(programs[i].name, name) == 0)
            return &programs[i];
    }

    return NULL;
}

void program_list(void) {
    kprintf("Programs:\n");
    for (size_t i = 0; i < program_count; i++)
        kprintf("  %s      - %s\n", programs[i].name, programs[i].description);
}

int program_run(const char *name) {
    const program_t *program = program_find(name);
    if (!program) {
        kprintf("program not found: %s\n", name);
        return -1;
    }

    program->entry();
    return 0;
}
