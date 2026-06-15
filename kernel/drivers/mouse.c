/* mouse.c - PS/2 mouse on IRQ12, standard 3-byte packets */
#include "mouse.h"
#include "io.h"
#include "../arch/isr.h"
#include "../drivers/pic.h"

static int mx, my, sw, sh;
static bool btn_left;
static uint8_t cycle, packet[3];

static void wait_input(void)  { for (int i = 0; i < 100000; i++) if (!(inb(0x64) & 2)) return; }
static void wait_output(void) { for (int i = 0; i < 100000; i++) if (inb(0x64) & 1) return; }

static void mouse_write(uint8_t val) {
    wait_input(); outb(0x64, 0xD4);
    wait_input(); outb(0x60, val);
}
static uint8_t mouse_read(void) {
    wait_output();
    return inb(0x60);
}

static void mouse_callback(registers_t *r) {
    (void)r;
    uint8_t status = inb(0x64);
    if (!(status & 1) || !(status & 0x20)) return;  /* data is from mouse only */
    uint8_t data = inb(0x60);

    switch (cycle) {
        case 0:
            packet[0] = data;
            if (!(data & 0x08)) return;   /* sync: bit3 must be set */
            cycle = 1;
            break;
        case 1:
            packet[1] = data; cycle = 2;
            break;
        case 2: {
            packet[2] = data;
            cycle = 0;
            int dx = packet[1];
            int dy = packet[2];
            if (packet[0] & 0x10) dx |= 0xFFFFFF00;   /* sign extend */
            if (packet[0] & 0x20) dy |= 0xFFFFFF00;
            if (packet[0] & 0xC0) { dx = 0; dy = 0; }  /* overflow */
            btn_left = (packet[0] & 1) != 0;
            mx += dx;
            my -= dy;                                   /* screen y is inverted */
            if (mx < 0) mx = 0;
            if (mx >= sw) mx = sw - 1;
            if (my < 0) my = 0;
            if (my >= sh) my = sh - 1;
            break;
        }
    }
}

void mouse_init(int screen_w, int screen_h) {
    sw = screen_w; sh = screen_h;
    mx = sw / 2; my = sh / 2;
    cycle = 0; btn_left = false;

    wait_input(); outb(0x64, 0xA8);          /* enable auxiliary device */

    wait_input(); outb(0x64, 0x20);          /* read compaq status byte */
    uint8_t status = mouse_read();
    status |= 0x02;                           /* enable IRQ12 */
    status &= ~0x20;                          /* enable mouse clock */
    wait_input(); outb(0x64, 0x60);
    wait_input(); outb(0x60, status);

    mouse_write(0xF6); mouse_read();          /* set defaults (ACK) */
    mouse_write(0xF4); mouse_read();          /* enable packet streaming (ACK) */

    irq_install_handler(12, mouse_callback);
    pic_clear_mask(2);                         /* cascade */
    pic_clear_mask(12);                        /* mouse */
}

int  mouse_x(void)    { return mx; }
int  mouse_y(void)    { return my; }
bool mouse_left(void) { return btn_left; }
