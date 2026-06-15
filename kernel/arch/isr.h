/* isr.h - interrupt service routines & IRQ dispatch */
#ifndef M1KE_ISR_H
#define M1KE_ISR_H
#include <stdint.h>

typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

typedef void (*irq_handler_t)(registers_t *);

void idt_init(void);
void irq_install_handler(int irq, irq_handler_t handler);
void irq_uninstall_handler(int irq);

#endif
