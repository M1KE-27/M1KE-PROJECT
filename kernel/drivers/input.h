/* input.h - unified input from PS/2 keyboard and serial console */
#ifndef M1KE_INPUT_H
#define M1KE_INPUT_H

int  input_poll(void);     /* non-blocking: char/KEY_* code, or -1 */
int  input_getchar(void);  /* blocking (halts CPU until a key) */

#endif
