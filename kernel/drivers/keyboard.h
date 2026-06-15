/* keyboard.h - PS/2 keyboard driver with ring buffer */
#ifndef M1KE_KEYBOARD_H
#define M1KE_KEYBOARD_H
#include <stdbool.h>

void keyboard_init(void);
bool keyboard_has_input(void);
int  keyboard_getc_nonblock(void);   /* returns ASCII or -1 */
void keyboard_set_layout(int es);    /* 0 = US, 1 = ES (Spanish) */
int  keyboard_layout(void);

/* special key codes returned via the buffer (above ASCII range) */
#define KEY_UP    0x100
#define KEY_DOWN  0x101
#define KEY_LEFT  0x102
#define KEY_RIGHT 0x103

#endif
