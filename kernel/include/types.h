#pragma once

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long      size_t;
typedef int                bool;

#define true  1
#define false 0
#define NULL  ((void*)0)

#define ALIGN_UP(n, a)   (((n) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(n, a) ((n) & ~((a) - 1))
