/* keyboard.c - PS/2 keyboard, scancode set 1, IRQ1 driven.
 * Supports US and ES (Spanish, Spain) layouts with Shift, CapsLock and AltGr. */
#include "keyboard.h"
#include "io.h"
#include "../arch/isr.h"

#define BUF_SIZE 256
static volatile int  buf[BUF_SIZE];
static volatile int  head, tail;
static bool shift, caps, ext, altgr;
static int  layout;   /* 0 = US, 1 = ES */
static int  dead;     /* pending dead accent: 0 none, 1 acute(´), 2 diaeresis(¨) */

static const char map_lower[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
static const char map_upper[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* Spanish (ES) overrides for the keys that differ from US. Letters a-z stay in
 * their US positions; only symbols and the ñ/ç keys change. CP437 byte values:
 * ñ=0xA4 Ñ=0xA5 ç=0x87 Ç=0x80 ª=0xA6 º=0xA7 ¡=0xAD ¿=0xA8 ·=0xFA ¬=0xAA */
static int es_lookup(uint8_t sc, bool sh, bool cp, bool ag) {
    if (ag) {
        switch (sc) {
            case 0x02: return '|';  case 0x03: return '@';  case 0x04: return '#';
            case 0x05: return '~';  case 0x07: return 0xAA; case 0x1A: return '[';
            case 0x1B: return ']';  case 0x2B: return '}';  case 0x29: return '\\';
        }
    }
    switch (sc) {
        case 0x02: return sh ? '!'  : '1';
        case 0x03: return sh ? '"'  : '2';
        case 0x04: return sh ? 0xFA : '3';   /* · */
        case 0x05: return sh ? '$'  : '4';
        case 0x06: return sh ? '%'  : '5';
        case 0x07: return sh ? '&'  : '6';
        case 0x08: return sh ? '/'  : '7';
        case 0x09: return sh ? '('  : '8';
        case 0x0A: return sh ? ')'  : '9';
        case 0x0B: return sh ? '='  : '0';
        case 0x0C: return sh ? '?'  : '\'';
        case 0x0D: return sh ? 0xA8 : 0xAD;  /* ¿ : ¡ */
        case 0x1A: return sh ? '^'  : '`';
        case 0x1B: return sh ? '*'  : '+';
        case 0x27: return (sh ^ cp) ? 0xA5 : 0xA4;  /* Ñ : ñ */
        case 0x28: return sh ? '"'  : '\'';  /* acute key (dead keys: TODO) */
        case 0x29: return sh ? 0xA6 : 0xA7;  /* ª : º */
        case 0x2B: return (sh ^ cp) ? 0x80 : 0x87;  /* Ç : ç */
        case 0x33: return sh ? ';'  : ',';
        case 0x34: return sh ? ':'  : '.';
        case 0x35: return sh ? '_'  : '-';
    }
    return -2;  /* not an ES-specific key -> fall back to US map */
}

/* combine a dead accent with a base letter -> accented CP437 glyph (0 if none) */
static int combine_accent(int d, char base, bool up) {
    if (d == 1) {        /* acute ´ */
        switch (base) {
            case 'a': return up ? 0xB5 : 0xA0;  case 'e': return up ? 0x90 : 0x82;
            case 'i': return up ? 0xD6 : 0xA1;  case 'o': return up ? 0xE0 : 0xA2;
            case 'u': return up ? 0xE9 : 0xA3;
        }
    } else if (d == 2) { /* diaeresis ¨ */
        if (base == 'u') return up ? 0x9A : 0x81;   /* ü / Ü */
    }
    return 0;
}

static void push(int c) {
    int n = (head + 1) % BUF_SIZE;
    if (n != tail) { buf[head] = c; head = n; }
}

static void kb_callback(registers_t *r) {
    (void)r;
    uint8_t sc = inb(0x60);

    if (sc == 0xE0) { ext = true; return; }

    if (ext) {                       /* extended scancodes (arrows, right alt…) */
        ext = false;
        bool rel = (sc & 0x80) != 0;
        uint8_t code = sc & 0x7F;
        if (code == 0x38) { altgr = !rel; return; }   /* AltGr = right Alt */
        if (!rel) {
            switch (code) {
                case 0x48: push(KEY_UP);    return;
                case 0x50: push(KEY_DOWN);  return;
                case 0x4B: push(KEY_LEFT);  return;
                case 0x4D: push(KEY_RIGHT); return;
            }
        }
        return;
    }

    if (sc & 0x80) {                 /* key release */
        uint8_t code = sc & 0x7F;
        if (code == 0x2A || code == 0x36) shift = false;
        return;
    }

    if (sc == 0x2A || sc == 0x36) { shift = true; return; }
    if (sc == 0x3A) { caps = !caps; return; }

    if (sc < 128) {
        if (layout == 1) {
            if (sc == 0x28 && !altgr) { dead = shift ? 2 : 1; return; }  /* dead accent key */
            if (dead) {
                int d = dead; dead = 0;
                int comb = combine_accent(d, map_lower[sc], shift ^ caps);
                if (comb) { push(comb); return; }
                /* not a vowel: accent dropped, process the key normally */
            }
            int es = es_lookup(sc, shift, caps, altgr);
            if (es != -2) { if (es) push((int)(unsigned char)es); return; }
            /* fall through to US handling for letters */
        }
        char base = map_lower[sc];
        char c;
        if (base >= 'a' && base <= 'z')
            c = (shift ^ caps) ? map_upper[sc] : map_lower[sc];
        else
            c = shift ? map_upper[sc] : map_lower[sc];
        if (c) push((int)(unsigned char)c);
    }
}

void keyboard_init(void) {
    head = tail = 0;
    shift = caps = ext = altgr = false;
    layout = 0; dead = 0;
    irq_install_handler(1, kb_callback);
    while (inb(0x64) & 1) (void)inb(0x60);
}

void keyboard_set_layout(int es) { layout = es ? 1 : 0; }
int  keyboard_layout(void)       { return layout; }

bool keyboard_has_input(void) { return head != tail; }

int keyboard_getc_nonblock(void) {
    if (head == tail) return -1;
    int c = buf[tail];
    tail = (tail + 1) % BUF_SIZE;
    return c;
}
