/* string.c - freestanding string/memory utilities */
#include "string.h"

void *memset(void *dst, int c, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    while (n--) *d++ = (uint8_t)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
    while (n--) {
        if (*x != *y) return (int)*x - (int)*y;
        x++; y++;
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

char *strcpy(char *dst, const char *src) {
    char *r = dst;
    while ((*dst++ = *src++)) {}
    return r;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *r = dst;
    while (n && (*dst = *src)) { dst++; src++; n--; }
    while (n--) *dst++ = '\0';
    return r;
}

char *strcat(char *dst, const char *src) {
    char *r = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++)) {}
    return r;
}

char *strchr(const char *s, int c) {
    while (*s) { if (*s == (char)c) return (char *)s; s++; }
    return (c == 0) ? (char *)s : 0;
}

char *strrchr(const char *s, int c) {
    const char *last = 0;
    do { if (*s == (char)c) last = s; } while (*s++);
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char *)haystack;
    }
    return 0;
}

int atoi(const char *s) {
    int sign = 1, val = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); s++; }
    return sign * val;
}

static const char digits[] = "0123456789abcdef";

char *utoa(unsigned int value, char *buf, int base) {
    char tmp[33];
    int i = 0;
    if (base < 2 || base > 16) { buf[0] = 0; return buf; }
    if (value == 0) tmp[i++] = '0';
    while (value) { tmp[i++] = digits[value % base]; value /= base; }
    int j = 0;
    while (i) buf[j++] = tmp[--i];
    buf[j] = 0;
    return buf;
}

char *itoa(int value, char *buf, int base) {
    if (base == 10 && value < 0) {
        buf[0] = '-';
        utoa((unsigned int)(-value), buf + 1, base);
        return buf;
    }
    return utoa((unsigned int)value, buf, base);
}
