/* pkg.c - m1pkg: a small package manager with a built-in repository.
 * "Installing" a package materializes its files into the ramfs and records
 * it under /var/lib/m1pkg/<name>. */
#include "pkg.h"
#include "../fs/ramfs.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include "../drivers/console.h"

typedef struct {
    const char *path;
    const char *contents;
} pkg_file_t;

typedef struct {
    const char *name;
    const char *version;
    const char *category;
    const char *summary;
    int         kb;           /* fake download size */
    pkg_file_t  files[4];      /* up to 4 files, terminated by {0,0} */
} package_t;

static const package_t repo[] = {
    { "coreutils", "1.2", "base", "Essential m1keOS file & text utilities", 320, {
        {"/bin/coreutils", "#!m1sh\n# coreutils bundle: ls cat echo cp mv rm\n"},
        {"/usr/share/doc/coreutils/README", "coreutils provides the basic commands.\n"},
        {0,0} } },
    { "m1edit", "0.9", "editor", "Minimalist orange/black text editor", 180, {
        {"/bin/m1edit", "#!m1sh\n# m1edit - tiny editor. usage: m1edit <file>\n"},
        {"/usr/share/doc/m1edit/README", "m1edit: arrows to move, type to insert, Esc to save.\n"},
        {0,0} } },
    { "cowsay", "3.0", "fun", "An ASCII cow that says things", 24, {
        {"/bin/cowsay", " _____\n< moo >\n -----\n     \\\n      \\\n       ^__^\n"},
        {0,0} } },
    { "fortune", "1.0", "fun", "Random hacker wisdom cookies", 40, {
        {"/usr/share/fortune/hacker", "There are 10 kinds of people: those who get binary and those who don't.\n"},
        {0,0} } },
    { "snake", "1.1", "games", "Classic terminal snake game", 96, {
        {"/bin/snake", "#!m1sh\n# snake game - eat the @, avoid the walls!\n"},
        {0,0} } },
    { "doom-ascii", "0.3", "games", "DOOM, but rendered in ASCII", 666, {
        {"/bin/doom", "#!m1sh\n# RIP AND TEAR (in ASCII)\n"},
        {"/usr/share/doom/E1M1.txt", "######\n# @  #\n#  i #\n######\n"},
        {0,0} } },
    { "neofetch", "2.0", "system", "Show off your m1keOS system info", 60, {
        {"/bin/neofetch", "#!m1sh\n# system info fetcher with the m1keOS logo\n"},
        {0,0} } },
    { "m1gcc", "11.0", "devel", "The m1keOS C compiler toolchain (stub)", 4096, {
        {"/bin/m1gcc", "#!m1sh\n# m1gcc - someday this will self-host m1keOS :)\n"},
        {"/usr/share/doc/m1gcc/README", "A real cross-compiler would live here.\n"},
        {0,0} } },
    { "m1net", "0.1", "net", "Experimental networking stack & tools", 512, {
        {"/bin/ping", "#!m1sh\n# ping - pretends to reach the internet\n"},
        {0,0} } },
    { "htop-m1", "1.0", "system", "Interactive process viewer", 140, {
        {"/bin/htop", "#!m1sh\n# htop - watch your (one) process work hard\n"},
        {0,0} } },
};
#define REPO_COUNT ((int)(sizeof(repo) / sizeof(repo[0])))

static const char *DB_DIR = "/var/lib/m1pkg";

void pkg_init(void) {
    fs_mkdir_p(DB_DIR);
    fs_mkdir_p("/bin");
    fs_mkdir_p("/usr/share");
}

static const package_t *find_pkg(const char *name) {
    for (int i = 0; i < REPO_COUNT; i++)
        if (strcmp(repo[i].name, name) == 0) return &repo[i];
    return 0;
}

static bool is_installed(const char *name) {
    char path[128];
    ksnprintf(path, sizeof(path), "%s/%s", DB_DIR, name);
    return fs_resolve(fs_root(), path) != 0;
}

static void progress_bar(int kb) {
    const int width = 24;
    kprintf("  [");
    for (int i = 0; i <= width; i++) {
        kprintf("#");
    }
    kprintf("] %d KB\n", kb);
}

static void do_install(const char *name) {
    const package_t *p = find_pkg(name);
    if (!p) { kprintf("m1pkg: package '%s' not found in repository\n", name); return; }
    if (is_installed(name)) { kprintf("m1pkg: '%s' is already installed\n", name); return; }

    console_set_color(COL_GREEN, COL_BLACK);
    kprintf(":: Resolving dependencies for %s-%s...\n", p->name, p->version);
    console_set_color(COL_WHITE, COL_BLACK);
    kprintf(":: Downloading %s (%d KB) from repo.m1ke.os ...\n", p->name, p->kb);
    progress_bar(p->kb);

    int nfiles = 0;
    for (int i = 0; i < 4 && p->files[i].path; i++) {
        fs_put(p->files[i].path, p->files[i].contents);
        nfiles++;
    }

    char dbpath[128], rec[96];
    ksnprintf(dbpath, sizeof(dbpath), "%s/%s", DB_DIR, p->name);
    ksnprintf(rec, sizeof(rec), "%s %s\n", p->version, p->category);
    fs_put(dbpath, rec);

    console_set_color(COL_ORANGE, COL_BLACK);
    kprintf(":: Installed %s-%s (%d files). \n", p->name, p->version, nfiles);
    console_set_color(COL_WHITE, COL_BLACK);
}

static void do_remove(const char *name) {
    if (!is_installed(name)) { kprintf("m1pkg: '%s' is not installed\n", name); return; }
    const package_t *p = find_pkg(name);
    if (p) {
        for (int i = 0; i < 4 && p->files[i].path; i++) {
            fs_node_t *f = fs_resolve(fs_root(), p->files[i].path);
            if (f) fs_remove(f);
        }
    }
    char dbpath[128];
    ksnprintf(dbpath, sizeof(dbpath), "%s/%s", DB_DIR, name);
    fs_node_t *db = fs_resolve(fs_root(), dbpath);
    if (db) fs_remove(db);
    kprintf(":: Removed %s\n", name);
}

static void do_list(void) {
    fs_node_t *dir = fs_resolve(fs_root(), DB_DIR);
    if (!dir || !dir->children) { kprintf("No packages installed.\n"); return; }
    kprintf("Installed packages:\n");
    int n = 0;
    for (fs_node_t *c = dir->children; c; c = c->next) {
        const package_t *p = find_pkg(c->name);
        console_set_color(COL_ORANGE, COL_BLACK);
        kprintf("  %-14s", c->name);
        console_set_color(COL_WHITE, COL_BLACK);
        if (p) kprintf(" %-6s %s", p->version, p->summary);
        kprintf("\n");
        n++;
    }
    kprintf("%d package(s) installed.\n", n);
}

static void do_search(const char *q) {
    kprintf("Available packages%s%s:\n", q ? " matching " : "", q ? q : "");
    for (int i = 0; i < REPO_COUNT; i++) {
        const package_t *p = &repo[i];
        if (q && !strstr(p->name, q) && !strstr(p->category, q)) continue;
        console_set_color(COL_ORANGE, COL_BLACK);
        kprintf("  %-14s", p->name);
        console_set_color(COL_GRAY, COL_BLACK);
        kprintf(" %-6s [%-6s]", p->version, p->category);
        console_set_color(COL_WHITE, COL_BLACK);
        kprintf(" %s%s\n", p->summary, is_installed(p->name) ? "  (installed)" : "");
    }
}

static void do_info(const char *name) {
    const package_t *p = find_pkg(name);
    if (!p) { kprintf("m1pkg: package '%s' not found\n", name); return; }
    kprintf("Name:        %s\n", p->name);
    kprintf("Version:     %s\n", p->version);
    kprintf("Category:    %s\n", p->category);
    kprintf("Size:        %d KB\n", p->kb);
    kprintf("Summary:     %s\n", p->summary);
    kprintf("Status:      %s\n", is_installed(p->name) ? "installed" : "not installed");
    kprintf("Files:\n");
    for (int i = 0; i < 4 && p->files[i].path; i++) kprintf("  %s\n", p->files[i].path);
}

static void usage(void) {
    kprintf("m1pkg - m1keOS package manager\n");
    kprintf("usage:\n");
    kprintf("  m1pkg list                 list installed packages\n");
    kprintf("  m1pkg search [query]       search the repository\n");
    kprintf("  m1pkg info <pkg>           show package details\n");
    kprintf("  m1pkg install <pkg>...     install package(s)\n");
    kprintf("  m1pkg remove <pkg>...      remove package(s)\n");
    kprintf("  m1pkg update               refresh the repository\n");
}

void pkg_command(int argc, char **argv) {
    if (argc < 1) { usage(); return; }
    const char *cmd = argv[0];
    if (strcmp(cmd, "list") == 0) {
        do_list();
    } else if (strcmp(cmd, "search") == 0) {
        do_search(argc > 1 ? argv[1] : 0);
    } else if (strcmp(cmd, "info") == 0 && argc > 1) {
        do_info(argv[1]);
    } else if (strcmp(cmd, "install") == 0 && argc > 1) {
        for (int i = 1; i < argc; i++) do_install(argv[i]);
    } else if (strcmp(cmd, "remove") == 0 && argc > 1) {
        for (int i = 1; i < argc; i++) do_remove(argv[i]);
    } else if (strcmp(cmd, "update") == 0) {
        kprintf(":: Synchronizing package databases...\n");
        kprintf(":: repo.m1ke.os is up to date (%d packages).\n", REPO_COUNT);
    } else {
        usage();
    }
}
