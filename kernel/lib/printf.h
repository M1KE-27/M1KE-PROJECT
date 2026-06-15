/* printf.h - minimal formatted output */
#ifndef M1KE_PRINTF_H
#define M1KE_PRINTF_H
#include <stdarg.h>
#include <stddef.h>

/* Provided by console layer: write one char to screen + serial */
void kputc(char c);

void kprintf(const char *fmt, ...);
void kvprintf(const char *fmt, va_list ap);

/* format into a buffer (returns length written, always NUL-terminated) */
int  ksnprintf(char *buf, size_t size, const char *fmt, ...);
int  kvsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

#endif
