/* shell.c - m1keOS interactive shell (works over keyboard or serial) */
#include "shell.h"
#include "../drivers/input.h"
#include "../drivers/keyboard.h"
#include "../drivers/console.h"
#include "../drivers/timer.h"
#include "../include/io.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include "../mm/heap.h"
#include "../fs/ramfs.h"
#include "../pkg/pkg.h"
#include "../core/control.h"
#include "../core/config.h"
#include "../core/events.h"
#include "../process/process.h"

/* provided by the GUI module */
void desktop_run(void);

#define LINE_MAX 256
#define HIST_MAX 16
#define ARG_MAX  32

static fs_node_t *cwd;
static char hist[HIST_MAX][LINE_MAX];
static int  hist_count, hist_pos;

/* ----------------------- RTC (CMOS) for `date` ----------------------- */
static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}
static uint8_t bcd2bin(uint8_t v) { return (uint8_t)((v & 0x0F) + ((v >> 4) * 10)); }

static void print_datetime(void) {
    uint8_t sec = bcd2bin(cmos_read(0x00));
    uint8_t min = bcd2bin(cmos_read(0x02));
    uint8_t hr  = bcd2bin(cmos_read(0x04));
    uint8_t day = bcd2bin(cmos_read(0x07));
    uint8_t mon = bcd2bin(cmos_read(0x08));
    uint8_t yr  = bcd2bin(cmos_read(0x09));
    kprintf("20%02u-%02u-%02u %02u:%02u:%02u UTC\n", yr, mon, day, hr, min, sec);
}

/* ----------------------- prompt / line editing ----------------------- */
static void print_prompt(void) {
    char path[256];
    fs_abspath(cwd, path, sizeof(path));
    console_set_color(theme_accent(), COL_BLACK);
    kprintf("%s", config_get("shell.prompt", "m1ke@m1keOS"));
    console_set_color(COL_WHITE, COL_BLACK);
    kprintf(":");
    console_set_color(COL_BLUE, COL_BLACK);
    kprintf("%s", path);
    console_set_color(COL_WHITE, COL_BLACK);
    kprintf("$ ");
}

static void erase_line(int n) {
    for (int i = 0; i < n; i++) kprintf("\b \b");
}

/* read a line, with history navigation. returns length. */
static int read_line(char *buf) {
    int len = 0;
    buf[0] = 0;
    hist_pos = hist_count;
    for (;;) {
        int c = input_getchar();
        if (c == '\n') { kputc('\n'); buf[len] = 0; return len; }
        else if (c == '\b') {
            if (len > 0) { len--; buf[len] = 0; kprintf("\b \b"); }
        } else if (c == KEY_UP) {
            if (hist_pos > 0) {
                hist_pos--;
                erase_line(len);
                strcpy(buf, hist[hist_pos]);
                len = (int)strlen(buf);
                kprintf("%s", buf);
            }
        } else if (c == KEY_DOWN) {
            if (hist_pos < hist_count) {
                hist_pos++;
                erase_line(len);
                if (hist_pos == hist_count) { buf[0] = 0; len = 0; }
                else { strcpy(buf, hist[hist_pos]); len = (int)strlen(buf); kprintf("%s", buf); }
            }
        } else if (c >= 32 && c < 256 && c != 127) {
            if (len < LINE_MAX - 1) { buf[len++] = (char)c; buf[len] = 0; kputc((char)c); }
        }
    }
}

static void history_add(const char *line) {
    if (!line[0]) return;
    if (hist_count > 0 && strcmp(hist[hist_count - 1], line) == 0) return;
    if (hist_count == HIST_MAX) {
        for (int i = 1; i < HIST_MAX; i++) strcpy(hist[i - 1], hist[i]);
        hist_count--;
    }
    strncpy(hist[hist_count++], line, LINE_MAX - 1);
}

/* ----------------------- argv tokenizer ----------------------- */
static int tokenize(char *line, char **argv) {
    int argc = 0;
    char *p = line;
    while (*p && argc < ARG_MAX) {
        while (*p == ' ' || *p == '\t') *p++ = 0;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    return argc;
}

/* ----------------------- builtins ----------------------- */
static const char *LOGO[] = {
    "    ___ ___  _ _  ___ ___  ___",
    "   |   |_  || | || __/ _ \\/ __|",
    "   | | | | || | || _| (_) \\__ \\",
    "   |_|_|_|_||___||___\\___/|___/",
};

static void cmd_neofetch(void) {
    char path[128];
    fs_node_t *db = fs_resolve(fs_root(), "/var/lib/m1pkg");
    int pkgs = 0;
    if (db) for (fs_node_t *c = db->children; c; c = c->next) pkgs++;
    (void)path;

    console_set_color(COL_ORANGE, COL_BLACK);
    for (unsigned i = 0; i < sizeof(LOGO)/sizeof(LOGO[0]); i++) kprintf("%s\n", LOGO[i]);
    console_set_color(COL_WHITE, COL_BLACK);
    kprintf("\n");
    kprintf("  user@host : m1ke@m1keOS\n");
    kprintf("  os        : m1keOS 0.5 \"Naranja\" (i686)\n");
    kprintf("  kernel    : m1ke-hybrid 32-bit\n");
    kprintf("  uptime    : %us\n", (unsigned)timer_seconds());
    kprintf("  display   : %s\n", console_is_graphical() ? "framebuffer 1024x768x32" : "VGA text 80x25");
    kprintf("  memory    : %u KB / %u KB used\n",
            (unsigned)(heap_used()/1024), (unsigned)(heap_total()/1024));
    kprintf("  packages  : %d (m1pkg)\n", pkgs);
    kprintf("  theme     : ");
    console_set_color(COL_ORANGE, COL_BLACK); kprintf("orange");
    console_set_color(COL_WHITE, COL_BLACK);  kprintf(" on ");
    console_set_color(COL_GRAY, COL_BLACK);   kprintf("black\n");
    console_set_color(COL_WHITE, COL_BLACK);
}

static void cmd_cowsay(int argc, char **argv) {
    char msg[200] = "Moo!";
    if (argc > 1) {
        msg[0] = 0;
        for (int i = 1; i < argc; i++) {
            strcat(msg, argv[i]);
            if (i < argc - 1) strcat(msg, " ");
        }
    }
    int n = (int)strlen(msg);
    kprintf("  ");
    for (int i = 0; i < n + 2; i++) kprintf("_");
    kprintf("\n < %s >\n  ", msg);
    for (int i = 0; i < n + 2; i++) kprintf("-");
    kprintf("\n        \\   ^__^\n         \\  (oo)\\_______\n            (__)\\       )\\/\\\n                ||----w |\n                ||     ||\n");
}

static const char *FORTUNES[] = {
    "The best way to predict the future is to compile it.",
    "rm -rf is forever. Backups are temporary.",
    "There is no place like 127.0.0.1",
    "A kernel a day keeps the triple fault away.",
    "Real hackers count from zero.",
    "Segfaults build character.",
};
static void cmd_fortune(void) {
    int i = (int)(timer_ticks() % (sizeof(FORTUNES)/sizeof(FORTUNES[0])));
    kprintf("%s\n", FORTUNES[i]);
}

static void list_dir(fs_node_t *dir, bool longfmt) {
    if (!dir) { kprintf("ls: not found\n"); return; }
    if (dir->type == FS_FILE) { kprintf("%s\n", dir->name); return; }
    for (fs_node_t *c = dir->children; c; c = c->next) {
        if (longfmt) {
            kprintf("%s %6u  ", c->type == FS_DIR ? "d" : "-", (unsigned)c->size);
        }
        if (c->type == FS_DIR) console_set_color(COL_BLUE, COL_BLACK);
        else console_set_color(COL_WHITE, COL_BLACK);
        kprintf("%s%s", c->name, c->type == FS_DIR ? "/" : "");
        console_set_color(COL_WHITE, COL_BLACK);
        kprintf(longfmt ? "\n" : "  ");
    }
    if (!longfmt) kprintf("\n");
}

static void tree_rec(fs_node_t *n, int depth) {
    for (fs_node_t *c = n->children; c; c = c->next) {
        for (int i = 0; i < depth; i++) kprintf("  ");
        if (c->type == FS_DIR) {
            console_set_color(COL_BLUE, COL_BLACK);
            kprintf("%s/\n", c->name);
            console_set_color(COL_WHITE, COL_BLACK);
            tree_rec(c, depth + 1);
        } else {
            kprintf("%s\n", c->name);
        }
    }
}

static void show_help(void) {
    console_set_color(COL_ORANGE, COL_BLACK);
    kprintf("m1keOS shell - available commands:\n");
    console_set_color(COL_WHITE, COL_BLACK);
    kprintf("  files : ls  cd  pwd  mkdir  touch  cat  write  rm  rmdir  tree\n");
    kprintf("  text  : echo  clear  history\n");
    kprintf("  system: mem  free  uptime  uname  whoami  date  ps  reboot  halt\n");
    kprintf("  pkg   : m1pkg [list|search|info|install|remove|update]\n");
    kprintf("  ctl   : m1kectl ...   control plane (inspect/config/theme/module)\n");
    kprintf("          dmesg         show the system event log\n");
    kprintf("  fun   : neofetch  cowsay  fortune\n");
    kprintf("  gui   : gui          launch the graphical desktop\n");
    kprintf("  help  : help         show this message\n");
    kprintf("Tip: 'm1kectl help' controls the whole system from the terminal.\n");
}

/* try to run an installed program from /bin */
static bool run_program(const char *name, int argc, char **argv) {
    char path[128];
    ksnprintf(path, sizeof(path), "/bin/%s", name);
    fs_node_t *f = fs_resolve(fs_root(), path);
    if (!f || f->type != FS_FILE) return false;

    if (strcmp(name, "cowsay") == 0) { cmd_cowsay(argc, argv); return true; }
    if (strcmp(name, "neofetch") == 0) { cmd_neofetch(); return true; }

    kprintf("[running %s]\n", path);
    if (f->data) kprintf("%s", f->data);
    return true;
}

static void ps_print(const task_t *t) {
    kprintf("%5u  %-6s %4u  %s\n", t->pid, task_state_name(t->state),
            (unsigned)t->cpu_ticks, t->name);
}

static void execute(int argc, char **argv) {
    if (argc == 0) return;
    const char *cmd = argv[0];

    if (!strcmp(cmd, "help")) show_help();
    else if (!strcmp(cmd, "clear")) console_clear();
    else if (!strcmp(cmd, "echo")) {
        for (int i = 1; i < argc; i++) kprintf("%s%s", argv[i], i < argc - 1 ? " " : "");
        kprintf("\n");
    }
    else if (!strcmp(cmd, "pwd")) {
        char p[256]; fs_abspath(cwd, p, sizeof(p)); kprintf("%s\n", p);
    }
    else if (!strcmp(cmd, "cd")) {
        if (argc < 2) { cwd = fs_root(); return; }
        fs_node_t *t = fs_resolve(cwd, argv[1]);
        if (!t) kprintf("cd: %s: no such directory\n", argv[1]);
        else if (t->type != FS_DIR) kprintf("cd: %s: not a directory\n", argv[1]);
        else cwd = t;
    }
    else if (!strcmp(cmd, "ls")) {
        bool lng = false; const char *target = 0;
        for (int i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "-l")) lng = true; else target = argv[i];
        }
        fs_node_t *d = target ? fs_resolve(cwd, target) : cwd;
        list_dir(d, lng);
    }
    else if (!strcmp(cmd, "tree")) {
        fs_node_t *d = (argc > 1) ? fs_resolve(cwd, argv[1]) : cwd;
        if (d) tree_rec(d, 0); else kprintf("tree: not found\n");
    }
    else if (!strcmp(cmd, "mkdir")) {
        if (argc < 2) { kprintf("usage: mkdir <dir>\n"); return; }
        for (int i = 1; i < argc; i++) fs_create(cwd, argv[i], FS_DIR);
    }
    else if (!strcmp(cmd, "touch")) {
        if (argc < 2) { kprintf("usage: touch <file>\n"); return; }
        for (int i = 1; i < argc; i++) fs_create(cwd, argv[i], FS_FILE);
    }
    else if (!strcmp(cmd, "cat")) {
        if (argc < 2) { kprintf("usage: cat <file>\n"); return; }
        for (int i = 1; i < argc; i++) {
            fs_node_t *f = fs_resolve(cwd, argv[i]);
            if (!f) kprintf("cat: %s: not found\n", argv[i]);
            else if (f->type != FS_FILE) kprintf("cat: %s: is a directory\n", argv[i]);
            else if (f->data) kprintf("%s", f->data);
        }
    }
    else if (!strcmp(cmd, "write")) {
        if (argc < 3) { kprintf("usage: write <file> <text...>\n"); return; }
        fs_node_t *f = fs_resolve(cwd, argv[1]);
        if (!f) f = fs_create(cwd, argv[1], FS_FILE);
        if (!f || f->type != FS_FILE) { kprintf("write: cannot write %s\n", argv[1]); return; }
        char text[LINE_MAX]; text[0] = 0;
        for (int i = 2; i < argc; i++) { strcat(text, argv[i]); if (i < argc-1) strcat(text, " "); }
        strcat(text, "\n");
        fs_write(f, text, strlen(text));
    }
    else if (!strcmp(cmd, "rm") || !strcmp(cmd, "rmdir")) {
        if (argc < 2) { kprintf("usage: %s <name>\n", cmd); return; }
        for (int i = 1; i < argc; i++) {
            fs_node_t *f = fs_resolve(cwd, argv[i]);
            if (!f) { kprintf("%s: %s: not found\n", cmd, argv[i]); continue; }
            if (!fs_remove(f)) kprintf("%s: cannot remove %s\n", cmd, argv[i]);
        }
    }
    else if (!strcmp(cmd, "mem") || !strcmp(cmd, "free")) {
        kprintf("heap: %u KB used, %u KB free, %u KB total\n",
                (unsigned)(heap_used()/1024), (unsigned)(heap_free()/1024),
                (unsigned)(heap_total()/1024));
    }
    else if (!strcmp(cmd, "uptime")) {
        uint64_t s = timer_seconds();
        kprintf("up %u min %u sec (%u ticks @ %u Hz)\n",
                (unsigned)(s/60), (unsigned)(s%60),
                (unsigned)timer_ticks(), (unsigned)timer_freq());
    }
    else if (!strcmp(cmd, "uname")) kprintf("m1keOS 0.7 Naranja i686 m1ke-hybrid\n");
    else if (!strcmp(cmd, "whoami")) kprintf("m1ke\n");
    else if (!strcmp(cmd, "date")) print_datetime();
    else if (!strcmp(cmd, "ps")) {
        kprintf("  PID  STATE   CPU  NAME\n");
        task_foreach(ps_print);
        kprintf("%d task(s)\n", task_count());
    }
    else if (!strcmp(cmd, "history")) {
        for (int i = 0; i < hist_count; i++) kprintf("%4d  %s\n", i + 1, hist[i]);
    }
    else if (!strcmp(cmd, "neofetch")) cmd_neofetch();
    else if (!strcmp(cmd, "cowsay")) cmd_cowsay(argc, argv);
    else if (!strcmp(cmd, "fortune")) cmd_fortune();
    else if (!strcmp(cmd, "sleep")) { if (argc > 1) timer_sleep_ms((uint32_t)atoi(argv[1]) * 1000); }
    else if (!strcmp(cmd, "m1pkg")) pkg_command(argc - 1, argv + 1);
    else if (!strcmp(cmd, "m1kectl") || !strcmp(cmd, "ctl")) control_command(argc - 1, argv + 1);
    else if (!strcmp(cmd, "dmesg")) events_dump(argc > 1 ? atoi(argv[1]) : 0);
    else if (!strcmp(cmd, "gui")) {
        if (console_is_graphical()) desktop_run();
        else kprintf("gui: no framebuffer available (text mode)\n");
    }
    else if (!strcmp(cmd, "reboot")) {
        kprintf("Rebooting...\n");
        uint8_t good = 0x02;
        while (good & 0x02) good = inb(0x64);
        outb(0x64, 0xFE);          /* pulse reset line via 8042 */
        for (;;) __asm__ __volatile__("hlt");
    }
    else if (!strcmp(cmd, "halt")) {
        kprintf("System halted. You may power off.\n");
        for (;;) __asm__ __volatile__("cli; hlt");
    }
    else if (!run_program(cmd, argc, argv)) {
        kprintf("m1sh: command not found: %s\n", cmd);
        kprintf("      type 'help' for a list of commands.\n");
    }
}

/* run a single command line (used by the GUI terminal app) */
void shell_exec_line(char *line) {
    char *argv[ARG_MAX];
    int argc = tokenize(line, argv);
    execute(argc, argv);
}

/* ----------------------- public ----------------------- */
void shell_init(void) {
    cwd = fs_root();
    hist_count = hist_pos = 0;

    /* seed the filesystem with a friendly home */
    fs_mkdir_p("/home/m1ke");
    fs_mkdir_p("/etc");
    fs_mkdir_p("/tmp");
    fs_put("/etc/motd", "Welcome to m1keOS - the system that serves YOU.\n");
    fs_put("/home/m1ke/espanol.txt",
           "Teclado espa\xA4ol (ES) activo con: m1kectl keyboard layout es\n\n"
           "La e\xA4""e:  \xA4 \xA5\n"
           "Acentos:  \xA0 \x82 \xA1 \xA2 \xA3   di\x82resis: \x81\n"
           "Signos:   \xAD Hola !  \xA8 Qu\x82 tal ?   \xA7 \xA6  \xAA\n"
           "\xA1M1keOS habla espa\xA4ol!\n");
    fs_put("/home/m1ke/readme.txt",
           "Hi m1ke! This is your home (in memory for now).\n\n"
           "Friendly start:  help        - see what you can do\n"
           "                 gui         - open the graphical desktop\n"
           "                 neofetch    - show off your system\n\n"
           "Full power:      m1kectl help     - control EVERYTHING\n"
           "                 m1kectl theme edit  - restyle the UI (m1ss language)\n"
           "                 dmesg            - watch the system live\n\n"
           "Everything here is yours to change. Have fun!\n");
    cwd = fs_resolve(fs_root(), "/home/m1ke");
}

void shell_run(void) {
    char line[LINE_MAX];
    char *argv[ARG_MAX];

    fs_node_t *motd = fs_resolve(fs_root(), "/etc/motd");
    if (motd && motd->data) {
        console_set_color(COL_GREEN, COL_BLACK);
        kprintf("%s", motd->data);
        console_set_color(COL_WHITE, COL_BLACK);
    }
    console_set_color(COL_WHITE, COL_BLACK);
    kprintf("New here? type ");
    console_set_color(theme_accent(), COL_BLACK); kprintf("help");
    console_set_color(COL_WHITE, COL_BLACK); kprintf(".  Want full control? type ");
    console_set_color(theme_accent(), COL_BLACK); kprintf("m1kectl help");
    console_set_color(COL_WHITE, COL_BLACK); kprintf(".\n\n");

    for (;;) {
        print_prompt();
        read_line(line);
        history_add(line);
        char copy[LINE_MAX];
        strncpy(copy, line, LINE_MAX);
        int argc = tokenize(copy, argv);
        execute(argc, argv);
    }
}
