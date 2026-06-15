/* multiboot.h - Multiboot 1 information structures (subset we use) */
#ifndef M1KE_MULTIBOOT_H
#define M1KE_MULTIBOOT_H
#include <stdint.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

#define MB_FLAG_MEM       (1u << 0)
#define MB_FLAG_MMAP      (1u << 6)
#define MB_FLAG_FRAMEBUF  (1u << 12)

typedef struct {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;       /* 1 = available */
} __attribute__((packed)) multiboot_mmap_entry_t;

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;   /* 0=indexed 1=rgb 2=ega text */
    uint8_t  color_info[6];
} __attribute__((packed)) multiboot_info_t;

#endif
