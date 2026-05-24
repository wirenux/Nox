#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define NULL  ((void*)0)

#define ALIGN_UP(n, a)   (((n) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(n, a) ((n) & ~((a) - 1))