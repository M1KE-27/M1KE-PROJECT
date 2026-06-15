/* mouse.h - PS/2 mouse driver */
#ifndef M1KE_MOUSE_H
#define M1KE_MOUSE_H
#include <stdbool.h>

void mouse_init(int screen_w, int screen_h);
int  mouse_x(void);
int  mouse_y(void);
bool mouse_left(void);

#endif
