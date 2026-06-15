/* process.h - preemptive round-robin scheduler with real kernel tasks.
 *
 * Single address space (no paging yet): tasks are kernel threads, each with its
 * own stack and PCB. Preemption is driven by the timer IRQ (IRQ0). */
#ifndef M1KE_PROCESS_H
#define M1KE_PROCESS_H
#include <stdint.h>
#include <stdbool.h>

#define TASK_NAME_MAX 32
#define MAX_TASKS     32          /* >= 16 concurrent processes */

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

typedef struct task {
    uint32_t      esp;            /* saved stack pointer (used by switch_context) */
    uint32_t      pid;
    char          name[TASK_NAME_MAX];
    task_state_t  state;
    uint32_t     *stack_base;     /* heap-allocated stack (NULL for the boot task) */
    uint64_t      wake_tick;      /* tick to wake at while sleeping */
    uint64_t      cpu_ticks;      /* accumulated scheduling ticks (CPU time) */
    struct task  *next;           /* circular run-queue */
} task_t;

void      scheduler_init(void);                         /* turns the boot flow into task 0 */
task_t   *task_create(const char *name, void (*entry)(void));
void      scheduler_tick(void);                         /* called from the timer IRQ */
void      task_yield(void);                             /* voluntarily give up the CPU */
void      task_sleep(uint32_t ms);                      /* block this task for a while */
void      task_exit(void);                              /* end the current task */

int       task_count(void);                             /* live (non-dead) tasks */
task_t   *task_current(void);
uint32_t  getpid(void);
bool      scheduler_enabled(void);

typedef void (*task_iter_t)(const task_t *);
void      task_foreach(task_iter_t cb);                 /* iterate tasks (ps, sysmonitor) */
const char *task_state_name(task_state_t s);

#endif
