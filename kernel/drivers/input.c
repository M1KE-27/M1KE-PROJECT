/* input.c - merges PS/2 keyboard and serial input into one stream.
 * Parses ANSI arrow-key escape sequences arriving over serial. */
#include "input.h"
#include "keyboard.h"
#include "serial.h"

/* serial ANSI escape state: 0 idle, 1 saw ESC, 2 saw ESC[ */
static int esc_state;

static int translate_serial(int c) {
    if (esc_state == 1) {
        esc_state = (c == '[') ? 2 : 0;
        return -1;
    }
    if (esc_state == 2) {
        esc_state = 0;
        switch (c) {
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'C': return KEY_RIGHT;
            case 'D': return KEY_LEFT;
            default:  return -1;
        }
    }
    if (c == 0x1B) { esc_state = 1; return -1; }
    if (c == '\r') return '\n';
    if (c == 0x7F) return '\b';   /* DEL -> backspace */
    return c;
}

int input_poll(void) {
    int c = keyboard_getc_nonblock();
    if (c != -1) return c;

    c = serial_getc_nonblock();
    if (c != -1) return translate_serial(c);

    return -1;
}

int input_getchar(void) {
    for (;;) {
        int c = input_poll();
        if (c != -1) return c;
        __asm__ __volatile__("hlt");
    }
}
