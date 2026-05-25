#pragma once
#include <stdint.h>
#include <stddef.h>

// Start the terminal — call this from a dedicated thread
void terminal_run(void *arg);