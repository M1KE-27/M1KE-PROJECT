/* console.h - unified text console over VGA text mode or RGB framebuffer.
 * All output is also mirrored to the serial port. */
#ifndef M1KE_CONSOLE_H
#define M1KE_CONSOLE_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "multiboot.h"

/* m1keOS palette (0x00RRGGBB) */
#define COL_BLACK     0x00000000
#define COL_ORANGE    0x00FF8C1A   /* primary accent */
#define COL_AMBER     0x00FFB347
#define COL_ORANGE_DK 0x00C2670F
#define COL_YELLOW    0x00FFD24A
#define COL_WHITE     0x00EAEAEA
#define COL_GRAY      0x00909090
#define COL_DKGRAY    0x00303030
#define COL_GREEN     0x0046E07A
#define COL_RED       0x00FF5555
#define COL_BLUE      0x005AA8FF
#define COL_PURPLE    0x00B98CFF
#define COL_DKBLUE    0x00101828

void console_init(multiboot_info_t *mbi);
void console_clear(void);
void console_putc(char c);
void console_write(const char *s);
void console_set_color(uint32_t fg, uint32_t bg);
void console_get_color(uint32_t *fg, uint32_t *bg);

/* live-customizable theme accent (default orange) */
void     theme_set_accent(uint32_t color);
uint32_t theme_accent(void);

bool console_is_graphical(void);

/* output capture (used by the in-window terminal): while active, kputc text is
 * written to the capture buffer instead of the screen. */
void   console_capture_begin(char *buf, size_t cap);
size_t console_capture_end(void);

/* Raw framebuffer accessors (only valid when graphical). */
bool      fb_available(void);
uint32_t  fb_width(void);
uint32_t  fb_height(void);
uint32_t *fb_backbuffer(void);
void      fb_present(void);
void      fb_putpixel(int x, int y, uint32_t color);
void      fb_fillrect(int x, int y, int w, int h, uint32_t color);
void      fb_fill_round(int x, int y, int w, int h, int radius, uint32_t color);
/* modern UI compositing primitives (operate on the back buffer) */
void      fb_blend_round(int x, int y, int w, int h, int radius, uint32_t color, int alpha);
void      fb_round_outline(int x, int y, int w, int h, int radius, uint32_t color, int alpha);
void      fb_blur_region(int x, int y, int w, int h, int radius);
void      fb_blur_buffer(uint32_t *dst, const uint32_t *src, int w, int h, int radius); /* one-shot */
void      fb_blit(const uint32_t *src);   /* copy a full fb_w*fb_h image into the back buffer */
/* anti-aliased frosted-glass panel sampling a precomputed blurred image `src` */
void      fb_glass_round(int x, int y, int w, int h, int radius, const uint32_t *src, uint32_t tint, int alpha);
/* dirty-rectangle present: copy only one back-buffer rect to the hardware */
void      fb_present_rect(int x, int y, int w, int h);
/* write a pixel straight to the hardware framebuffer (cursor overlay) */
void      fb_hw_putpixel(int x, int y, uint32_t color);
void      fb_draw_char(int px, int py, char c, uint32_t fg, uint32_t bg, bool transparent_bg);
void      fb_draw_string(int px, int py, const char *s, uint32_t fg, uint32_t bg, bool transparent_bg);
void      fb_draw_string_scaled(int px, int py, const char *s, int scale, uint32_t fg);

#endif
