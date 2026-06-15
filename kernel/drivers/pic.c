/* pic.c - remap 8259 PIC so IRQ0..15 -> int 32..47 */
#include "pic.h"
#include "io.h"

#define PIC1      0x20
#define PIC2      0xA0
#define PIC1_CMD  PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_CMD  PIC2
#define PIC2_DATA (PIC2 + 1)
#define PIC_EOI   0x20

void pic_remap(void) {
    uint8_t m1 = inb(PIC1_DATA);
    uint8_t m2 = inb(PIC2_DATA);

    outb(PIC1_CMD, 0x11); io_wait();   /* start init (cascade) */
    outb(PIC2_CMD, 0x11); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();  /* master offset 32 */
    outb(PIC2_DATA, 0x28); io_wait();  /* slave offset 40 */
    outb(PIC1_DATA, 0x04); io_wait();  /* tell master slave at IRQ2 */
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();  /* 8086 mode */
    outb(PIC2_DATA, 0x01); io_wait();

    outb(PIC1_DATA, m1);               /* restore masks */
    outb(PIC2_DATA, m2);
}

void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

void pic_set_mask(unsigned char irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb(port, inb(port) | (1 << irq));
}

void pic_clear_mask(unsigned char irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb(port, inb(port) & ~(1 << irq));
}
