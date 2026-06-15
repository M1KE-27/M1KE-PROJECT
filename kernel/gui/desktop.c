/* desktop.c - m1keOS compositor & desktop (KDE-Plasma-inspired glassmorphism).
 * Everything visual is driven by the m1ss theme (/etc/m1ke/theme.m1ss):
 * window blur/opacity/radius/shadow, taskbar position/visibility, colors.
 * Apps: Terminal (real shell), Editor (edit+save), Files, Monitor, Settings, About.
 * Controls: mouse, Tab cycles windows, ` toggles start menu, Esc -> shell. */
#include "desktop.h"
#include "../drivers/console.h"
#include "../drivers/input.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/timer.h"
#include "../mm/heap.h"
#include "../fs/ramfs.h"
#include "../shell/shell.h"
#include "m1ss.h"
#include "../lib/printf.h"
#include "../lib/string.h"
#include <stdint.h>
#include <stdbool.h>

#define TXT_DIM 0x00A0A0A0
#define FW 8
#define FH 16
#define WTH 28          /* window title bar height */

enum { W_TERM, W_EDIT, W_FILES, W_MON, W_SET, W_ABOUT, W_COUNT };

typedef struct { int x, y, w, h; const char *title; bool open; } window_t;

static window_t wins[W_COUNT];
static int  active;
static bool startmenu;
static bool wall_dirty;

static uint32_t *wallcache;     /* prebuilt wallpaper (fb_w*fb_h) */
static uint32_t *wallblur;      /* pre-blurred wallpaper for fast glass */

/* editor / terminal state */
static char editbuf[4096];
static int  editlen;
static const char *editpath = "/home/m1ke/notes.txt";
static uint64_t saved_until;
static char termout[8192];
static char termin[128];
static int  termlen;

static const struct { uint32_t col; const char *name; } accents[] = {
    { 0x00FF8C1A, "orange" }, { 0x00FF4D6D, "rose" }, { 0x0046E07A, "green" },
    { 0x005AA8FF, "blue" }, { 0x00B98CFF, "purple" }, { 0x00FFD24A, "gold" },
};
#define N_ACC ((int)(sizeof(accents)/sizeof(accents[0])))

/* ---------- cursor ---------- */
static const uint8_t cursor[18][11] = {
    {2,0,0,0,0,0,0,0,0,0,0},{2,2,0,0,0,0,0,0,0,0,0},{2,1,2,0,0,0,0,0,0,0,0},
    {2,1,1,2,0,0,0,0,0,0,0},{2,1,1,1,2,0,0,0,0,0,0},{2,1,1,1,1,2,0,0,0,0,0},
    {2,1,1,1,1,1,2,0,0,0,0},{2,1,1,1,1,1,1,2,0,0,0},{2,1,1,1,1,1,1,1,2,0,0},
    {2,1,1,1,1,1,1,1,1,2,0},{2,1,1,1,1,1,2,2,2,2,2},{2,1,1,2,1,1,2,0,0,0,0},
    {2,1,2,0,2,1,1,2,0,0,0},{2,2,0,0,2,1,1,2,0,0,0},{2,0,0,0,0,2,1,1,2,0,0},
    {0,0,0,0,0,2,1,1,2,0,0},{0,0,0,0,0,0,2,2,2,0,0},{0,0,0,0,0,0,0,0,0,0,0},
};
#define CURSOR_W 11
#define CURSOR_H 18
/* drawn straight to the hardware framebuffer as an overlay, so the back buffer
 * always holds a clean (cursor-free) scene for cheap dirty-rect restores. */
static void draw_cursor(int x, int y) {
    for (int r = 0; r < CURSOR_H; r++)
        for (int c = 0; c < CURSOR_W; c++) {
            uint8_t v = cursor[r][c];
            if (v == 1) fb_hw_putpixel(x + c, y + r, 0x00FFFFFF);
            else if (v == 2) fb_hw_putpixel(x + c, y + r, 0x00101014);
        }
}

static bool in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

/* ---------- wallpaper (cached) ---------- */
static uint32_t addc(uint32_t base, uint32_t col, int a) {
    int r = ((base>>16)&255) + (((col>>16)&255) * a)/255;
    int g = ((base>>8)&255)  + (((col>>8)&255)  * a)/255;
    int b = (base&255)       + ((col&255)       * a)/255;
    if (r>255) r=255;
    if (g>255) g=255;
    if (b>255) b=255;
    return (uint32_t)((r<<16)|(g<<8)|b);
}

static void build_wallpaper(void) {
    if (!wallcache) return;
    theme_t *t = theme_get();
    int W = (int)fb_width(), H = (int)fb_height();
    if (t->wallpaper == 3) {                              /* solid */
        for (int i = 0; i < W*H; i++) wallcache[i] = 0x000C0C12;
    } else if (t->wallpaper == 2) {                       /* grid */
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
            wallcache[y*W+x] = ((x%40==0)||(y%40==0)) ? 0x001c1822 : 0x00121018;
    } else if (t->wallpaper == 1) {                       /* vertical gradient */
        for (int y = 0; y < H; y++) {
            int a = y*180/H;
            uint32_t c = addc(0x000A0A12, t->glow2, a);
            for (int x = 0; x < W; x++) wallcache[y*W+x] = c;
        }
    } else {                                              /* aurora */
        int c1x = W*72/100, c1y = H*20/100, c2x = W*22/100, c2y = H*82/100;
        int R = (W*58/100); long R2 = (long)R*R;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                uint32_t c = 0x000A0A12;
                long d1 = (long)(x-c1x)*(x-c1x) + (long)(y-c1y)*(y-c1y);
                if (d1 < R2) { int f = (int)((R2-d1)*256/R2); f = f*f/256; c = addc(c, t->glow1, f*180/256); }
                long d2 = (long)(x-c2x)*(x-c2x) + (long)(y-c2y)*(y-c2y);
                if (d2 < R2) { int f = (int)((R2-d2)*256/R2); f = f*f/256; c = addc(c, t->glow2, f*150/256); }
                wallcache[y*W+x] = c;
            }
        }
    }
    /* pre-blur the wallpaper ONCE so window glass is cheap every frame */
    if (wallblur) fb_blur_buffer(wallblur, wallcache, W, H, t->win_blur > 0 ? t->win_blur + 2 : 4);
    wall_dirty = false;
}

/* ---------- text block (wrap, tail) ---------- */
static void draw_text_block(int x, int y, int cols, int rows, const char *tx,
                            uint32_t fg, bool tail, int *ecol, int *erow) {
    int total = 1, col = 0;
    for (const char *p = tx; *p; p++) {
        if (*p == '\n') { total++; col = 0; }
        else { col++; if (col >= cols) { total++; col = 0; } }
    }
    int skip = (tail && total > rows) ? total - rows : 0;
    col = 0; int row = 0;
    for (const char *p = tx; *p; p++) {
        if (*p == '\n') { row++; col = 0; continue; }
        int vr = row - skip;
        if (vr >= 0 && vr < rows) fb_draw_char(x + col*FW, y + vr*FH, *p, fg, 0, true);
        col++;
        if (col >= cols) { row++; col = 0; }
    }
    if (ecol) *ecol = col;
    if (erow) *erow = row - skip;
}

/* ---------- window content ---------- */
static const char *ABOUT[] = {
    "Welcome to m1keOS!",
    "",
    "A from-scratch operating system that",
    "puts you in charge of everything.",
    "",
    "Friendly now, fully technical when you",
    "want it. The system serves you.",
    "",
    "Open the Terminal and type 'help', or",
    "restyle this desktop by editing:",
    "  /etc/m1ke/theme.m1ss   (our own CSS)",
    "then run:  m1kectl theme reload",
};

static void content(window_t *w) {
    theme_t *t = theme_get();
    uint32_t acc = theme_accent();
    int cx = w->x + 10, cy = w->y + WTH + 6;
    int cols = (w->w - 20) / FW, rows = (w->h - WTH - 14) / FH;
    int kind = (int)(w - wins);

    if (kind == W_ABOUT) {
        for (unsigned i = 0; i < sizeof(ABOUT)/sizeof(ABOUT[0]); i++)
            fb_draw_string(cx, cy + (int)i*FH, ABOUT[i], i == 0 ? acc : 0x00E0E0E0, 0, true);
    } else if (kind == W_MON) {
        char b[64];
        ksnprintf(b,64,"uptime : %u s", (unsigned)timer_seconds()); fb_draw_string(cx,cy+0*FH,b,0x00E0E0E0,0,true);
        ksnprintf(b,64,"heap   : %u / %u KB",(unsigned)(heap_used()/1024),(unsigned)(heap_total()/1024)); fb_draw_string(cx,cy+1*FH,b,0x00E0E0E0,0,true);
        ksnprintf(b,64,"objects: control plane online"); fb_draw_string(cx,cy+2*FH,b,0x00E0E0E0,0,true);
        ksnprintf(b,64,"mouse  : %d,%d", mouse_x(), mouse_y()); fb_draw_string(cx,cy+3*FH,b,0x00E0E0E0,0,true);
        int bw = w->w - 20, used = (int)(heap_used()*bw/heap_total()); if (used < 2) used = 2;
        fb_blend_round(cx, cy+5*FH, bw, 10, 4, 0x00000000, 120);
        fb_fill_round(cx, cy+5*FH, used, 10, 4, acc);
    } else if (kind == W_FILES) {
        fb_draw_string(cx, cy, "/home/m1ke", acc, 0, true);
        fs_node_t *d = fs_resolve(fs_root(), "/home/m1ke");
        int row = 1;
        if (d) for (fs_node_t *c = d->children; c && row < rows; c = c->next, row++) {
            char b[80]; ksnprintf(b,80,"%s%s",c->name,c->type==FS_DIR?"/":"");
            fb_draw_string(cx, cy+row*FH, b, c->type==FS_DIR?acc:0x00E0E0E0, 0, true);
        }
    } else if (kind == W_EDIT) {
        fb_fill_round(w->x + w->w - 74, w->y + 5, 60, 18, 8, acc);
        fb_draw_string(w->x + w->w - 66, w->y + 6, "Save", 0, 0, true);
        char hdr[80]; ksnprintf(hdr,80,"%s  (%d bytes)%s",editpath,editlen, timer_ticks()<saved_until?"  saved!":"");
        fb_draw_string(cx, cy, hdr, TXT_DIM, 0, true);
        int ec, er;
        draw_text_block(cx, cy+FH, cols, rows-1, editbuf, 0x00E0E0E0, false, &ec, &er);
        if ((timer_ticks()/25)&1) fb_fill_round(cx+ec*FW, cy+FH+er*FH, FW, FH, 0, acc);
    } else if (kind == W_TERM) {
        /* a slightly darker inset panel for terminal readability */
        fb_blend_round(w->x+6, w->y+WTH+2, w->w-12, w->h-WTH-8, 8, t->term_bg, 150);
        int irow = rows - 1;
        draw_text_block(cx, cy, cols, irow, termout, t->term_fg, true, 0, 0);
        char line[160]; ksnprintf(line,160,"$ %s", termin);
        fb_draw_string(cx, cy+irow*FH, line, acc, 0, true);
        if ((timer_ticks()/25)&1) fb_fill_round(cx+(2+termlen)*FW, cy+irow*FH, FW, FH, 0, acc);
    } else if (kind == W_SET) {
        fb_draw_string(cx, cy, "Accent (click a swatch):", 0x00E0E0E0, 0, true);
        for (int i = 0; i < N_ACC; i++) {
            int sx = cx + i*48, sy = cy + FH + 4;
            fb_fill_round(sx, sy, 40, 24, 6, accents[i].col);
            if (accents[i].col == acc) fb_round_outline(sx-2, sy-2, 44, 28, 7, 0x00FFFFFF, 255);
        }
        fb_draw_string(cx, cy+3*FH+6, "Wallpaper:", 0x00E0E0E0, 0, true);
        const char *wn[] = { "aurora","gradient","grid","solid" };
        for (int i = 0; i < 4; i++) {
            int sx = cx + i*92, sy = cy + 4*FH + 8;
            fb_blend_round(sx, sy, 84, 22, 6, i==t->wallpaper?acc:0x00000000, i==t->wallpaper?255:120);
            fb_draw_string(sx+8, sy+3, wn[i], i==t->wallpaper?0:0x00E0E0E0, 0, true);
        }
        char b[64];
        ksnprintf(b,64,"blur=%d  radius=%d  opacity=%d", t->win_blur, t->win_radius, t->win_opacity);
        fb_draw_string(cx, cy+7*FH, b, TXT_DIM, 0, true);
        fb_draw_string(cx, cy+8*FH, "Full control: edit /etc/m1ke/theme.m1ss", TXT_DIM, 0, true);
        fb_draw_string(cx, cy+9*FH, "then: m1kectl theme reload", TXT_DIM, 0, true);
    }
}

/* ---------- window frame (glass) ---------- */
static void draw_window(window_t *w, bool act) {
    if (!w->open) return;
    theme_t *t = theme_get();
    uint32_t acc = theme_accent();
    int r = t->win_radius;

    /* soft drop shadow (few cheap layers) */
    for (int i = t->win_shadow; i >= 2; i -= 4)
        fb_blend_round(w->x - i, w->y - i + 6, w->w + 2*i, w->h + 2*i, r + i, 0x00000000, 9);

    /* frosted glass: anti-aliased, sampling the pre-blurred wallpaper (fast) */
    int op = act ? t->win_opacity : t->win_opacity + 18;
    if (op > 100) op = 100;
    fb_glass_round(w->x, w->y, w->w, w->h, r, wallblur ? wallblur : wallcache, t->win_tint, op*255/100);
    fb_blend_round(w->x, w->y, w->w, 46, r, 0x00FFFFFF, 6);                 /* top sheen */
    fb_blend_round(w->x, w->y, w->w, WTH, r, acc, act ? 70 : 32);          /* title tint */
    if (t->win_border) fb_round_outline(w->x, w->y, w->w, w->h, r, acc, act ? 210 : 90);

    fb_draw_string(w->x + 12, w->y + 6, w->title, act ? 0x00FFFFFF : TXT_DIM, 0, true);
    fb_fill_round(w->x + w->w - 22, w->y + 8, 12, 12, 6, 0x00FF5566);      /* close dot */
    content(w);
}

/* ---------- taskbar ---------- */
static int bar_y(void) {
    theme_t *t = theme_get();
    if (!t->bar_visible) return -1;
    return t->bar_pos ? 0 : (int)fb_height() - t->bar_height;
}

static void taskbar(void) {
    theme_t *t = theme_get();
    if (!t->bar_visible) return;
    int W = (int)fb_width();
    int by = bar_y(), bh = t->bar_height;
    uint32_t acc = theme_accent();
    fb_glass_round(0, by, W, bh, 0, wallblur ? wallblur : wallcache, 0x00100C14, t->bar_opacity*255/100);
    fb_fillrect(0, t->bar_pos ? by + bh - 1 : by, W, 1, acc);
    int pad = (bh - 20) / 2;
    fb_fill_round(8, by + pad, 92, 20, 8, acc);
    fb_draw_string(18, by + pad + 2, "m1keOS", 0, 0, true);
    int bx = 112;
    for (int i = 0; i < W_COUNT; i++) {
        if (!wins[i].open) continue;
        fb_blend_round(bx, by+pad, 120, 20, 7, i==active?acc:0x00FFFFFF, i==active?180:30);
        fb_draw_string(bx+8, by+pad+2, wins[i].title, i==active?0:0x00E0E0E0, 0, true);
        bx += 126;
    }
    char clk[16]; uint64_t s = timer_seconds();
    ksnprintf(clk,16,"%02u:%02u:%02u",(unsigned)(s/3600)%24,(unsigned)(s/60)%60,(unsigned)(s%60));
    fb_draw_string(W - 80, by + pad + 2, clk, acc, 0, true);
}

/* ---------- start menu ---------- */
static const char *menu_items[] = { "Terminal","Editor","Files","System Monitor","Settings","About","Exit to Shell" };
#define N_MENU ((int)(sizeof(menu_items)/sizeof(menu_items[0])))
#define MENU_W 210
static int menu_x(void) { return 8; }
static int menu_top(void) {
    theme_t *t = theme_get();
    int mh = N_MENU*24 + 12;
    if (t->bar_visible && t->bar_pos == 1) return t->bar_height + 6;          /* below top bar */
    int base = t->bar_visible ? (int)fb_height() - t->bar_height : (int)fb_height();
    return base - mh - 6;                                                     /* above bottom bar */
}
static void startmenu_draw(void) {
    if (!startmenu) return;
    uint32_t acc = theme_accent();
    int mx = menu_x(), my = menu_top(), mh = N_MENU*24 + 12;
    fb_glass_round(mx, my, MENU_W, mh, 12, wallblur ? wallblur : wallcache, 0x00141018, 235);
    fb_round_outline(mx, my, MENU_W, mh, 12, acc, 180);
    fb_draw_string(mx + 14, my + 8, "Applications", acc, 0, true);
    for (int i = 0; i < N_MENU; i++) {
        int iy = my + 28 + i*24;
        fb_draw_string(mx + 16, iy, menu_items[i], i==N_MENU-1?0x00FF5566:0x00EAEAEA, 0, true);
    }
}
static int menu_to_win(int i) {
    switch (i){case 0:return W_TERM;case 1:return W_EDIT;case 2:return W_FILES;case 3:return W_MON;case 4:return W_SET;case 5:return W_ABOUT;}
    return -1;
}

/* ---------- terminal ---------- */
static void term_run(void) {
    char prompt[160]; ksnprintf(prompt,160,"$ %s\n", termin);
    if (!strcmp(termin,"clear")) { termout[0]=0; termlen=0; termin[0]=0; return; }
    if (!strcmp(termin,"gui"))   { strcat(termout,prompt); strcat(termout,"(already in the desktop)\n"); termlen=0; termin[0]=0; return; }
    if (strlen(termout)+320 >= sizeof(termout)) termout[0]=0;
    strcat(termout, prompt);
    char cmd[128]; strncpy(cmd, termin, sizeof(cmd)); cmd[sizeof(cmd)-1]=0;
    char cap[2048]; console_capture_begin(cap, sizeof(cap));
    shell_exec_line(cmd);
    console_capture_end();
    if (strlen(termout)+strlen(cap) < sizeof(termout)) strcat(termout, cap);
    /* a theme command may have restyled things */
    wall_dirty = true;
    termlen = 0; termin[0] = 0;
}

static void editor_key(int c) {
    if (c=='\b') { if (editlen>0) editbuf[--editlen]=0; }
    else if (c=='\n') { if (editlen<(int)sizeof(editbuf)-1){editbuf[editlen++]='\n';editbuf[editlen]=0;} }
    else if (c>=32&&c<256&&c!=127) { if (editlen<(int)sizeof(editbuf)-1){editbuf[editlen++]=(char)c;editbuf[editlen]=0;} }
}
static void terminal_key(int c) {
    if (c=='\b') { if (termlen>0) termin[--termlen]=0; }
    else if (c=='\n') term_run();
    else if (c>=32&&c<256&&c!=127) { if (termlen<(int)sizeof(termin)-2){termin[termlen++]=(char)c;termin[termlen]=0;} }
}
static int next_open(int from) {
    for (int k=1;k<=W_COUNT;k++){int i=(from+k)%W_COUNT;if(wins[i].open)return i;}
    return from;
}
static void editor_load(void) {
    fs_node_t *f = fs_resolve(fs_root(), editpath);
    if (f && f->type==FS_FILE && f->data) { strncpy(editbuf,f->data,sizeof(editbuf)-1); editbuf[sizeof(editbuf)-1]=0; }
    else strcpy(editbuf, "Welcome to m1edit!\nType, then click Save.\n");
    editlen = (int)strlen(editbuf);
}

void desktop_run(void) {
    int W = (int)fb_width(), H = (int)fb_height();
    mouse_init(W, H);
    if (!wallcache) wallcache = (uint32_t *)kmalloc((size_t)W * H * 4);
    if (!wallblur)  wallblur  = (uint32_t *)kmalloc((size_t)W * H * 4);
    wall_dirty = true;

    wins[W_TERM]  = (window_t){ 50,  80, 540, 330, "Terminal",       true  };
    wins[W_EDIT]  = (window_t){ 500, 130,440, 330, "Editor",         false };
    wins[W_FILES] = (window_t){ 620, 90, 360, 230, "Files",          true  };
    wins[W_MON]   = (window_t){ 120, 430,380, 170, "System Monitor", false };
    wins[W_SET]   = (window_t){ 300, 250,470, 230, "Settings",       false };
    wins[W_ABOUT] = (window_t){ 90,  120,440, 270, "About",          true  };
    active = W_TERM;
    startmenu = false;
    termout[0]=0; termlen=0; termin[0]=0;
    strcpy(termout, "m1keOS terminal - real shell. Try: help, m1kectl help, neofetch\n\n");
    editor_load();

    int last_mx = mouse_x(), last_my = mouse_y();
    bool dragging = false, last_btn = false, quit = false, scene_dirty = true;
    uint64_t last_sec = (uint64_t)-1;

    while (!quit) {
        theme_t *t = theme_get();
        int c = input_poll();
        window_t *aw = &wins[active];
        bool text_app = (active==W_EDIT || active==W_TERM) && !startmenu;

        if (c == 27) break;
        else if (c == '`') startmenu = !startmenu;
        else if (c == '\t') active = next_open(active);
        else if (text_app && active==W_EDIT) editor_key(c);
        else if (text_app && active==W_TERM) terminal_key(c);
        else if (c == KEY_LEFT)  aw->x -= 16;
        else if (c == KEY_RIGHT) aw->x += 16;
        else if (c == KEY_UP)    aw->y -= 16;
        else if (c == KEY_DOWN)  aw->y += 16;
        if (c != -1) scene_dirty = true;

        int mxp = mouse_x(), myp = mouse_y();
        bool btn = mouse_left();
        int by = bar_y(), bh = t->bar_height, pad = (bh-20)/2;
        bool cursor_moved = (mxp != last_mx || myp != last_my);
        if (btn != last_btn) scene_dirty = true;

        if (btn && !last_btn) {
            if (startmenu) {
                int mx = menu_x(), my = menu_top();
                for (int i = 0; i < N_MENU; i++)
                    if (in_rect(mxp, myp, mx, my + 28 + i*24 - 4, MENU_W, 22)) {
                        if (i == N_MENU-1) quit = true;
                        else { int wk = menu_to_win(i); if (wk>=0){wins[wk].open=true; active=wk; if(wk==W_EDIT) editor_load();} }
                    }
                startmenu = false;
            } else if (t->bar_visible && in_rect(mxp, myp, 8, by+pad, 92, 20)) {
                startmenu = true;
            } else {
                if (t->bar_visible) {
                    int bx = 112;
                    for (int i = 0; i < W_COUNT; i++) { if(!wins[i].open) continue; if(in_rect(mxp,myp,bx,by+pad,120,20)) active=i; bx += 126; }
                }
                for (int i = 0; i < W_COUNT; i++)
                    if (wins[i].open && in_rect(mxp,myp,wins[i].x,wins[i].y,wins[i].w,wins[i].h)) active = i;
                aw = &wins[active];
                if (aw->open) {
                    if (in_rect(mxp,myp,aw->x+aw->w-22,aw->y+8,12,12)) aw->open = false;
                    else if (active==W_EDIT && in_rect(mxp,myp,aw->x+aw->w-74,aw->y+5,60,18)) { fs_put(editpath, editbuf); saved_until = timer_ticks()+150; }
                    else if (active==W_SET) {
                        int scx = aw->x+10, scy = aw->y+WTH+6;
                        for (int i = 0; i < N_ACC; i++)
                            if (in_rect(mxp,myp,scx+i*48,scy+FH+4,40,24)) { theme_set("accent", accents[i].name); wall_dirty = true; }
                        const char *wn[] = {"aurora","gradient","grid","solid"};
                        for (int i = 0; i < 4; i++)
                            if (in_rect(mxp,myp,scx+i*92,scy+4*FH+8,84,22)) { theme_set("wallpaper", wn[i]); wall_dirty = true; }
                    }
                    else if (in_rect(mxp,myp,aw->x,aw->y,aw->w,WTH)) dragging = true;
                }
            }
        }
        if (!btn) dragging = false;
        if (dragging && cursor_moved) {
            wins[active].x += mxp - last_mx;
            wins[active].y += myp - last_my;
            scene_dirty = true;
        }

        /* once-a-second redraw keeps the clock alive */
        uint64_t sec = timer_seconds();
        if (sec != last_sec) { last_sec = sec; scene_dirty = true; }
        if (wall_dirty) scene_dirty = true;

        if (scene_dirty) {
            /* full recomposite of the scene into the back buffer (no cursor) */
            if (wall_dirty) build_wallpaper();
            if (wallcache) fb_blit(wallcache); else fb_fillrect(0, 0, W, H, 0x000A0A12);
            fb_draw_string(20, 16, "m1keOS", theme_accent(), 0, true);
            fb_draw_string(20 + 7*FW, 16, "desktop", TXT_DIM, 0, true);
            for (int i = 0; i < W_COUNT; i++) if (i != active) draw_window(&wins[i], false);
            draw_window(&wins[active], true);
            taskbar();
            startmenu_draw();
            fb_present();                                   /* one full present */
            draw_cursor(mxp, myp);                          /* cursor overlay (hw) */
            scene_dirty = false;
        } else if (cursor_moved) {
            /* nothing but the cursor changed: only repaint the two tiny rects */
            fb_present_rect(last_mx, last_my, CURSOR_W + 1, CURSOR_H + 1);  /* erase old */
            draw_cursor(mxp, myp);                                          /* draw new */
        } else {
            last_mx = mxp; last_my = myp; last_btn = btn;
            __asm__ __volatile__("hlt");                    /* idle: sleep the CPU */
            continue;
        }
        last_mx = mxp; last_my = myp; last_btn = btn;
    }

    console_clear();
    console_set_color(COL_WHITE, COL_BLACK);
}
