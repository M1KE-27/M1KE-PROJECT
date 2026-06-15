; boot/boot.asm - Multiboot 1 header + entry point for m1keOS
; Requests a 1024x768x32 linear framebuffer from GRUB.
bits 32

MB_ALIGN    equ 1 << 0          ; align modules on page boundaries
MB_MEMINFO  equ 1 << 1          ; provide memory map
MB_VIDEO    equ 1 << 2          ; request video mode
MB_FLAGS    equ MB_ALIGN | MB_MEMINFO | MB_VIDEO
MB_MAGIC    equ 0x1BADB002
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot
align 4
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM
    dd 0, 0, 0, 0, 0            ; a.out kludge fields (unused with ELF)
    dd 0                        ; mode_type: 0 = linear graphics
    dd 1024                     ; preferred width
    dd 768                      ; preferred height
    dd 32                       ; preferred depth (bpp)

section .bss
align 16
stack_bottom:
    resb 65536                  ; 64 KB kernel stack
stack_top:

section .text
global _start
extern kmain
_start:
    mov esp, stack_top
    mov edi, eax                ; preserve multiboot magic (SSE setup clobbers eax)

    ; Enable SSE/SSE2 (GCC emits xmm instructions at -O2)
    mov eax, cr0
    and ax, 0xFFFB              ; clear EM (coprocessor emulation)
    or  ax, 0x2                 ; set MP (monitor coprocessor)
    mov cr0, eax
    mov eax, cr4
    or  eax, (1 << 9) | (1 << 10) ; OSFXSR + OSXMMEXCPT
    mov cr4, eax

    push ebx                    ; multiboot info pointer (2nd arg)
    push edi                    ; multiboot magic (1st arg)
    call kmain
.hang:
    cli
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
