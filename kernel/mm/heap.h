#pragma once
#include <stdint.h>
#include <stddef.h>

// Call once after vmm_init()
void heap_init(void);

// Allocate size bytes — returns a pointer to usable memory, or NULL
void *kmalloc(size_t size);

// Allocate size bytes, zeroed
void *kzalloc(size_t size);

// Free a pointer returned by kmalloc/kzalloc
void kfree(void *ptr);

// Resize an allocation — like realloc
void *krealloc(void *ptr, size_t new_size);

// Debug — print heap stats
void heap_print_stats(void);