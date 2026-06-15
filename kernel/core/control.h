/* control.h - m1kectl control plane + dynamic module table */
#ifndef M1KE_CONTROL_H
#define M1KE_CONTROL_H
#include <stdbool.h>

void control_init(void);                       /* register kobjects + modules */
void control_command(int argc, char **argv);   /* argv excludes leading "m1kectl" */
void control_apply_config(void);               /* push config -> live state (theme) */

bool module_enabled(const char *name);

#endif
