/* config.c - text key/value store, persisted to /etc/m1ke/system.conf */
#include "config.h"
#include "events.h"
#include "../fs/ramfs.h"
#include "../lib/printf.h"
#include "../lib/string.h"

#define CFG_MAX 96
#define KEYLEN  48
#define VALLEN  80

typedef struct { char key[KEYLEN]; char val[VALLEN]; bool used; } entry_t;
static entry_t cfg[CFG_MAX];

static entry_t *find(const char *key) {
    for (int i = 0; i < CFG_MAX; i++)
        if (cfg[i].used && strcmp(cfg[i].key, key) == 0) return &cfg[i];
    return 0;
}

const char *config_get(const char *key, const char *def) {
    entry_t *e = find(key);
    return e ? e->val : def;
}

int config_get_int(const char *key, int def) {
    entry_t *e = find(key);
    return e ? atoi(e->val) : def;
}

bool config_set(const char *key, const char *val) {
    entry_t *e = find(key);
    if (!e) {
        for (int i = 0; i < CFG_MAX; i++) if (!cfg[i].used) { e = &cfg[i]; break; }
        if (!e) return false;
        e->used = true;
        strncpy(e->key, key, KEYLEN - 1);
        e->key[KEYLEN - 1] = 0;
    }
    strncpy(e->val, val, VALLEN - 1);
    e->val[VALLEN - 1] = 0;
    return true;
}

bool config_set_int(const char *key, int val) {
    char b[16];
    itoa(val, b, 10);
    return config_set(key, b);
}

int config_count(void) {
    int n = 0;
    for (int i = 0; i < CFG_MAX; i++) if (cfg[i].used) n++;
    return n;
}

static void set_default(const char *k, const char *v) {
    if (!find(k)) config_set(k, v);
}

/* parse lines "key = value", '#' comments, blanks ignored */
static void parse(const char *text) {
    char line[KEYLEN + VALLEN + 4];
    int li = 0;
    for (const char *p = text; ; p++) {
        if (*p == '\n' || *p == 0) {
            line[li] = 0;
            /* trim leading spaces */
            char *s = line; while (*s == ' ' || *s == '\t') s++;
            if (*s && *s != '#') {
                char *eq = strchr(s, '=');
                if (eq) {
                    *eq = 0;
                    char *k = s, *v = eq + 1;
                    /* trim trailing spaces on key */
                    char *ke = k + strlen(k); while (ke > k && (ke[-1]==' '||ke[-1]=='\t')) *--ke = 0;
                    while (*v == ' ' || *v == '\t') v++;
                    char *ve = v + strlen(v); while (ve > v && (ve[-1]==' '||ve[-1]=='\t'||ve[-1]=='\r')) *--ve = 0;
                    if (*k) config_set(k, v);
                }
            }
            li = 0;
            if (*p == 0) break;
        } else if (li < (int)sizeof(line) - 1) {
            line[li++] = *p;
        }
    }
}

void config_save(void) {
    char buf[CFG_MAX * (KEYLEN + VALLEN + 4)];
    int n = 0;
    n += ksnprintf(buf + n, sizeof(buf) - n, "# m1keOS system configuration\n# editable text - versionable with git\n\n");
    for (int i = 0; i < CFG_MAX; i++)
        if (cfg[i].used)
            n += ksnprintf(buf + n, sizeof(buf) - n, "%s = %s\n", cfg[i].key, cfg[i].val);
    fs_mkdir_p("/etc/m1ke");
    fs_put(CONFIG_PATH, buf);
}

void config_dump(const char *prefix) {
    for (int i = 0; i < CFG_MAX; i++) {
        if (!cfg[i].used) continue;
        if (prefix && strncmp(cfg[i].key, prefix, strlen(prefix)) != 0) continue;
        kprintf("  %-26s = %s\n", cfg[i].key, cfg[i].val);
    }
}

void config_init(void) {
    /* baseline defaults (only if not already present from disk) */
    set_default("ui.accent",         "orange");
    set_default("wm.wallpaper",      "grid");
    set_default("wm.border_radius",  "8");
    set_default("wm.titlebar_height","22");
    set_default("shell.prompt",      "m1ke@m1keOS");
    set_default("input.layout",      "us");
    set_default("scheduler.policy",  "round-robin");
    set_default("scheduler.quantum", "2");
    set_default("net.enabled",       "false");
    set_default("net.hostname",      "m1keos");

    /* load overrides from the text file if it exists */
    fs_node_t *f = fs_resolve(fs_root(), CONFIG_PATH);
    if (f && f->type == FS_FILE && f->data) {
        parse(f->data);
        event_log(EV_OK, "config loaded from %s", CONFIG_PATH);
    } else {
        config_save();   /* materialize defaults */
        event_log(EV_INFO, "config defaults written to %s", CONFIG_PATH);
    }
}
