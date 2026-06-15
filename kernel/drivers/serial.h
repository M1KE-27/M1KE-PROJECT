/* serial.h - COM1 serial port driver (used as headless console) */
#ifndef M1KE_SERIAL_H
#define M1KE_SERIAL_H
#include <stdbool.h>

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);
bool serial_has_input(void);
int  serial_getc_nonblock(void);   /* -1 if none */

#endif
