/* pmm.h - physical memory manager (4 KB frame bitmap allocator) */
#ifndef M1KE_PMM_H
#define M1KE_PMM_H
#include <stdint.h>
#include "../include/multiboot.h"

#define PAGE_SIZE 4096

void     pmm_init(multiboot_info_t *mbi);
uint32_t pmm_alloc_frame(void);              /* physical address, 0 on failure */
void     pmm_free_frame(uint32_t addr);

uint64_t pmm_total_bytes(void);
uint64_t pmm_free_bytes(void);
uint64_t pmm_used_bytes(void);
uint32_t pmm_total_frames(void);

#endif
