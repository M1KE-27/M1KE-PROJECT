/* heap.c - first-fit kernel heap with coalescing over a static region.
 * Public entry points run inside a critical section (IRQs off) so the allocator
 * stays consistent under preemptive multitasking. */
#include "heap.h"
#include "../lib/string.h"
#include "../include/io.h"
#include <stdint.h>

#define HEAP_SIZE (8 * 1024 * 1024)   /* 8 MB */
#define ALIGN 8

static uint8_t heap_area[HEAP_SIZE] __attribute__((aligned(16)));

typedef struct block {
    size_t        size;   /* payload size in bytes */
    int           free;
    struct block *next;
    struct block *prev;
} block_t;

#define HDR sizeof(block_t)

static block_t *head;
static size_t   used_bytes;

void heap_init(void) {
    head = (block_t *)heap_area;
    head->size = HEAP_SIZE - HDR;
    head->free = 1;
    head->next = 0;
    head->prev = 0;
    used_bytes = 0;
}

static size_t align_up(size_t n) { return (n + (ALIGN - 1)) & ~(size_t)(ALIGN - 1); }

static void *heap_alloc(size_t size) {
    if (size == 0) return 0;
    size = align_up(size);

    for (block_t *b = head; b; b = b->next) {
        if (b->free && b->size >= size) {
            /* split if there's room for another header + min payload */
            if (b->size >= size + HDR + ALIGN) {
                block_t *nb = (block_t *)((uint8_t *)b + HDR + size);
                nb->size = b->size - size - HDR;
                nb->free = 1;
                nb->next = b->next;
                nb->prev = b;
                if (b->next) b->next->prev = nb;
                b->next = nb;
                b->size = size;
            }
            b->free = 0;
            used_bytes += b->size + HDR;
            return (uint8_t *)b + HDR;
        }
    }
    return 0;  /* out of memory */
}

static void coalesce(block_t *b) {
    if (b->next && b->next->free) {
        b->size += HDR + b->next->size;
        b->next = b->next->next;
        if (b->next) b->next->prev = b;
    }
    if (b->prev && b->prev->free) {
        b->prev->size += HDR + b->size;
        b->prev->next = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

static void heap_dealloc(void *ptr) {
    if (!ptr) return;
    block_t *b = (block_t *)((uint8_t *)ptr - HDR);
    if (b->free) return;
    b->free = 1;
    used_bytes -= b->size + HDR;
    coalesce(b);
}

/* ---- public, locked entry points ---- */
void *kmalloc(size_t size) {
    uint32_t f = irq_save();
    void *p = heap_alloc(size);
    irq_restore(f);
    return p;
}

void kfree(void *ptr) {
    uint32_t f = irq_save();
    heap_dealloc(ptr);
    irq_restore(f);
}

void *kcalloc(size_t n, size_t size) {
    size_t total = n * size;
    void *p = kmalloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void *krealloc(void *ptr, size_t size) {
    if (!ptr) return kmalloc(size);
    if (size == 0) { kfree(ptr); return 0; }
    uint32_t f = irq_save();
    block_t *b = (block_t *)((uint8_t *)ptr - HDR);
    void *result;
    if (b->size >= size) {
        result = ptr;
    } else {
        result = heap_alloc(size);
        if (result) { memcpy(result, ptr, b->size); heap_dealloc(ptr); }
    }
    irq_restore(f);
    return result;
}

size_t heap_used(void)  { return used_bytes; }
size_t heap_total(void) { return HEAP_SIZE; }
size_t heap_free(void)  { return HEAP_SIZE - used_bytes; }
