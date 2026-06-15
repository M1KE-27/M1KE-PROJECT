/* pkg.h - m1pkg, the m1keOS package manager */
#ifndef M1KE_PKG_H
#define M1KE_PKG_H

void pkg_init(void);
/* handle a "m1pkg ..." command line (argv excludes the leading "m1pkg") */
void pkg_command(int argc, char **argv);

#endif
