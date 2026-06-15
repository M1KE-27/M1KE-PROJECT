/* m1ss.c - parser + model for m1ke Style Sheets (our own CSS-like language).
 * Grammar (tolerant): selector { prop : value ; ... }
 * Supports C-style, line, and hash comments inside the stylesheet. */
#include "m1ss.h"
#include "../fs/ramfs.h"
#include "../drivers/console.h"
#include "../lib/printf.h"
#include "../lib/string.h"

static theme_t g;

/* ---------- named colors ---------- */
static const struct { const char *name; uint32_t col; } named[] = {
    { "orange", 0x00FF8C1A }, { "rose", 0x00FF4D6D }, { "green", 0x0046E07A },
    { "blue", 0x005AA8FF }, { "purple", 0x00B98CFF }, { "gold", 0x00FFD24A },
    { "white", 0x00EAEAEA }, { "black", 0x00000000 }, { "gray", 0x00909090 },
};
#define N_NAMED ((int)(sizeof(named)/sizeof(named[0])))

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool m1ss_parse_color(const char *s, uint32_t *out) {
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '#') {
        s++;
        uint32_t v = 0; int n = 0;
        while (n < 6 && hexval(*s) >= 0) { v = (v << 4) | (uint32_t)hexval(*s); s++; n++; }
        if (n == 6) { *out = v & 0x00FFFFFF; return true; }
        return false;
    }
    for (int i = 0; i < N_NAMED; i++)
        if (strcmp(named[i].name, s) == 0) { *out = named[i].col; return true; }
    return false;
}

static bool parse_bool(const char *s) {
    return !strcmp(s, "true") || !strcmp(s, "1") || !strcmp(s, "on") || !strcmp(s, "yes");
}
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void theme_defaults(theme_t *t) {
    t->accent      = 0x00FF8C1A;
    t->wallpaper   = 0;            /* aurora */
    t->glow1       = 0x00FF8C1A;
    t->glow2       = 0x006A4CFF;
    t->win_radius  = 14;
    t->win_blur    = 6;
    t->win_opacity = 58;
    t->win_shadow  = 16;
    t->win_border  = 1;
    t->win_tint    = 0x0012121A;
    t->bar_visible = true;
    t->bar_pos     = 0;            /* bottom */
    t->bar_height  = 34;
    t->bar_opacity = 70;
    t->term_bg     = 0x000E0E16;
    t->term_fg     = 0x00EAEAEA;
}

static int wallpaper_id(const char *v) {
    if (!strcmp(v, "gradient")) return 1;
    if (!strcmp(v, "grid"))     return 2;
    if (!strcmp(v, "solid"))    return 3;
    return 0; /* aurora */
}

/* apply one "selector.prop = value" into the theme */
static void apply(const char *sel, const char *prop, const char *val) {
    uint32_t c;
    char key[48];
    ksnprintf(key, sizeof(key), "%s.%s", sel, prop);

    if      (!strcmp(key, "desktop.accent")    && m1ss_parse_color(val, &c)) g.accent = c;
    else if (!strcmp(key, "desktop.wallpaper")) g.wallpaper = wallpaper_id(val);
    else if (!strcmp(key, "desktop.glow1")     && m1ss_parse_color(val, &c)) g.glow1 = c;
    else if (!strcmp(key, "desktop.glow2")     && m1ss_parse_color(val, &c)) g.glow2 = c;
    else if (!strcmp(key, "window.radius"))     g.win_radius  = clampi(atoi(val), 0, 40);
    else if (!strcmp(key, "window.blur"))       g.win_blur    = clampi(atoi(val), 0, 12);
    else if (!strcmp(key, "window.opacity"))    g.win_opacity = clampi(atoi(val), 0, 100);
    else if (!strcmp(key, "window.shadow"))     g.win_shadow  = clampi(atoi(val), 0, 40);
    else if (!strcmp(key, "window.border"))     g.win_border  = clampi(atoi(val), 0, 1);
    else if (!strcmp(key, "window.tint")       && m1ss_parse_color(val, &c)) g.win_tint = c;
    else if (!strcmp(key, "taskbar.visible"))   g.bar_visible = parse_bool(val);
    else if (!strcmp(key, "taskbar.position"))  g.bar_pos     = !strcmp(val, "top") ? 1 : 0;
    else if (!strcmp(key, "taskbar.height"))    g.bar_height  = clampi(atoi(val), 18, 64);
    else if (!strcmp(key, "taskbar.opacity"))   g.bar_opacity = clampi(atoi(val), 0, 100);
    else if (!strcmp(key, "terminal.background")&& m1ss_parse_color(val, &c)) g.term_bg = c;
    else if (!strcmp(key, "terminal.foreground")&& m1ss_parse_color(val, &c)) g.term_fg = c;
}

/* ---------- tokenizer ---------- */
static const char *skip_ws(const char *p) {
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (p[0] == '/' && p[1] == '*') { p += 2; while (*p && !(p[0]=='*'&&p[1]=='/')) p++; if (*p) p += 2; continue; }
        if ((p[0] == '/' && p[1] == '/') || p[0] == '#') { while (*p && *p != '\n') p++; continue; }
        break;
    }
    return p;
}
static const char *read_token(const char *p, char *out, int cap, const char *stops) {
    int n = 0;
    while (*p && !strchr(stops, *p) && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
        if (n < cap - 1) out[n++] = *p;
        p++;
    }
    out[n] = 0;
    return p;
}

void m1ss_parse(const char *text) {
    const char *p = text;
    char sel[32], prop[32], val[48];
    while (*p) {
        p = skip_ws(p);
        if (!*p) break;
        p = read_token(p, sel, sizeof(sel), "{");
        p = skip_ws(p);
        if (*p != '{') { if (*p) p++; continue; }
        p++; /* consume { */
        for (;;) {
            p = skip_ws(p);
            if (*p == '}' || !*p) { if (*p) p++; break; }
            p = read_token(p, prop, sizeof(prop), ":}");
            p = skip_ws(p);
            if (*p != ':') { while (*p && *p != ';' && *p != '}') p++; if (*p==';') p++; continue; }
            p++; /* consume : */
            p = skip_ws(p);
            /* value: up to ; or } */
            int n = 0;
            while (*p && *p != ';' && *p != '}') { if (n < (int)sizeof(val)-1) val[n++] = *p; p++; }
            val[n] = 0;
            /* trim trailing ws */
            while (n > 0 && (val[n-1]==' '||val[n-1]=='\t'||val[n-1]=='\r')) val[--n] = 0;
            if (*p == ';') p++;
            if (prop[0]) apply(sel, prop, val);
        }
    }
}

/* ---------- public ---------- */
theme_t *theme_get(void) { return &g; }

static void apply_to_console(void) { theme_set_accent(g.accent); }

void theme_save(void) {
    char buf[1400];
    char a[8], g1[8], g2[8], tnt[8], tb[8], tf[8];
    ksnprintf(a,8,"%06x", g.accent); ksnprintf(g1,8,"%06x", g.glow1);
    ksnprintf(g2,8,"%06x", g.glow2); ksnprintf(tnt,8,"%06x", g.win_tint);
    ksnprintf(tb,8,"%06x", g.term_bg); ksnprintf(tf,8,"%06x", g.term_fg);
    const char *wp = g.wallpaper==1?"gradient":g.wallpaper==2?"grid":g.wallpaper==3?"solid":"aurora";
    ksnprintf(buf, sizeof(buf),
        "/* m1ss - m1keOS Style Sheet\n"
        " * Edit me, then run:  m1kectl theme reload\n"
        " * Restyles the live desktop. The system serves you. */\n\n"
        "desktop {\n"
        "  accent:    #%s;     /* orange|rose|green|blue|purple|gold|#hex */\n"
        "  wallpaper: %s;      /* aurora|gradient|grid|solid */\n"
        "  glow1:     #%s;\n"
        "  glow2:     #%s;\n"
        "}\n\n"
        "window {\n"
        "  radius:  %d;\n"
        "  blur:    %d;\n"
        "  opacity: %d;        /* frosted-glass strength 0..100 */\n"
        "  shadow:  %d;\n"
        "  border:  %d;\n"
        "  tint:    #%s;\n"
        "}\n\n"
        "taskbar {\n"
        "  visible:  %s;\n"
        "  position: %s;       /* bottom|top */\n"
        "  height:   %d;\n"
        "  opacity:  %d;\n"
        "}\n\n"
        "terminal {\n"
        "  background: #%s;\n"
        "  foreground: #%s;\n"
        "}\n",
        a, wp, g1, g2,
        g.win_radius, g.win_blur, g.win_opacity, g.win_shadow, g.win_border, tnt,
        g.bar_visible ? "true" : "false", g.bar_pos ? "top" : "bottom",
        g.bar_height, g.bar_opacity, tb, tf);
    fs_mkdir_p("/etc/m1ke");
    fs_put(THEME_PATH, buf);
}

void theme_init(void) {
    theme_defaults(&g);
    fs_node_t *f = fs_resolve(fs_root(), THEME_PATH);
    if (f && f->type == FS_FILE && f->data) m1ss_parse(f->data);
    else theme_save();
    apply_to_console();
}

void theme_reload(void) {
    theme_defaults(&g);
    fs_node_t *f = fs_resolve(fs_root(), THEME_PATH);
    if (f && f->type == FS_FILE && f->data) m1ss_parse(f->data);
    apply_to_console();
}

void theme_show(void) {
    fs_node_t *f = fs_resolve(fs_root(), THEME_PATH);
    if (f && f->data) kprintf("%s", f->data);
}

bool theme_set(const char *prop, const char *val) {
    /* allow "accent" as shorthand for "desktop.accent" */
    if (!strchr(prop, '.')) {
        if (!strcmp(prop, "accent") || !strcmp(prop, "wallpaper") ||
            !strcmp(prop, "glow1") || !strcmp(prop, "glow2"))
            apply("desktop", prop, val);
        else if (!strcmp(prop,"radius")||!strcmp(prop,"blur")||!strcmp(prop,"opacity")||
                 !strcmp(prop,"shadow")||!strcmp(prop,"border")||!strcmp(prop,"tint"))
            apply("window", prop, val);
        else return false;
    } else {
        char sel[32], pr[32];
        const char *dot = strchr(prop, '.');
        int sl = (int)(dot - prop); if (sl > 31) sl = 31;
        memcpy(sel, prop, sl); sel[sl] = 0;
        strncpy(pr, dot + 1, sizeof(pr) - 1); pr[sizeof(pr)-1] = 0;
        apply(sel, pr, val);
    }
    theme_save();
    apply_to_console();
    return true;
}
