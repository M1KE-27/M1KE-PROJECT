/* console.c - text console for VGA text mode and RGB framebuffer.
 * Graphical text is rendered directly to the hardware framebuffer using an
 * 8x16 font. GUI helpers draw into a software back buffer (fb_present blits).
 * All text output is mirrored to the serial port. */
#include "console.h"
#include "serial.h"
#include "../lib/string.h"
#include "font8x16.h"

/* ---- shared state ---- */
static int   g_mode;          /* 0 = none, 1 = vga text, 2 = framebuffer */
static uint32_t g_fg = COL_ORANGE;
static uint32_t g_bg = COL_BLACK;
static uint32_t g_accent = COL_ORANGE;

/* output capture */
static char  *cap_buf;
static size_t cap_cap, cap_len;

/* ---- VGA text mode ---- */
static volatile uint16_t *vga = (uint16_t *)0xB8000;
static int vt_x, vt_y;
#define VT_COLS 80
#define VT_ROWS 25

/* ---- framebuffer ---- */
static uint32_t *fb;          /* hardware framebuffer */
static uint32_t  fb_w, fb_h, fb_pitch_px;
static int       cx, cy;      /* text cursor in pixels' grid cells */
static int       cols, rows;

#define MAX_FB_PIXELS (1280 * 1024)
static uint32_t backbuffer[MAX_FB_PIXELS];
static int       have_backbuffer;

/* ---------- color helpers ---------- */
static uint8_t vga_attr(uint32_t fg) {
    /* crude RGB -> 16-color VGA mapping, biased to the orange theme */
    if (fg == COL_ORANGE || fg == COL_YELLOW) return 0x0E;  /* yellow */
    if (fg == COL_GREEN)  return 0x0A;
    if (fg == COL_RED)    return 0x0C;
    if (fg == COL_BLUE)   return 0x09;
    if (fg == COL_GRAY || fg == COL_DKGRAY) return 0x07;
    return 0x0F; /* white */
}

bool console_is_graphical(void) { return g_mode == 2; }

void console_set_color(uint32_t fg, uint32_t bg) { g_fg = fg; g_bg = bg; }
void console_get_color(uint32_t *fg, uint32_t *bg) { *fg = g_fg; *bg = g_bg; }

void     theme_set_accent(uint32_t color) { g_accent = color; }
uint32_t theme_accent(void) { return g_accent; }

void console_capture_begin(char *buf, size_t cap) {
    cap_buf = buf; cap_cap = cap; cap_len = 0;
    if (buf && cap) buf[0] = 0;
}
size_t console_capture_end(void) {
    size_t n = cap_len;
    cap_buf = 0; cap_cap = cap_len = 0;
    return n;
}

/* ---------- VGA text backend ---------- */
static void vt_clear(void) {
    uint16_t cell = (uint16_t)(' ' | (vga_attr(g_fg) << 8));
    for (int i = 0; i < VT_COLS * VT_ROWS; i++) vga[i] = cell;
    vt_x = vt_y = 0;
}

static void vt_scroll(void) {
    for (int i = 0; i < VT_COLS * (VT_ROWS - 1); i++) vga[i] = vga[i + VT_COLS];
    uint16_t cell = (uint16_t)(' ' | (vga_attr(g_fg) << 8));
    for (int i = VT_COLS * (VT_ROWS - 1); i < VT_COLS * VT_ROWS; i++) vga[i] = cell;
}

static void vt_putc(char c) {
    if (c == '\n') { vt_x = 0; vt_y++; }
    else if (c == '\r') { vt_x = 0; }
    else if (c == '\b') { if (vt_x > 0) { vt_x--; vga[vt_y * VT_COLS + vt_x] = (uint16_t)(' ' | (vga_attr(g_fg) << 8)); } }
    else if (c == '\t') { vt_x = (vt_x + 8) & ~7; }
    else {
        vga[vt_y * VT_COLS + vt_x] = (uint16_t)((uint8_t)c | (vga_attr(g_fg) << 8));
        vt_x++;
    }
    if (vt_x >= VT_COLS) { vt_x = 0; vt_y++; }
    if (vt_y >= VT_ROWS) { vt_scroll(); vt_y = VT_ROWS - 1; }
}

/* ---------- framebuffer backend ---------- */
static inline void fb_real_pixel(int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || (uint32_t)x >= fb_w || (uint32_t)y >= fb_h) return;
    fb[y * fb_pitch_px + x] = color;
}

static void fb_glyph(int px, int py, char c, uint32_t fg, uint32_t bg, bool transparent) {
    const unsigned char *g = font8x16[(uint8_t)c];
    for (int row = 0; row < FONT_HEIGHT; row++) {
        unsigned char bits = g[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            if (bits & (0x80 >> col)) fb_real_pixel(px + col, py + row, fg);
            else if (!transparent)    fb_real_pixel(px + col, py + row, bg);
        }
    }
}

static void fb_scroll(void) {
    /* move everything up by one text row (FONT_HEIGHT px) */
    int row_px = FONT_HEIGHT;
    for (uint32_t y = 0; y < fb_h - row_px; y++)
        memcpy(&fb[y * fb_pitch_px], &fb[(y + row_px) * fb_pitch_px], fb_w * 4);
    for (uint32_t y = fb_h - row_px; y < fb_h; y++)
        for (uint32_t x = 0; x < fb_w; x++) fb[y * fb_pitch_px + x] = g_bg;
}

static void fb_clear(void) {
    for (uint32_t y = 0; y < fb_h; y++)
        for (uint32_t x = 0; x < fb_w; x++) fb[y * fb_pitch_px + x] = g_bg;
    cx = cy = 0;
}

static void fb_putc(char c) {
    if (c == '\n') { cx = 0; cy++; }
    else if (c == '\r') { cx = 0; }
    else if (c == '\t') { cx = (cx + 4) & ~3; }
    else if (c == '\b') {
        if (cx > 0) { cx--; fb_glyph(cx * FONT_WIDTH, cy * FONT_HEIGHT, ' ', g_fg, g_bg, false); }
    } else {
        fb_glyph(cx * FONT_WIDTH, cy * FONT_HEIGHT, c, g_fg, g_bg, false);
        cx++;
    }
    if (cx >= cols) { cx = 0; cy++; }
    if (cy >= rows) { fb_scroll(); cy = rows - 1; }
}

/* ---------- public ---------- */
void console_init(multiboot_info_t *mbi) {
    if ((mbi->flags & MB_FLAG_FRAMEBUF) && mbi->framebuffer_type == 1 &&
        mbi->framebuffer_bpp == 32) {
        g_mode = 2;
        fb = (uint32_t *)(uintptr_t)mbi->framebuffer_addr;
        fb_w = mbi->framebuffer_width;
        fb_h = mbi->framebuffer_height;
        fb_pitch_px = mbi->framebuffer_pitch / 4;
        cols = (int)fb_w / FONT_WIDTH;
        rows = (int)fb_h / FONT_HEIGHT;
        have_backbuffer = (fb_w * fb_h <= MAX_FB_PIXELS);
        fb_clear();
    } else {
        g_mode = 1;
        vt_clear();
    }
}

void console_clear(void) {
    if (g_mode == 2) fb_clear();
    else if (g_mode == 1) vt_clear();
}

void console_putc(char c) {
    if (g_mode == 2) fb_putc(c);
    else if (g_mode == 1) vt_putc(c);
}

void console_write(const char *s) { while (*s) console_putc(*s++); }

/* kputc: used by kprintf - screen + serial (or capture buffer when active) */
void kputc(char c) {
    if (c == '\n') serial_putc('\r');
    serial_putc(c);
    if (cap_buf) {
        if (cap_len + 1 < cap_cap) { cap_buf[cap_len++] = c; cap_buf[cap_len] = 0; }
        return;   /* suppress screen output while capturing */
    }
    console_putc(c);
}

/* ---------- framebuffer GUI helpers (operate on back buffer) ---------- */
bool      fb_available(void)   { return g_mode == 2; }
uint32_t  fb_width(void)       { return fb_w; }
uint32_t  fb_height(void)      { return fb_h; }
uint32_t *fb_backbuffer(void)  { return have_backbuffer ? backbuffer : fb; }

void fb_present(void) {
    if (!have_backbuffer) return;  /* already drawing direct */
    for (uint32_t y = 0; y < fb_h; y++)
        memcpy(&fb[y * fb_pitch_px], &backbuffer[y * fb_w], fb_w * 4);
}

void fb_putpixel(int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || (uint32_t)x >= fb_w || (uint32_t)y >= fb_h) return;
    uint32_t *buf = fb_backbuffer();
    uint32_t stride = have_backbuffer ? fb_w : fb_pitch_px;
    buf[y * stride + x] = color;
}

void fb_fillrect(int x, int y, int w, int h, uint32_t color) {
    uint32_t *buf = fb_backbuffer();
    uint32_t stride = have_backbuffer ? fb_w : fb_pitch_px;
    for (int j = 0; j < h; j++) {
        int yy = y + j;
        if (yy < 0 || (uint32_t)yy >= fb_h) continue;
        for (int i = 0; i < w; i++) {
            int xx = x + i;
            if (xx < 0 || (uint32_t)xx >= fb_w) continue;
            buf[yy * stride + xx] = color;
        }
    }
}

static int isqrt_i(int n) {
    if (n < 0) return 0;
    int x = n, y = (x + 1) / 2;
    while (y < x) { x = y; y = (x + n / x) / 2; }
    return x;
}

/* filled rectangle with rounded corners (radius px) */
void fb_fill_round(int x, int y, int w, int h, int radius, uint32_t color) {
    if (radius <= 0) { fb_fillrect(x, y, w, h, color); return; }
    int r = radius;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    for (int j = 0; j < h; j++) {
        int inset = 0, off = -1;
        if (j < r) off = r - j;
        else if (j >= h - r) off = j - (h - r) + 1;
        if (off > 0) inset = r - isqrt_i(r * r - off * off);
        fb_fillrect(x + inset, y + j, w - 2 * inset, 1, color);
    }
}

/* alpha-blend src over dst, alpha 0..255 */
static inline uint32_t blend_px(uint32_t dst, uint32_t src, int a) {
    int r1 = (dst >> 16) & 255, g1 = (dst >> 8) & 255, b1 = dst & 255;
    int r2 = (src >> 16) & 255, g2 = (src >> 8) & 255, b2 = src & 255;
    int r = (r2 * a + r1 * (255 - a)) / 255;
    int g = (g2 * a + g1 * (255 - a)) / 255;
    int b = (b2 * a + b1 * (255 - a)) / 255;
    return (uint32_t)((r << 16) | (g << 8) | b);
}

static int round_inset(int j, int h, int r) {
    int off = -1;
    if (j < r) off = r - j;
    else if (j >= h - r) off = j - (h - r) + 1;
    if (off > 0 && r > 0) return r - isqrt_i(r * r - off * off);
    return 0;
}

void fb_blend_round(int x, int y, int w, int h, int radius, uint32_t color, int alpha) {
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;
    uint32_t *buf = fb_backbuffer();
    uint32_t stride = have_backbuffer ? fb_w : fb_pitch_px;
    int r = radius; if (r < 0) r = 0; if (r > w/2) r = w/2; if (r > h/2) r = h/2;
    for (int j = 0; j < h; j++) {
        int yy = y + j; if (yy < 0 || (uint32_t)yy >= fb_h) continue;
        int inset = round_inset(j, h, r);
        for (int xx = x + inset; xx < x + w - inset; xx++) {
            if (xx < 0 || (uint32_t)xx >= fb_w) continue;
            uint32_t *p = &buf[yy * stride + xx];
            *p = blend_px(*p, color, alpha);
        }
    }
}

void fb_round_outline(int x, int y, int w, int h, int radius, uint32_t color, int alpha) {
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;
    uint32_t *buf = fb_backbuffer();
    uint32_t stride = have_backbuffer ? fb_w : fb_pitch_px;
    int r = radius; if (r < 0) r = 0; if (r > w/2) r = w/2; if (r > h/2) r = h/2;
    for (int j = 0; j < h; j++) {
        int yy = y + j; if (yy < 0 || (uint32_t)yy >= fb_h) continue;
        int inset = round_inset(j, h, r);
        int xL = x + inset, xR = x + w - 1 - inset;
        if (j == 0 || j == h - 1) {
            for (int xx = xL; xx <= xR; xx++) {
                if (xx < 0 || (uint32_t)xx >= fb_w) continue;
                uint32_t *p = &buf[yy * stride + xx]; *p = blend_px(*p, color, alpha);
            }
        } else {
            if (xL >= 0 && (uint32_t)xL < fb_w) { uint32_t *p = &buf[yy*stride+xL]; *p = blend_px(*p, color, alpha); }
            if (xR >= 0 && (uint32_t)xR < fb_w) { uint32_t *p = &buf[yy*stride+xR]; *p = blend_px(*p, color, alpha); }
        }
    }
}

void fb_blur_region(int x, int y, int w, int h, int radius) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if ((uint32_t)(x + w) > fb_w) w = (int)fb_w - x;
    if ((uint32_t)(y + h) > fb_h) h = (int)fb_h - y;
    if (w <= 0 || h <= 0 || radius <= 0) return;
    uint32_t *buf = fb_backbuffer();
    uint32_t stride = have_backbuffer ? fb_w : fb_pitch_px;
    static uint32_t tmp[1280];
    int r = radius;
    for (int j = 0; j < h; j++) {                       /* horizontal pass */
        uint32_t *row = &buf[(y + j) * stride + x];
        for (int i = 0; i < w; i++) tmp[i] = row[i];
        for (int i = 0; i < w; i++) {
            int rr = 0, gg = 0, bb = 0, n = 0;
            for (int k = i - r; k <= i + r; k++) {
                if (k < 0 || k >= w) continue;
                uint32_t c = tmp[k]; rr += (c>>16)&255; gg += (c>>8)&255; bb += c&255; n++;
            }
            row[i] = (uint32_t)(((rr/n)<<16) | ((gg/n)<<8) | (bb/n));
        }
    }
    static uint32_t tcol[1024];
    for (int i = 0; i < w; i++) {                       /* vertical pass */
        for (int j = 0; j < h; j++) tcol[j] = buf[(y + j) * stride + x + i];
        for (int j = 0; j < h; j++) {
            int rr = 0, gg = 0, bb = 0, n = 0;
            for (int k = j - r; k <= j + r; k++) {
                if (k < 0 || k >= h) continue;
                uint32_t c = tcol[k]; rr += (c>>16)&255; gg += (c>>8)&255; bb += c&255; n++;
            }
            buf[(y + j) * stride + x + i] = (uint32_t)(((rr/n)<<16) | ((gg/n)<<8) | (bb/n));
        }
    }
}

void fb_blit(const uint32_t *src) {
    uint32_t stride = have_backbuffer ? fb_w : fb_pitch_px;
    uint32_t *buf = fb_backbuffer();
    for (uint32_t y = 0; y < fb_h; y++)
        memcpy(&buf[y * stride], &src[y * fb_w], fb_w * 4);
}

/* copy just one rectangle from the back buffer to the hardware framebuffer */
void fb_present_rect(int x, int y, int w, int h) {
    if (!have_backbuffer) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if ((uint32_t)(x + w) > fb_w) w = (int)fb_w - x;
    if ((uint32_t)(y + h) > fb_h) h = (int)fb_h - y;
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++)
        memcpy(&fb[(y + j) * fb_pitch_px + x], &backbuffer[(y + j) * fb_w + x], (size_t)w * 4);
}

void fb_hw_putpixel(int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || (uint32_t)x >= fb_w || (uint32_t)y >= fb_h) return;
    fb[y * fb_pitch_px + x] = color;
}

/* O(w*h) separable box blur src->dst (one-shot; used to pre-blur the wallpaper) */
void fb_blur_buffer(uint32_t *dst, const uint32_t *src, int w, int h, int radius) {
    if (radius <= 0) { memcpy(dst, src, (size_t)w*h*4); return; }
    int r = radius;
    /* horizontal: src -> dst */
    for (int y = 0; y < h; y++) {
        const uint32_t *s = &src[y*w];
        uint32_t *d = &dst[y*w];
        long sr=0,sg=0,sb=0; int cnt=0;
        for (int k = 0; k <= r && k < w; k++) { uint32_t c=s[k]; sr+=(c>>16)&255; sg+=(c>>8)&255; sb+=c&255; cnt++; }
        for (int i = 0; i < w; i++) {
            d[i] = (uint32_t)(((sr/cnt)<<16)|((sg/cnt)<<8)|(sb/cnt));
            int l = i - r; if (l >= 0) { uint32_t c=s[l]; sr-=(c>>16)&255; sg-=(c>>8)&255; sb-=c&255; cnt--; }
            int rr = i + r + 1; if (rr < w) { uint32_t c=s[rr]; sr+=(c>>16)&255; sg+=(c>>8)&255; sb+=c&255; cnt++; }
        }
    }
    /* vertical: dst -> dst (via temp column) */
    static uint32_t col[1024];
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) col[y] = dst[y*w + x];
        long sr=0,sg=0,sb=0; int cnt=0;
        for (int k = 0; k <= r && k < h; k++) { uint32_t c=col[k]; sr+=(c>>16)&255; sg+=(c>>8)&255; sb+=c&255; cnt++; }
        for (int y = 0; y < h; y++) {
            dst[y*w + x] = (uint32_t)(((sr/cnt)<<16)|((sg/cnt)<<8)|(sb/cnt));
            int l = y - r; if (l >= 0) { uint32_t c=col[l]; sr-=(c>>16)&255; sg-=(c>>8)&255; sb-=c&255; cnt--; }
            int rr = y + r + 1; if (rr < h) { uint32_t c=col[rr]; sr+=(c>>16)&255; sg+=(c>>8)&255; sb+=c&255; cnt++; }
        }
    }
}

void fb_glass_round(int x, int y, int w, int h, int radius, const uint32_t *src, uint32_t tint, int alpha) {
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;
    uint32_t *buf = fb_backbuffer();
    uint32_t stride = have_backbuffer ? fb_w : fb_pitch_px;
    int r = radius; if (r < 0) r = 0; if (r > w/2) r = w/2; if (r > h/2) r = h/2;
    int r2 = r*r, ri2 = (r-1)*(r-1);
    for (int j = 0; j < h; j++) {
        int yy = y + j; if (yy < 0 || (uint32_t)yy >= fb_h) continue;
        int ccy = (j < r) ? r : (j >= h - r) ? h-1-r : -1;
        for (int i = 0; i < w; i++) {
            int xx = x + i; if (xx < 0 || (uint32_t)xx >= fb_w) continue;
            int cov = 255;
            if (r > 0 && ccy >= 0) {
                int ccx = (i < r) ? r : (i >= w - r) ? w-1-r : -1;
                if (ccx >= 0) {
                    int dx = i - ccx, dy = j - ccy, d2 = dx*dx + dy*dy;
                    if (d2 >= r2) continue;
                    else if (d2 > ri2) cov = (r2 - d2) * 255 / (r2 - ri2 + 1);
                }
            }
            uint32_t bg = src[(uint32_t)yy * fb_w + (uint32_t)xx];
            uint32_t glass = blend_px(bg, tint, alpha);
            uint32_t *p = &buf[yy * stride + xx];
            *p = (cov >= 255) ? glass : blend_px(*p, glass, cov);
        }
    }
}

void fb_draw_char(int px, int py, char c, uint32_t fg, uint32_t bg, bool transparent_bg) {
    uint32_t *buf = fb_backbuffer();
    uint32_t stride = have_backbuffer ? fb_w : fb_pitch_px;
    const unsigned char *g = font8x16[(uint8_t)c];
    for (int row = 0; row < FONT_HEIGHT; row++) {
        unsigned char bits = g[row];
        int yy = py + row;
        if (yy < 0 || (uint32_t)yy >= fb_h) continue;
        for (int col = 0; col < FONT_WIDTH; col++) {
            int xx = px + col;
            if (xx < 0 || (uint32_t)xx >= fb_w) continue;
            if (bits & (0x80 >> col)) buf[yy * stride + xx] = fg;
            else if (!transparent_bg) buf[yy * stride + xx] = bg;
        }
    }
}

void fb_draw_string(int px, int py, const char *s, uint32_t fg, uint32_t bg, bool transparent_bg) {
    while (*s) {
        fb_draw_char(px, py, *s++, fg, bg, transparent_bg);
        px += FONT_WIDTH;
    }
}

/* draw text magnified by an integer scale (transparent background) */
void fb_draw_string_scaled(int px, int py, const char *s, int scale, uint32_t fg) {
    if (scale < 1) scale = 1;
    int x = px;
    for (; *s; s++) {
        const unsigned char *g = font8x16[(uint8_t)*s];
        for (int row = 0; row < FONT_HEIGHT; row++) {
            unsigned char bits = g[row];
            for (int col = 0; col < FONT_WIDTH; col++) {
                if (bits & (0x80 >> col))
                    fb_fillrect(x + col * scale, py + row * scale, scale, scale, fg);
            }
        }
        x += FONT_WIDTH * scale;
    }
}
