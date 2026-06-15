/* syscall.h - system call ABI (software interrupt 0x80).
 *
 * EAX = syscall number, EBX/ECX/EDX = args, return value in EAX.
 * Callable today from kernel tasks (ring0->ring0); the same gate (DPL=3) will
 * serve ring3 userspace once per-process address spaces + a TSS land. */
#ifndef M1KE_SYSCALL_H
#define M1KE_SYSCALL_H
#include <stdint.h>
#include "../arch/isr.h"

/* numbers follow the project's roadmap table */
#define SYS_EXIT        1
#define SYS_WRITE       4
#define SYS_GETPID      7
#define SYS_SLEEP       9
#define SYS_YIELD       10
#define SYS_UPTIME      30
#define SYS_GETMEMINFO  31

void     syscall_dispatch(registers_t *r);   /* called from the int 0x80 handler */
uint64_t syscall_count(void);

/* ---- inline wrappers (issue the int 0x80 from C) ---- */
static inline int syscall0(int n) {
    int r; __asm__ __volatile__("int $0x80" : "=a"(r) : "a"(n) : "memory"); return r;
}
static inline int syscall1(int n, int a) {
    int r; __asm__ __volatile__("int $0x80" : "=a"(r) : "a"(n), "b"(a) : "memory"); return r;
}
static inline int syscall3(int n, int a, int b, int c) {
    int r; __asm__ __volatile__("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory"); return r;
}

static inline int  sys_getpid(void)                       { return syscall0(SYS_GETPID); }
static inline int  sys_write(int fd, const char *b, int n){ return syscall3(SYS_WRITE, fd, (int)(uintptr_t)b, n); }
static inline int  sys_sleep(int ms)                      { return syscall1(SYS_SLEEP, ms); }
static inline void sys_yield(void)                        { (void)syscall0(SYS_YIELD); }
static inline int  sys_uptime(void)                       { return syscall0(SYS_UPTIME); }
static inline int  sys_meminfo_kb(void)                   { return syscall0(SYS_GETMEMINFO); }
static inline void sys_exit(void)                         { (void)syscall0(SYS_EXIT); }

#endif
