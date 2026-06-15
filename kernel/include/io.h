/* io.h - x86 port I/O helpers */
#ifndef M1KE_IO_H
#define M1KE_IO_H
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ __volatile__("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ __volatile__("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
/* small delay by writing to unused port 0x80 */
static inline void io_wait(void) {
    __asm__ __volatile__("outb %%al, $0x80" : : "a"(0));
}
static inline void cli(void) { __asm__ __volatile__("cli"); }
static inline void sti(void) { __asm__ __volatile__("sti"); }
static inline void hlt(void) { __asm__ __volatile__("hlt"); }

/* critical sections: save EFLAGS + disable IRQs, then restore (nestable) */
static inline uint32_t irq_save(void) {
    uint32_t f;
    __asm__ __volatile__("pushf; pop %0; cli" : "=r"(f) :: "memory");
    return f;
}
static inline void irq_restore(uint32_t f) {
    __asm__ __volatile__("push %0; popf" :: "r"(f) : "memory", "cc");
}

#endif
