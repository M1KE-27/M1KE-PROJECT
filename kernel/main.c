/* main.c - m1keOS kernel entry point */
#include <stdint.h>
#include "include/multiboot.h"
#include "include/io.h"
#include "drivers/serial.h"
#include "drivers/console.h"
#include "drivers/timer.h"
#include "drivers/keyboard.h"
#include "arch/gdt.h"
#include "arch/isr.h"
#include "mm/heap.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "fs/ramfs.h"
#include "pkg/pkg.h"
#include "process/process.h"
#include "syscall/syscall.h"
#include "shell/shell.h"
#include "core/config.h"
#include "core/control.h"
#include "core/events.h"
#include "gui/m1ss.h"
#include "lib/printf.h"
#include "lib/string.h"

/* bring up the core subsystems shared by both boot paths */
static void core_init(multiboot_info_t *mbi) {
    gdt_init();
    idt_init();
    pmm_init(mbi);                    /* physical memory bitmap from the Multiboot map */
    vmm_init();                       /* enable paging (full identity map) */
    heap_init();
    scheduler_init();                 /* turn the boot flow into task 0 (must precede timer) */
    ramfs_init();
    config_init();
    theme_init();                     /* load /etc/m1ke/theme.m1ss (m1ss) + apply accent */
    control_init();
    pkg_init();
    timer_init(100);
    keyboard_init();
    keyboard_set_layout(strcmp(config_get("input.layout", "us"), "es") == 0);
    sti();
    event_log(EV_OK, "kernel subsystems online");
}

/* ---------------- graphical boot splash ---------------- */
static void busy_delay(volatile uint32_t n) { while (n--) __asm__ __volatile__("nop"); }

static int centerx(const char *s, int scale) {
    int w = (int)strlen(s) * 8 * scale;
    return ((int)fb_width() - w) / 2;
}

static void splash_bg(void) {
    uint32_t W = fb_width(), H = fb_height();
    for (uint32_t y = 0; y < H; y++) {
        /* subtle dark vertical gradient with a warm tint */
        uint32_t t = (y * 24) / H;
        uint32_t col = (t << 16) | ((t / 2) << 8) | 0x06;
        fb_fillrect(0, (int)y, (int)W, 1, 0x00000000 | col);
    }
    /* accent rule lines */
    fb_fillrect((int)W/2 - 220, (int)H/2 - 150, 440, 2, COL_ORANGE_DK);
    fb_fillrect((int)W/2 - 220, (int)H/2 + 70, 440, 2, COL_ORANGE_DK);
}

static void splash_progress(int pct, const char *status) {
    uint32_t W = fb_width(), H = fb_height();
    int bw = 440, bh = 18;
    int bx = ((int)W - bw) / 2, by = (int)H / 2 + 130;
    fb_fillrect(bx - 2, by - 2, bw + 4, bh + 4, COL_ORANGE_DK);
    fb_fillrect(bx, by, bw, bh, 0x00141014);
    fb_fillrect(bx, by, bw * pct / 100, bh, COL_ORANGE);
    /* clear status line area then draw */
    fb_fillrect(bx, by + bh + 10, bw, 18, 0x00060406);
    fb_draw_string(bx, by + bh + 10, status, COL_AMBER, 0, true);
    char pc[8];
    ksnprintf(pc, sizeof(pc), "%d%%", pct);
    fb_draw_string(bx + bw - 40, by + bh + 10, pc, COL_WHITE, 0, true);
    fb_present();
    busy_delay(8000000);
}

static void splash_logo(void) {
    int s = 6;
    int x = centerx("m1keOS", s);
    int y = (int)fb_height() / 2 - 120;
    /* drop shadow then logo */
    fb_draw_string_scaled(x + 4, y + 4, "m1keOS", s, 0x00201005);
    fb_draw_string_scaled(x, y, "m1keOS", s, COL_ORANGE);
    const char *tag = "N A R A N J A   Y   N E G R O   E D I T I O N";
    fb_draw_string_scaled(centerx(tag, 1), y + 16 * s + 24, tag, 1, COL_AMBER);
    const char *ver = "kernel v0.7  -  32-bit  -  preemptive  -  paged";
    fb_draw_string(centerx(ver, 1), y + 16 * s + 48, ver, COL_GRAY, 0, true);
    fb_present();
}

static void boot_splash_graphical(multiboot_info_t *mbi) {
    splash_bg();
    splash_logo();
    splash_progress(18, "init: cpu, memory & interrupts...");
    splash_progress(46, "init: filesystem, config & packages...");
    core_init(mbi);
    splash_progress(80, "init: drivers (timer, keyboard, mouse)...");
    splash_progress(100, "m1keOS ready. launching shell...");
    timer_sleep_ms(1200);
    console_clear();
}

static void boot_text(multiboot_info_t *mbi) {
    console_set_color(COL_ORANGE, COL_BLACK);
    kprintf("\n");
    kprintf("    ___ ___  _ _  ___ ___  ___\n");
    kprintf("   |   |_  || | || __/ _ \\/ __|     m1keOS v0.7 \"Naranja\"\n");
    kprintf("   | | | | || | || _| (_) \\__ \\     orange & black hacker edition\n");
    kprintf("   |_|_|_|_||___||___\\___/|___/     hyper-personalizable\n");
    console_set_color(COL_WHITE, COL_BLACK);
    kprintf("\n");
    core_init(mbi);
}

static void banner(void) {
    console_set_color(theme_accent(), COL_BLACK);
    kprintf("  m1keOS v0.7 \"Naranja\"");
    console_set_color(COL_GRAY, COL_BLACK);
    kprintf("  -  control plane: type 'm1kectl help'\n");
    console_set_color(COL_WHITE, COL_BLACK);
}

/* demo tasks: prove preemptive multitasking by heartbeating over the serial
 * port (kept off the screen so they don't fight the shell for the console). */
static void worker_alpha(void) {
    for (;;) { serial_write("    [task alpha] heartbeat\r\n"); task_sleep(900); }
}
static void worker_beta(void) {
    for (;;) { serial_write("    [task beta ] heartbeat\r\n"); task_sleep(1400); }
}
static void worker_gamma(void) {
    int n = 0;
    for (;;) {
        char b[48];
        ksnprintf(b, sizeof(b), "    [task gamma] beat #%d (pid %u)\r\n", n++, getpid());
        serial_write(b);
        task_sleep(2000);
    }
}
/* this task talks to the kernel ONLY through int 0x80 syscalls */
static void worker_delta(void) {
    const char *hello = "    [task delta] hello through int 0x80\n";
    for (;;) {
        sys_write(1, hello, (int)strlen(hello));
        char b[64];
        ksnprintf(b, sizeof(b), "    [task delta] sys_getpid=%d uptime=%ds free=%dKB\n",
                  sys_getpid(), sys_uptime(), sys_meminfo_kb());
        sys_write(1, b, (int)strlen(b));
        sys_sleep(1700);
    }
}

void kmain(uint32_t magic, multiboot_info_t *mbi) {
    serial_init();
    console_init(mbi);
    console_clear();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        serial_write("[warn] unexpected multiboot magic\n");

    if (console_is_graphical()) boot_splash_graphical(mbi);
    else boot_text(mbi);

    banner();
    if (console_is_graphical())
        kprintf("  display: framebuffer %ux%u 32bpp\n\n", fb_width(), fb_height());
    else
        kprintf("  display: VGA text mode\n\n");

    /* spawn a few concurrent kernel tasks (preemption demo) */
    task_create("alpha", worker_alpha);
    task_create("beta",  worker_beta);
    task_create("gamma", worker_gamma);
    task_create("delta", worker_delta);   /* uses syscalls (int 0x80) */
    event_log(EV_OK, "scheduler: %d tasks running", task_count());

    shell_init();
    shell_run();   /* runs as task 0, preempted alongside the workers */

    for (;;) __asm__ __volatile__("hlt");
}
