/* timer.c - PIT channel 0 at a chosen frequency, drives IRQ0 */
#include "timer.h"
#include "io.h"
#include "../arch/isr.h"

static volatile uint64_t ticks;
static uint32_t freq_hz = 100;
static void (*tick_handler)(void);

void timer_set_tick_handler(void (*fn)(void)) { tick_handler = fn; }

static void timer_callback(registers_t *r) {
    (void)r;
    ticks++;
    if (tick_handler) tick_handler();
}

void timer_init(uint32_t frequency) {
    if (frequency == 0) frequency = 100;
    freq_hz = frequency;
    ticks = 0;
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36);                        /* channel 0, lobyte/hibyte, mode 3 */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    irq_install_handler(0, timer_callback);
}

uint64_t timer_ticks(void)   { return ticks; }
uint32_t timer_freq(void)    { return freq_hz; }
uint64_t timer_seconds(void) { return ticks / freq_hz; }

void timer_sleep_ms(uint32_t ms) {
    uint64_t target = ticks + (uint64_t)ms * freq_hz / 1000;
    while (ticks < target) { __asm__ __volatile__("hlt"); }
}
