/* m1ss.h - m1ke Style Sheets: a small CSS-like language that styles the whole
 * desktop from a text file (/etc/m1ke/theme.m1ss). Edit it, reload live. */
#ifndef M1KE_M1SS_H
#define M1KE_M1SS_H
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    /* desktop */
    uint32_t accent;
    int      wallpaper;       /* 0 aurora, 1 gradient, 2 grid, 3 solid */
    uint32_t glow1, glow2;    /* aurora glow colors */
    /* window */
    int      win_radius;
    int      win_blur;
    int      win_opacity;     /* 0..100 frosted-glass strength */
    int      win_shadow;      /* shadow spread (px) */
    int      win_border;      /* 0/1 hairline border */
    uint32_t win_tint;        /* glass tint color */
    /* taskbar */
    bool     bar_visible;
    int      bar_pos;         /* 0 bottom, 1 top */
    int      bar_height;
    int      bar_opacity;     /* 0..100 */
    /* terminal */
    uint32_t term_bg, term_fg;
} theme_t;

#define THEME_PATH "/etc/m1ke/theme.m1ss"

void     theme_init(void);                 /* load file (or write default) + apply */
theme_t *theme_get(void);
void     theme_reload(void);               /* re-read + parse + apply accent */
void     theme_save(void);                 /* regenerate readable .m1ss text */
void     theme_show(void);                 /* print current theme as m1ss */
bool     theme_set(const char *prop, const char *val);  /* "window.radius", "accent", ... */
bool     m1ss_parse_color(const char *s, uint32_t *out);

#endif
