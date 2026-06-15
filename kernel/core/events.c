/* events.c - ring buffer of timestamped system events (dmesg-like) */
#include "events.h"
#include "../lib/printf.h"
#include "../drivers/console.h"
#include "../drivers/timer.h"
#include <stdint.h>

#define EV_MAX 128
#define EV_MSGLEN 96

typedef struct {
    uint32_t   sec;
    ev_level_t level;
    char       msg[EV_MSGLEN];
} ev_entry_t;

static ev_entry_t ring[EV_MAX];
static int        ev_head;    /* next write slot */
static int        ev_total;   /* total logged */

void event_log(ev_level_t level, const char *fmt, ...) {
    ev_entry_t *e = &ring[ev_head];
    e->sec = (uint32_t)timer_seconds();
    e->level = level;
    va_list ap;
    va_start(ap, fmt);
    kvsnprintf(e->msg, EV_MSGLEN, fmt, ap);
    va_end(ap);
    ev_head = (ev_head + 1) % EV_MAX;
    ev_total++;
}

static const char *lvl_str(ev_level_t l) {
    switch (l) { case EV_WARN: return "WARN"; case EV_ERR: return "ERR "; case EV_OK: return "OK  "; default: return "INFO"; }
}
static uint32_t lvl_col(ev_level_t l) {
    switch (l) { case EV_WARN: return COL_YELLOW; case EV_ERR: return COL_RED; case EV_OK: return COL_GREEN; default: return COL_GRAY; }
}

int events_count(void) { return ev_total; }

void events_dump(int max) {
    int n = ev_total < EV_MAX ? ev_total : EV_MAX;
    if (max > 0 && max < n) n = max;
    int start = (ev_head - n + EV_MAX) % EV_MAX;
    for (int i = 0; i < n; i++) {
        ev_entry_t *e = &ring[(start + i) % EV_MAX];
        uint32_t fg, bg; console_get_color(&fg, &bg);
        kprintf("[%5u] ", e->sec);
        console_set_color(lvl_col(e->level), COL_BLACK);
        kprintf("%s ", lvl_str(e->level));
        console_set_color(fg, bg);
        kprintf("%s\n", e->msg);
    }
}
