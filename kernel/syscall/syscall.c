/* syscall.c - int 0x80 dispatcher and the implementations behind it. */
#include "syscall.h"
#include "../process/process.h"
#include "../drivers/timer.h"
#include "../drivers/serial.h"
#include "../mm/pmm.h"

static uint64_t total;

/* fd 1 (stdout) / 2 (stderr) -> serial console for now */
static int do_write(int fd, const char *buf, int len) {
    if (!buf || len < 0) return -1;
    if (fd != 1 && fd != 2) return -1;
    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') serial_putc('\r');
        serial_putc(buf[i]);
    }
    return len;
}

void syscall_dispatch(registers_t *r) {
    total++;
    int ret = -1;
    switch (r->eax) {
        case SYS_EXIT:        task_exit();                         break;  /* no return */
        case SYS_WRITE:       ret = do_write((int)r->ebx, (const char *)r->ecx, (int)r->edx); break;
        case SYS_GETPID:      ret = (int)getpid();                 break;
        case SYS_SLEEP:       task_sleep(r->ebx); ret = 0;         break;
        case SYS_YIELD:       task_yield();       ret = 0;         break;
        case SYS_UPTIME:      ret = (int)timer_seconds();          break;
        case SYS_GETMEMINFO:  ret = (int)(pmm_free_bytes() / 1024); break;
        default:              ret = -1;                            break;
    }
    r->eax = (uint32_t)ret;
}

uint64_t syscall_count(void) { return total; }
