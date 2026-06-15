/* printf.c - minimal printf supporting %c %s %d %i %u %x %X %p %% with
 * optional zero-pad and field width (e.g. %08x, %5d). */
#include "printf.h"
#include "string.h"

typedef struct {
    char  *buf;     /* if non-NULL, write here */
    size_t cap;     /* capacity of buf */
    size_t len;     /* chars produced (may exceed cap) */
    int    to_kputc;
} sink_t;

static void emit(sink_t *s, char c) {
    if (s->to_kputc) {
        kputc(c);
    } else if (s->buf && s->len + 1 < s->cap) {
        s->buf[s->len] = c;
    }
    s->len++;
}

static void emit_str(sink_t *s, const char *str) {
    while (*str) emit(s, *str++);
}

static void emit_pad(sink_t *s, const char *str, int width, int zero, int left) {
    int l = (int)strlen(str);
    if (left) {
        emit_str(s, str);
        for (int i = l; i < width; i++) emit(s, ' ');
        return;
    }
    char pad = zero ? '0' : ' ';
    /* handle leading '-' when zero padding numbers */
    if (zero && str[0] == '-') {
        emit(s, '-');
        str++;
        l--;
    }
    for (int i = l; i < width; i++) emit(s, pad);
    emit_str(s, str);
}

static void format(sink_t *s, const char *fmt, va_list ap) {
    char numbuf[35];
    for (; *fmt; fmt++) {
        if (*fmt != '%') { emit(s, *fmt); continue; }
        fmt++;
        int zero = 0, width = 0, left = 0;
        while (*fmt == '0' || *fmt == '-') {
            if (*fmt == '0') zero = 1; else left = 1;
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); fmt++; }
        switch (*fmt) {
            case 'c': emit(s, (char)va_arg(ap, int)); break;
            case 's': {
                const char *str = va_arg(ap, const char *);
                if (!str) str = "(null)";
                emit_pad(s, str, width, 0, left);
                break;
            }
            case 'd': case 'i':
                itoa(va_arg(ap, int), numbuf, 10);
                emit_pad(s, numbuf, width, zero, left);
                break;
            case 'u':
                utoa(va_arg(ap, unsigned int), numbuf, 10);
                emit_pad(s, numbuf, width, zero, left);
                break;
            case 'x':
                utoa(va_arg(ap, unsigned int), numbuf, 16);
                emit_pad(s, numbuf, width, zero, left);
                break;
            case 'X': {
                utoa(va_arg(ap, unsigned int), numbuf, 16);
                for (char *p = numbuf; *p; p++)
                    if (*p >= 'a' && *p <= 'f') *p = (char)(*p - 'a' + 'A');
                emit_pad(s, numbuf, width, zero, left);
                break;
            }
            case 'p': {
                emit_str(s, "0x");
                utoa((unsigned int)(uintptr_t)va_arg(ap, void *), numbuf, 16);
                emit_pad(s, numbuf, 8, 1, 0);
                break;
            }
            case '%': emit(s, '%'); break;
            case '\0': return;
            default: emit(s, '%'); emit(s, *fmt); break;
        }
    }
}

void kvprintf(const char *fmt, va_list ap) {
    sink_t s = { 0, 0, 0, 1 };
    format(&s, fmt, ap);
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

int kvsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    sink_t s = { buf, size, 0, 0 };
    format(&s, fmt, ap);
    if (buf && size) buf[s.len < size ? s.len : size - 1] = '\0';
    return (int)s.len;
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = kvsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}
