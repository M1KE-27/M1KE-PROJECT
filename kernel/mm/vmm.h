/* vmm.h - virtual memory / paging.
 * Phase 1: a full 4 GB identity map using 4 MB pages (PSE). Paging is enabled
 * so the kernel, heap, framebuffer and MMIO all keep working transparently,
 * giving us the foundation for per-process address spaces later. */
#ifndef M1KE_VMM_H
#define M1KE_VMM_H
#include <stdint.h>
#include <stdbool.h>

void     vmm_init(void);          /* build identity map + enable paging */
bool     vmm_enabled(void);
uint32_t vmm_cr3(void);

#endif
