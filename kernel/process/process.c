/* process.c - preemptive round-robin scheduler.
 *
 * The boot flow becomes task 0. Each created task gets a 16 KB stack and a
 * hand-crafted initial frame that switch_context() "returns" into, so the very
 * first switch starts the task at its entry point with interrupts enabled.
 * The timer IRQ calls scheduler_tick(); after the quantum expires it preempts. */
#include "process.h"
#include "../mm/heap.h"
#include "../lib/string.h"
#include "../drivers/timer.h"
#include "../include/io.h"

#define STACK_SIZE     16384
#define SCHED_QUANTUM  2          /* ticks per slice (2 * 10ms = 20ms) */

/* implemented in arch/lowlevel.asm */
extern void switch_context(uint32_t *save_to, uint32_t load_val);

static task_t   tasks[MAX_TASKS];
static bool     slot_used[MAX_TASKS];
static task_t  *current;
static task_t  *ring_head;        /* task 0 (kernel) */
static uint32_t next_pid;
static bool     enabled;
static int      quantum;

const char *task_state_name(task_state_t s) {
    switch (s) {
        case TASK_RUNNING:  return "run";
        case TASK_READY:    return "ready";
        case TASK_SLEEPING: return "sleep";
        default:            return "dead";
    }
}

static task_t *alloc_pcb(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!slot_used[i]) { slot_used[i] = true; memset(&tasks[i], 0, sizeof(task_t)); return &tasks[i]; }
    }
    /* reclaim a dead task's slot */
    for (int i = 0; i < MAX_TASKS; i++) {
        if (slot_used[i] && tasks[i].state == TASK_DEAD) {
            if (tasks[i].stack_base) kfree(tasks[i].stack_base);
            memset(&tasks[i], 0, sizeof(task_t));
            return &tasks[i];
        }
    }
    return 0;
}

void scheduler_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) slot_used[i] = false;
    next_pid = 0;

    task_t *t = alloc_pcb();
    t->pid = next_pid++;
    strcpy(t->name, "kernel");
    t->state = TASK_RUNNING;
    t->stack_base = 0;            /* uses the boot stack */
    t->next = t;                 /* ring of one */

    ring_head = current = t;
    quantum = SCHED_QUANTUM;
    enabled = true;

    timer_set_tick_handler(scheduler_tick);
}

task_t *task_create(const char *name, void (*entry)(void)) {
    cli();
    task_t *t = alloc_pcb();
    if (!t) { sti(); return 0; }

    t->pid = next_pid++;
    strncpy(t->name, name, TASK_NAME_MAX - 1);
    t->name[TASK_NAME_MAX - 1] = 0;
    t->stack_base = (uint32_t *)kmalloc(STACK_SIZE);
    if (!t->stack_base) { slot_used[t - tasks] = false; sti(); return 0; }
    t->state = TASK_READY;

    /* build the initial stack frame that switch_context() will pop into:
     * (low) eflags, ebx, esi, edi, ebp, entry, task_exit (high) */
    uint32_t *sp = (uint32_t *)((uint8_t *)t->stack_base + STACK_SIZE);
    *--sp = (uint32_t)task_exit;     /* return address if entry() returns */
    *--sp = (uint32_t)entry;         /* switch_context 'ret' lands here */
    *--sp = 0;                       /* ebp */
    *--sp = 0;                       /* edi */
    *--sp = 0;                       /* esi */
    *--sp = 0;                       /* ebx */
    *--sp = 0x00000202;              /* eflags: IF=1 (interrupts on) */
    t->esp = (uint32_t)sp;

    /* insert right after the kernel task in the ring */
    t->next = ring_head->next;
    ring_head->next = t;

    sti();
    return t;
}

/* pick the next runnable task and switch. Must be called with IRQs disabled. */
static void schedule(void) {
    task_t *prev = current;
    task_t *n = prev->next;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (n->state == TASK_READY) break;
        n = n->next;
    }
    if (n->state != TASK_READY) {
        /* nobody else is runnable */
        if (prev->state == TASK_RUNNING) return;   /* keep running prev */
        n = prev;                                   /* last resort */
    }
    if (n == prev) { prev->state = TASK_RUNNING; return; }

    if (prev->state == TASK_RUNNING) prev->state = TASK_READY;
    n->state = TASK_RUNNING;
    current = n;
    switch_context(&prev->esp, n->esp);
}

void scheduler_tick(void) {
    if (!enabled) return;

    uint64_t now = timer_ticks();
    current->cpu_ticks++;

    /* wake any sleepers whose time has come */
    task_t *t = ring_head;
    do {
        if (t->state == TASK_SLEEPING && now >= t->wake_tick) t->state = TASK_READY;
        t = t->next;
    } while (t != ring_head);

    if (--quantum > 0) return;
    quantum = SCHED_QUANTUM;
    schedule();
}

void task_yield(void) {
    if (!enabled) return;
    cli();
    schedule();
    sti();
}

void task_sleep(uint32_t ms) {
    if (!enabled) { timer_sleep_ms(ms); return; }
    cli();
    current->wake_tick = timer_ticks() + (uint64_t)ms * timer_freq() / 1000;
    current->state = TASK_SLEEPING;
    schedule();
    sti();
}

void task_exit(void) {
    cli();
    current->state = TASK_DEAD;
    schedule();
    /* never reached, but just in case we are scheduled again */
    for (;;) { sti(); __asm__ __volatile__("hlt"); }
}

int task_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_TASKS; i++)
        if (slot_used[i] && tasks[i].state != TASK_DEAD) n++;
    return n;
}

task_t  *task_current(void)     { return current; }
uint32_t getpid(void)           { return current ? current->pid : 0; }
bool     scheduler_enabled(void){ return enabled; }

void task_foreach(task_iter_t cb) {
    if (!ring_head) return;
    task_t *t = ring_head;
    do { cb(t); t = t->next; } while (t != ring_head);
}
