#include "string.h"

// ── Memory functions ──────────────────────────────────────────────────

void *memcpy(void *dst, const void *src, size_t n) {
    // Copy 8 bytes at a time for the bulk of the data,
    // then handle the remaining 0–7 bytes one at a time.
    // This is 4–8x faster than a naive byte loop for large copies
    // like framebuffer scrolling.
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    // Fast path: 64-bit copies while at least 8 bytes remain
    size_t qwords = n / 8;
    for (size_t i = 0; i < qwords; i++) {
        *(uint64_t *)d = *(const uint64_t *)s;
        d += 8;
        s += 8;
    }

    // Tail: remaining bytes
    size_t tail = n % 8;
    for (size_t i = 0; i < tail; i++)
        *d++ = *s++;

    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (d == s || n == 0)
        return dst;

    if (d < s) {
        // dst is before src — safe to copy forward (same as memcpy)
        size_t qwords = n / 8;
        for (size_t i = 0; i < qwords; i++) {
            *(uint64_t *)d = *(const uint64_t *)s;
            d += 8; s += 8;
        }
        size_t tail = n % 8;
        for (size_t i = 0; i < tail; i++)
            *d++ = *s++;
    } else {
        // dst is after src — regions overlap, copy backward to avoid
        // corrupting src before we read it
        d += n;
        s += n;
        size_t qwords = n / 8;
        for (size_t i = 0; i < qwords; i++) {
            d -= 8; s -= 8;
            *(uint64_t *)d = *(const uint64_t *)s;
        }
        size_t tail = n % 8;
        for (size_t i = 0; i < tail; i++)
            *--d = *--s;
    }

    return dst;
}

void *memset(void *dst, int val, size_t n) {
    uint8_t  v = (uint8_t)val;
    uint8_t *d = (uint8_t *)dst;

    // Build a 64-bit word filled with the byte value so we can
    // zero (or fill) 8 bytes per iteration
    uint64_t wide = (uint64_t)v;
    wide |= wide << 8;
    wide |= wide << 16;
    wide |= wide << 32;

    size_t qwords = n / 8;
    for (size_t i = 0; i < qwords; i++) {
        *(uint64_t *)d = wide;
        d += 8;
    }
    size_t tail = n % 8;
    for (size_t i = 0; i < tail; i++)
        *d++ = v;

    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *p = (const uint8_t *)a;
    const uint8_t *q = (const uint8_t *)b;
    for (size_t i = 0; i < n; i++) {
        if (p[i] != q[i])
            return (int)p[i] - (int)q[i];
    }
    return 0;
}

// ── String functions ──────────────────────────────────────────────────

size_t strlen(const char *s) {
    size_t n = 0;
    while (*s++) n++;
    return n;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (n && (*d++ = *src++)) n--;
    // Pad remaining bytes with null if src was shorter than n
    while (n--) *d++ = '\0';
    return dst;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : NULL;
}