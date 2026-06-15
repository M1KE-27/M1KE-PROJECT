/* events.h - system event/log bus (observable system) */
#ifndef M1KE_EVENTS_H
#define M1KE_EVENTS_H

typedef enum { EV_INFO, EV_WARN, EV_ERR, EV_OK } ev_level_t;

void event_log(ev_level_t level, const char *fmt, ...);
void events_dump(int max);     /* print up to `max` recent events (0 = all) */
int  events_count(void);

#endif
