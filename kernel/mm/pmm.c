/* pmm.c - physical memory manager.
 * Builds a frame bitmap from the Multiboot memory map: every available RAM
 * frame starts free, then the kernel image (incl. its static heap) and the low
 * 1 MB are marked used. */
#include "pmm.h"
#include "../lib/string.h"
#include "../include/io.h"

#define MAX_FRAMES (1024 * 1024)          /* up to 4 GB of 4 KB frames */
#define BITMAP_BYTES (MAX_FRAMES / 8)     /* 128 KB */

static uint8_t  bitmap[BITMAP_BYTES];
static uint32_t total_frames;
static uint32_t used_frames;

/* end of the kernel image (incl. .bss / static heap) from the linker script */
extern uint8_t kernel_end;

static inline void bm_set(uint32_t f)   { bitmap[f >> 3] |=  (uint8_t)(1u << (f & 7)); }
static inline void bm_clear(uint32_t f) { bitmap[f >> 3] &= (uint8_t)~(1u << (f & 7)); }
static inline int  bm_test(uint32_t f)  { return (bitmap[f >> 3] >> (f & 7)) & 1; }

static void mark_used(uint32_t f) {
    if (f >= total_frames) return;
    if (!bm_test(f)) { bm_set(f); used_frames++; }
}
static void mark_free(uint32_t f) {
    if (f >= total_frames) return;
    if (bm_test(f)) { bm_clear(f); used_frames--; }
}

void pmm_init(multiboot_info_t *mbi) {
    /* find the highest available physical address */
    uint64_t max_addr = 16 * 1024 * 1024;   /* assume at least 16 MB */
    if (mbi->flags & MB_FLAG_MMAP) {
        uint32_t off = 0;
        while (off < mbi->mmap_length) {
            multiboot_mmap_entry_t *e = (multiboot_mmap_entry_t *)(uintptr_t)(mbi->mmap_addr + off);
            if (e->type == 1) {                     /* available RAM */
                uint64_t end = e->addr + e->len;
                if (end > max_addr) max_addr = end;
            }
            off += e->size + 4;
        }
    } else if (mbi->flags & MB_FLAG_MEM) {
        max_addr = (uint64_t)(mbi->mem_upper + 1024) * 1024;
    }

    total_frames = (uint32_t)(max_addr / PAGE_SIZE);
    if (total_frames > MAX_FRAMES) total_frames = MAX_FRAMES;

    /* everything used, then free the available regions */
    memset(bitmap, 0xFF, BITMAP_BYTES);
    used_frames = total_frames;

    if (mbi->flags & MB_FLAG_MMAP) {
        uint32_t off = 0;
        while (off < mbi->mmap_length) {
            multiboot_mmap_entry_t *e = (multiboot_mmap_entry_t *)(uintptr_t)(mbi->mmap_addr + off);
            if (e->type == 1) {
                uint64_t start = e->addr;
                uint64_t end   = e->addr + e->len;
                for (uint64_t a = start; a + PAGE_SIZE <= end; a += PAGE_SIZE)
                    mark_free((uint32_t)(a / PAGE_SIZE));
            }
            off += e->size + 4;
        }
    } else {
        for (uint32_t f = 256; f < total_frames; f++) mark_free(f);  /* >1 MB free */
    }

    /* reserve the low 1 MB (BIOS/VGA) and the whole kernel image + static heap */
    uint32_t kend = ((uint32_t)&kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t reserve = kend > 256 ? kend : 256;
    for (uint32_t f = 0; f < reserve; f++) mark_used(f);
}

uint32_t pmm_alloc_frame(void) {
    uint32_t flags = irq_save();
    for (uint32_t f = 0; f < total_frames; f++) {
        if (!bm_test(f)) { bm_set(f); used_frames++; irq_restore(flags); return f * PAGE_SIZE; }
    }
    irq_restore(flags);
    return 0;
}

void pmm_free_frame(uint32_t addr) {
    uint32_t flags = irq_save();
    mark_free(addr / PAGE_SIZE);
    irq_restore(flags);
}

uint64_t pmm_total_bytes(void) { return (uint64_t)total_frames * PAGE_SIZE; }
uint64_t pmm_used_bytes(void)  { return (uint64_t)used_frames * PAGE_SIZE; }
uint64_t pmm_free_bytes(void)  { return (uint64_t)(total_frames - used_frames) * PAGE_SIZE; }
uint32_t pmm_total_frames(void){ return total_frames; }
