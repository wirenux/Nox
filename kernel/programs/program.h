#pragma once

#include <stddef.h>

typedef void (*program_entry_t)(void);

typedef struct {
    const char *name;
    const char *description;
    program_entry_t entry;
} program_t;

const program_t *program_find(const char *name);
void program_list(void);
int program_run(const char *name);
