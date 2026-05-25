#include <stdio.h>

#include "../kernel/lib/printf.h"

int vprintf(const char *fmt, va_list args) {
    kvprintf(fmt, args);
    return 0;
}

int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vprintf(fmt, args);
    va_end(args);
    return ret;
}
