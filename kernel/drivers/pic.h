/* pic.h - 8259 Programmable Interrupt Controller */
#ifndef M1KE_PIC_H
#define M1KE_PIC_H
void pic_remap(void);
void pic_send_eoi(unsigned char irq);
void pic_set_mask(unsigned char irq);
void pic_clear_mask(unsigned char irq);
#endif
