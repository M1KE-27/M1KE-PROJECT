/* heap.h - kernel heap allocator */
#ifndef M1KE_HEAP_H
#define M1KE_HEAP_H
#include <stddef.h>

void   heap_init(void);
void  *kmalloc(size_t size);
void  *kcalloc(size_t n, size_t size);
void  *krealloc(void *ptr, size_t size);
void   kfree(void *ptr);

size_t heap_used(void);
size_t heap_total(void);
size_t heap_free(void);

#endif
