/* string.h - freestanding string/memory utilities */
#ifndef M1KE_STRING_H
#define M1KE_STRING_H
#include <stddef.h>
#include <stdint.h>

void  *memset(void *dst, int c, size_t n);
void  *memcpy(void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
int    memcmp(const void *a, const void *b, size_t n);

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *dst, const char *src);
char  *strncpy(char *dst, const char *src, size_t n);
char  *strcat(char *dst, const char *src);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *haystack, const char *needle);

int    atoi(const char *s);
/* itoa into buf, base 2..16, returns buf */
char  *itoa(int value, char *buf, int base);
char  *utoa(unsigned int value, char *buf, int base);

#endif
