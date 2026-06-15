/* timer.h - PIT (programmable interval timer) */
#ifndef M1KE_TIMER_H
#define M1KE_TIMER_H
#include <stdint.h>
void     timer_init(uint32_t frequency);
uint64_t timer_ticks(void);
uint32_t timer_freq(void);
uint64_t timer_seconds(void);
void     timer_sleep_ms(uint32_t ms);
/* optional hook invoked on every tick (used by the scheduler) */
void     timer_set_tick_handler(void (*fn)(void));
#endif
