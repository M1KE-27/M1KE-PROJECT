/* control.c - m1kectl: the unified, scriptable control & inspection plane.
 * Every subsystem is a kobject with inspect() + ctl(); modules can be
 * loaded/unloaded at runtime (real effect: PIC IRQ mask). */
#include "control.h"
#include "registry.h"
#include "config.h"
#include "events.h"
#include "../drivers/console.h"
#include "../drivers/pic.h"
#include "../drivers/timer.h"
#include "../drivers/keyboard.h"
#include "../process/process.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../mm/heap.h"
#include "../syscall/syscall.h"
#include "../gui/m1ss.h"
#include "../lib/printf.h"
#include "../lib/string.h"

/* ----------------- accent palette (name <-> color) ----------------- */
static const struct { const char *name; uint32_t col; } palette[] = {
    { "orange", COL_ORANGE }, { "rose", 0x00FF4D6D }, { "green", 0x0046E07A },
    { "blue", 0x005AA8FF }, { "purple", 0x00B98CFF }, { "gold", 0x00FFD24A },
    { "white", COL_WHITE },
};
#define N_PAL ((int)(sizeof(palette)/sizeof(palette[0])))

static bool accent_lookup(const char *name, uint32_t *out) {
    for (int i = 0; i < N_PAL; i++)
        if (strcmp(palette[i].name, name) == 0) { *out = palette[i].col; return true; }
    return false;
}

void control_apply_config(void) {
    uint32_t c;
    if (accent_lookup(config_get("ui.accent", "orange"), &c)) theme_set_accent(c);
}

/* ----------------- module table (dynamic) ----------------- */
typedef struct { const char *name; const char *kind; int irq; bool core; bool enabled; const char *desc; } module_t;
static module_t modules[] = {
    { "serial",   "driver",    4, true,  true, "COM1 serial console" },
    { "timer",    "driver",    0, true,  true, "PIT 100Hz system timer" },
    { "keyboard", "driver",    1, false, true, "PS/2 keyboard" },
    { "mouse",    "driver",   12, false, true, "PS/2 mouse" },
    { "console",  "driver",   -1, true,  true, "framebuffer / VGA console" },
    { "heap",     "subsystem",-1, true,  true, "kernel heap allocator" },
    { "ramfs",    "subsystem",-1, true,  true, "in-memory filesystem" },
    { "m1pkg",    "subsystem",-1, true,  true, "package manager" },
};
#define N_MOD ((int)(sizeof(modules)/sizeof(modules[0])))

static module_t *mod_find(const char *n) {
    for (int i = 0; i < N_MOD; i++) if (strcmp(modules[i].name, n) == 0) return &modules[i];
    return 0;
}
bool module_enabled(const char *name) { module_t *m = mod_find(name); return m ? m->enabled : false; }

static void mod_list(void) {
    kprintf("  %-10s %-9s %-7s %s\n", "MODULE", "KIND", "STATE", "DESCRIPTION");
    for (int i = 0; i < N_MOD; i++) {
        module_t *m = &modules[i];
        console_set_color(theme_accent(), COL_BLACK);
        kprintf("  %-10s", m->name);
        console_set_color(COL_GRAY, COL_BLACK);
        kprintf(" %-9s ", m->kind);
        console_set_color(m->enabled ? COL_GREEN : COL_RED, COL_BLACK);
        kprintf("%-7s", m->enabled ? "loaded" : "unloaded");
        console_set_color(COL_WHITE, COL_BLACK);
        kprintf(" %s%s\n", m->desc, m->core ? "  (core)" : "");
    }
}

static void mod_set(const char *name, bool on) {
    module_t *m = mod_find(name);
    if (!m) { kprintf("m1kectl: no such module '%s'\n", name); return; }
    if (m->core && !on) {
        kprintf("m1kectl: '%s' is a core module and cannot be unloaded\n", name);
        return;
    }
    if (m->enabled == on) { kprintf("m1kectl: '%s' already %s\n", name, on ? "loaded" : "unloaded"); return; }
    m->enabled = on;
    if (m->irq >= 0) { if (on) pic_clear_mask((unsigned char)m->irq); else pic_set_mask((unsigned char)m->irq); }
    event_log(on ? EV_OK : EV_WARN, "module %s %s", name, on ? "loaded" : "unloaded");
    kprintf("m1kectl: module '%s' %s\n", name, on ? "loaded" : "unloaded");
}

/* ----------------- kobjects ----------------- */
static void k_kernel_inspect(void) {
    kprintf("kernel    : m1keOS v0.7 \"Naranja\" (hybrid, 32-bit)\n");
    kprintf("uptime    : %u s (%u ticks)\n", (unsigned)timer_seconds(), (unsigned)timer_ticks());
    kprintf("heap      : %u / %u KB\n", (unsigned)(heap_used()/1024), (unsigned)(heap_total()/1024));
    kprintf("display   : %s\n", console_is_graphical() ? "framebuffer 32bpp" : "VGA text");
    kprintf("objects   : %d registered\n", reg_count());
    kprintf("events    : %d logged\n", events_count());
    kprintf("syscalls  : %u served (int 0x80)\n", (unsigned)syscall_count());
}
static int k_kernel_ctl(int argc, char **argv) {
    if (argc >= 1 && !strcmp(argv[0], "config")) { kprintf("kernel + scheduler config:\n"); config_dump("scheduler."); return 0; }
    k_kernel_inspect(); return 0;
}

static void sched_task_row(const task_t *t) {
    kprintf("    %3u  %-6s %5u  %s\n", t->pid, task_state_name(t->state),
            (unsigned)t->cpu_ticks, t->name);
}
static void k_sched_inspect(void) {
    kprintf("scheduler : round-robin preemptive (IRQ0), %d tasks\n", task_count());
    kprintf("policy    : %s, quantum=%s ticks\n",
            config_get("scheduler.policy", "round-robin"), config_get("scheduler.quantum", "2"));
    kprintf("    PID  STATE   CPU  NAME\n");
    task_foreach(sched_task_row);
}
static int k_sched_ctl(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[0], "set")) {
        const char *p = argv[1];
        if (strcmp(p,"cooperative") && strcmp(p,"round-robin") && strcmp(p,"priority")) {
            kprintf("m1kectl: unknown policy '%s' (cooperative|round-robin|priority)\n", p); return 1;
        }
        config_set("scheduler.policy", p); config_save();
        event_log(EV_OK, "scheduler policy set to %s", p);
        kprintf("scheduler policy = %s (saved)\n", p);
        return 0;
    }
    k_sched_inspect(); return 0;
}

static void k_mem_inspect(void) {
    uint64_t pt = pmm_total_bytes(), pu = pmm_used_bytes();
    int ppct = pt ? (int)(pu * 100 / pt) : 0;
    kprintf("physical  : %u MB total, %u MB used, %u MB free (%d%%)\n",
            (unsigned)(pt/1048576), (unsigned)(pu/1048576),
            (unsigned)(pmm_free_bytes()/1048576), ppct);
    kprintf("  [");
    int w = 30, pf = ppct * w / 100;
    for (int i = 0; i < w; i++) kputc(i < pf ? '#' : '.');
    kprintf("]\n");
    size_t hu = heap_used(), ht = heap_total();
    kprintf("kernel heap: %u KB used / %u KB total\n", (unsigned)(hu/1024), (unsigned)(ht/1024));
    kprintf("paging    : %s (identity map, 4 MB pages, cr3=%p)\n",
            vmm_enabled() ? "enabled" : "off", (void *)vmm_cr3());
}

static void k_wm_inspect(void) {
    theme_t *t = theme_get();
    kprintf("wm/desktop: m1wm compositor (m1ss-themed, glass)\n");
    kprintf("style sheet   = %s\n", THEME_PATH);
    kprintf("window radius = %d px, blur = %d, opacity = %d, shadow = %d\n",
            t->win_radius, t->win_blur, t->win_opacity, t->win_shadow);
    kprintf("taskbar       = %s, %s, height %d\n",
            t->bar_visible ? "visible" : "hidden", t->bar_pos ? "top" : "bottom", t->bar_height);
}

/* delegate visual control to the m1ss theme (live + persisted) */
static int k_wm_ctl(int argc, char **argv) {
    if (argc >= 1 && (!strcmp(argv[0], "config") || !strcmp(argv[0], "show"))) { theme_show(); return 0; }
    if (argc >= 1 && !strcmp(argv[0], "theme")) {
        if (argc >= 3 && !strcmp(argv[1], "set")) {
            if (!theme_set("accent", argv[2])) { kprintf("m1kectl: unknown color '%s'\n", argv[2]); return 1; }
            event_log(EV_OK, "theme accent set to %s", argv[2]);
            kprintf("theme accent = %s (applied + saved to %s)\n", argv[2], THEME_PATH);
            return 0;
        }
        if (argc >= 2 && !strcmp(argv[1], "reload")) { theme_reload(); kprintf("theme reloaded from %s\n", THEME_PATH); return 0; }
        kprintf("usage: m1kectl desktop theme set <color> | reload\n"); return 1;
    }
    if (argc >= 2 && !strcmp(argv[0], "border-radius")) {
        theme_set("window.radius", argv[1]);
        event_log(EV_OK, "window radius = %s", argv[1]);
        kprintf("window radius = %s px (saved; visible on next 'gui')\n", argv[1]);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[0], "wallpaper")) { theme_set("wallpaper", argv[1]); kprintf("wallpaper = %s (saved)\n", argv[1]); return 0; }
    if (argc >= 2 && !strcmp(argv[0], "blur"))      { theme_set("window.blur", argv[1]); kprintf("window blur = %s (saved)\n", argv[1]); return 0; }
    k_wm_inspect(); return 0;
}

static void k_shell_inspect(void) {
    kprintf("shell     : m1sh\n");
    kprintf("prompt    = %s\n", config_get("shell.prompt", "m1ke@m1keOS"));
}
static int k_shell_ctl(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[0], "prompt") && !strcmp(argv[1], "set")) {
        config_set("shell.prompt", argv[2]); config_save();
        kprintf("shell prompt = %s (saved)\n", argv[2]);
        return 0;
    }
    k_shell_inspect(); return 0;
}

static void k_net_inspect(void) {
    bool on = strcmp(config_get("net.enabled", "false"), "true") == 0;
    kprintf("network   : %s (host=%s)\n", on ? "running" : "stopped", config_get("net.hostname","m1keos"));
    kprintf("note      : TCP/IP stack arrives in Fase 8\n");
}
static int k_net_ctl(int argc, char **argv) {
    if (argc >= 1) {
        if (!strcmp(argv[0], "start") || !strcmp(argv[0], "restart")) {
            config_set("net.enabled", "true"); config_save();
            event_log(EV_OK, "service network %s", argv[0]);
            kprintf("network service %sed\n", argv[0]); return 0;
        }
        if (!strcmp(argv[0], "stop")) {
            config_set("net.enabled", "false"); config_save();
            event_log(EV_WARN, "service network stopped");
            kprintf("network service stopped\n"); return 0;
        }
    }
    k_net_inspect(); return 0;
}

static void k_theme_inspect(void) {
    kprintf("theme     : m1ss style sheet at %s\n", THEME_PATH);
    kprintf("edit it, then: m1kectl theme reload\n");
    theme_show();
}
static int k_theme_ctl(int argc, char **argv) {
    if (argc >= 1 && !strcmp(argv[0], "show"))   { theme_show(); return 0; }
    if (argc >= 1 && !strcmp(argv[0], "reload")) { theme_reload(); event_log(EV_OK, "m1ss theme reloaded"); kprintf("theme reloaded from %s\n", THEME_PATH); return 0; }
    if (argc >= 1 && !strcmp(argv[0], "edit"))   { kprintf("editable style sheet (%s):\n\n", THEME_PATH); theme_show(); kprintf("\nedit with the GUI Editor or 'm1kectl theme set <prop> <val>', then reload.\n"); return 0; }
    if (argc >= 3 && !strcmp(argv[0], "set")) {
        if (theme_set(argv[1], argv[2])) { event_log(EV_OK, "theme %s = %s", argv[1], argv[2]); kprintf("%s = %s (applied + saved)\n", argv[1], argv[2]); }
        else kprintf("m1kectl: cannot set theme '%s'\n", argv[1]);
        return 0;
    }
    k_theme_inspect(); return 0;
}

static void k_kbd_inspect(void) {
    kprintf("keyboard  : PS/2, layout = %s\n", keyboard_layout() ? "es (Spanish)" : "us");
    kprintf("switch with: m1kectl keyboard layout es | us\n");
}
static int k_kbd_ctl(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[0], "layout")) {
        int es;
        if      (!strcmp(argv[1], "es")) es = 1;
        else if (!strcmp(argv[1], "us")) es = 0;
        else { kprintf("m1kectl: unknown layout '%s' (es | us)\n", argv[1]); return 1; }
        keyboard_set_layout(es);
        config_set("input.layout", es ? "es" : "us"); config_save();
        event_log(EV_OK, "keyboard layout = %s", es ? "es" : "us");
        kprintf("keyboard layout = %s (applied + saved)\n", es ? "es (Spanish)" : "us");
        return 0;
    }
    k_kbd_inspect(); return 0;
}

static kobject_t obj_kbd     = { "keyboard",  "driver",    "PS/2 keyboard + layout",    k_kbd_inspect,    k_kbd_ctl,    0 };
static kobject_t obj_theme   = { "theme",     "subsystem", "m1ss visual style sheet",   k_theme_inspect,  k_theme_ctl,  0 };
static kobject_t obj_kernel  = { "kernel",    "subsystem", "core kernel state",         k_kernel_inspect, k_kernel_ctl, 0 };
static kobject_t obj_sched   = { "scheduler", "subsystem", "task scheduler",            k_sched_inspect,  k_sched_ctl,  0 };
static kobject_t obj_mem     = { "mem",       "subsystem", "memory / heap",             k_mem_inspect,    0,            0 };
static kobject_t obj_wm      = { "wm",        "subsystem", "window manager / desktop",  k_wm_inspect,     k_wm_ctl,     0 };
static kobject_t obj_desktop = { "desktop",   "subsystem", "alias of wm",               k_wm_inspect,     k_wm_ctl,     0 };
static kobject_t obj_shell   = { "shell",     "subsystem", "m1sh command shell",        k_shell_inspect,  k_shell_ctl,  0 };
static kobject_t obj_net     = { "network",   "service",   "TCP/IP networking",         k_net_inspect,    k_net_ctl,    0 };

void control_init(void) {
    reg_register(&obj_kernel);
    reg_register(&obj_sched);
    reg_register(&obj_mem);
    reg_register(&obj_wm);
    reg_register(&obj_desktop);
    reg_register(&obj_shell);
    reg_register(&obj_kbd);
    reg_register(&obj_theme);
    reg_register(&obj_net);
    event_log(EV_OK, "control plane online (%d objects)", reg_count());
}

/* ----------------- inspect / process helpers ----------------- */
static int  pi_target;
static void pi_one(const task_t *t) {
    if ((int)t->pid != pi_target) return;
    kprintf("pid     : %u\n", t->pid);
    kprintf("name    : %s\n", t->name);
    kprintf("state   : %s\n", task_state_name(t->state));
    kprintf("cpu     : %u ticks\n", (unsigned)t->cpu_ticks);
    kprintf("stack   : %p\n", (void *)t->stack_base);
    kprintf("ring    : 0 (kernel space - userspace arrives in Fase 4)\n");
}
static void pi_row(const task_t *t) {
    kprintf("  %3u  %-6s %5u  %s\n", t->pid, task_state_name(t->state), (unsigned)t->cpu_ticks, t->name);
}
static void inspect_process(int argc, char **argv) {
    if (argc >= 1) { pi_target = atoi(argv[0]); task_foreach(pi_one); return; }
    kprintf("  PID  STATE   CPU  NAME\n");
    task_foreach(pi_row);
}

/* ----------------- top-level dispatch ----------------- */
static void usage(void) {
    console_set_color(theme_accent(), COL_BLACK);
    kprintf("m1kectl - m1keOS control plane\n");
    console_set_color(COL_WHITE, COL_BLACK);
    kprintf("inspection:\n");
    kprintf("  m1kectl list [kind]            list registered objects\n");
    kprintf("  m1kectl inspect <obj>          show object state\n");
    kprintf("  m1kectl inspect process [pid]  show process(es)\n");
    kprintf("  m1kectl events [N] | dmesg     show the event log\n");
    kprintf("control:\n");
    kprintf("  m1kectl <obj> <verb> ...       e.g. scheduler set round-robin\n");
    kprintf("  m1kectl desktop theme set blue\n");
    kprintf("  m1kectl wm border-radius 20\n");
    kprintf("  m1kectl shell prompt set hax0r\n");
    kprintf("  m1kectl service restart network\n");
    kprintf("  m1kectl module list|load|unload <name>\n");
    kprintf("config:\n");
    kprintf("  m1kectl config [list|get|set|save|reload] ...\n");
    kprintf("  m1kectl edit <section>         show editable config section\n");
}

void control_command(int argc, char **argv) {
    if (argc < 1) { usage(); return; }
    const char *v = argv[0];

    if (!strcmp(v, "help")) { usage(); return; }
    if (!strcmp(v, "list")) { reg_list(argc > 1 ? argv[1] : 0); return; }
    if (!strcmp(v, "events") || !strcmp(v, "dmesg")) { events_dump(argc > 1 ? atoi(argv[1]) : 0); return; }

    if (!strcmp(v, "inspect")) {
        if (argc < 2) { kprintf("usage: m1kectl inspect <obj|process>\n"); return; }
        if (!strcmp(argv[1], "process")) { inspect_process(argc - 2, argv + 2); return; }
        kobject_t *o = reg_find(argv[1]);
        if (o && o->inspect) o->inspect();
        else kprintf("m1kectl: no inspectable object '%s'\n", argv[1]);
        return;
    }

    if (!strcmp(v, "module")) {
        if (argc < 2) { mod_list(); return; }
        if (!strcmp(argv[1], "list")) { mod_list(); return; }
        if (!strcmp(argv[1], "unload") && argc > 2) { mod_set(argv[2], false); return; }
        if (!strcmp(argv[1], "load") && argc > 2)   { mod_set(argv[2], true); return; }
        kprintf("usage: m1kectl module list|load|unload <name>\n"); return;
    }

    if (!strcmp(v, "service")) {
        if (argc < 2 || !strcmp(argv[1], "list")) { reg_list("service"); return; }
        /* accept both "service <name> <action>" and "service <action> <name>" */
        kobject_t *o = reg_find(argv[1]);
        if (o && !strcmp(o->kind, "service") && o->ctl) { o->ctl(argc - 2, argv + 2); return; }
        if (argc > 2) {
            o = reg_find(argv[2]);
            if (o && !strcmp(o->kind, "service") && o->ctl) {
                char *act[1] = { argv[1] };
                o->ctl(1, act); return;
            }
        }
        kprintf("m1kectl: no such service in '%s ...'\n", argv[1]);
        return;
    }

    if (!strcmp(v, "config")) {
        if (argc < 2 || !strcmp(argv[1], "list")) { config_dump(argc > 2 ? argv[2] : 0); return; }
        if (!strcmp(argv[1], "get") && argc > 2) { kprintf("%s\n", config_get(argv[2], "(unset)")); return; }
        if (!strcmp(argv[1], "set") && argc > 3) {
            config_set(argv[2], argv[3]); config_save(); control_apply_config();
            kprintf("%s = %s (saved)\n", argv[2], argv[3]); return;
        }
        if (!strcmp(argv[1], "save")) { config_save(); kprintf("config saved to %s\n", CONFIG_PATH); return; }
        if (!strcmp(argv[1], "reload")) { config_init(); control_apply_config(); kprintf("config reloaded\n"); return; }
        kprintf("usage: m1kectl config [list|get|set|save|reload]\n"); return;
    }

    if (!strcmp(v, "edit")) {
        kprintf("editable config (text at %s):\n", CONFIG_PATH);
        config_dump(argc > 1 ? argv[1] : 0);
        kprintf("use 'm1kectl config set <key> <value>' to change, or edit the file.\n");
        return;
    }

    /* fall through: treat first token as an object name */
    kobject_t *o = reg_find(v);
    if (o && o->ctl) { o->ctl(argc - 1, argv + 1); return; }
    if (o && o->inspect) { o->inspect(); return; }
    kprintf("m1kectl: unknown command '%s' (try 'm1kectl help')\n", v);
}
