/* shell.h - interactive command interpreter */
#ifndef M1KE_SHELL_H
#define M1KE_SHELL_H
void shell_init(void);
void shell_run(void);                  /* never returns */
void shell_exec_line(char *line);      /* run one command line (modifies line) */
#endif
