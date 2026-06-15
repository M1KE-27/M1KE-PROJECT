/* vmm.c - paging with a full identity map (4 MB pages).
 *
 * Each page-directory entry maps a 4 MB region: PDE[i] covers [i*4MB, i*4MB+4MB).
 * Mapping all 1024 entries identity-maps the entire 4 GB physical space, so
 * every address the kernel already uses (1 MB kernel, 8 MB static heap, the
 * VESA linear framebuffer at high physical memory, VGA, port-MMIO) stays valid
 * after paging is turned on. */
#include "vmm.h"

#define PDE_PRESENT  0x001
#define PDE_RW       0x002
#define PDE_PS       0x080      /* 4 MB page */
#define FOUR_MB      0x400000u

static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static bool     enabled;

static inline void load_cr3(uint32_t pd) { __asm__ __volatile__("mov %0, %%cr3" :: "r"(pd) : "memory"); }

static inline void enable_pse_paging(void) {
    uint32_t cr4, cr0;
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1u << 4);                       /* CR4.PSE: 4 MB pages */
    __asm__ __volatile__("mov %0, %%cr4" :: "r"(cr4));
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1u << 31);                      /* CR0.PG: enable paging */
    __asm__ __volatile__("mov %0, %%cr0" :: "r"(cr0) : "memory");
}

void vmm_init(void) {
    for (uint32_t i = 0; i < 1024; i++)
        page_directory[i] = (i * FOUR_MB) | PDE_PRESENT | PDE_RW | PDE_PS;

    load_cr3((uint32_t)page_directory);
    enable_pse_paging();
    enabled = true;
}

bool     vmm_enabled(void) { return enabled; }
uint32_t vmm_cr3(void)     { return (uint32_t)page_directory; }
