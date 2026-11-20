/* keyboard.c - Bildschirmtastatur (QWERTZ) zum Eingeben von Namen */
#include "app.h"

#define KB_MARGIN   40
#define KB_KEY_H    140
#define KB_GAP      10
#define KB_FIELD_T  260
#define KB_FIELD_H  150
#define KB_MAX      63          /* passt in NB.name[64] */

enum { K_CHAR, K_SHIFT, K_BACK, K_SPACE, K_OK, K_CANCEL };
typedef struct { Rect r; int code; const char *lo, *up; } Key;

static const char *row_lo[3][11] = {
    { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "ß" },
    { "q", "w", "e", "r", "t", "z", "u", "i", "o", "p", "ü" },
    { "a", "s", "d", "f", "g", "h", "j", "k", "l", "ö", "ä" },
};
static const char *row_up[3][11] = {
    { "!", "\"", "§", "$", "%", "&", "/", "(", ")", "=", "?" },
    { "Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P", "Ü" },
    { "A", "S", "D", "F", "G", "H", "J", "K", "L", "Ö", "Ä" },
};
static const char *r4_lo[8] = { "y", "x", "c", "v", "b", "n", "m", "-" };
static const char *r4_up[8] = { "Y", "X", "C", "V", "B", "N", "M", "_" };

static Key  keys[48];
static int  n_keys;
static char buf[KB_MAX + 1];
static bool shift;
static int  kb_top;

/* Position in halben Tastenbreiten (eine Reihe = 22 halbe Breiten) */
static void add_key(int row, int start, int width, int code, const char *lo, const char *up) {
    int half = (W - 2 * KB_MARGIN) / 22;
    Key *k = &keys[n_keys++];
    k->r = (Rect){ KB_MARGIN + start * half, kb_top + row * (KB_KEY_H + KB_GAP),
                     width * half - KB_GAP, KB_KEY_H };
    k->code = code;
    k->lo = lo;
    k->up = up;
}

static void layout(void) {
    n_keys = 0;
    kb_top = H - KB_MARGIN - 5 * KB_KEY_H - 4 * KB_GAP;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 11; c++)
            add_key(r, c * 2, 2, K_CHAR, row_lo[r][c], row_up[r][c]);
    add_key(3, 0, 3, K_SHIFT, "Groß", "GROSS");
    for (int c = 0; c < 8; c++)
        add_key(3, 3 + c * 2, 2, K_CHAR, r4_lo[c], r4_up[c]);
    add_key(3, 19, 3, K_BACK, "Löschen", "Löschen");
    add_key(4, 0, 6, K_CANCEL, "Abbrechen", "Abbrechen");
    add_key(4, 6, 10, K_SPACE, "Leerzeichen", "Leerzeichen");
    add_key(4, 16, 6, K_OK, "OK", "OK");
}

static Rect field_rect(void) { return (Rect){ KB_MARGIN, KB_FIELD_T, W - 2 * KB_MARGIN, KB_FIELD_H }; }
static Rect keys_rect(void)  { return (Rect){ 0, kb_top, W, H - kb_top }; }

static void draw_field(void) {
    Rect f = field_rect();
    fill(f, 0xFF);
    frame(f, 4, 0x00);
    char s[KB_MAX + 4];
    snprintf(s, sizeof s, "%s|", buf);
    text(s, 60, f, f.t + 40, false, 30);
}

static void draw_keys(void) {
    fill(keys_rect(), 0xFF);
    for (int i = 0; i < n_keys; i++) {
        Key *k = &keys[i];
        bool aktiv = k->code == K_SHIFT && shift;
        frame(k->r, aktiv ? 8 : 3, 0x00);
        int size = k->code == K_CHAR ? 50 : 40;
        text(shift ? k->up : k->lo, size, k->r, k->r.t + (KB_KEY_H - size) / 2 - 6, true, 0);
    }
}

static void append(const char *s) {
    if (strlen(buf) + strlen(s) <= KB_MAX) strcat(buf, s);
}

/* letztes Zeichen loeschen, auch wenn es ein Umlaut (2 Bytes UTF-8) ist */
static void backspace(void) {
    size_t n = strlen(buf);
    while (n > 0 && ((unsigned char)buf[n - 1] & 0xC0) == 0x80) n--;
    if (n > 0) n--;
    buf[n] = 0;
}

void kb_start(const char *initial) {
    snprintf(buf, sizeof buf, "%s", initial);
    shift = false;
    layout();
}

void kb_draw(void) {
    fill((Rect){ 0, 0, W, H }, 0xFF);
    text("Notizbuch umbenennen", 60, (Rect){ 0, 0, W, 200 }, 90, true, 0);
    draw_field();
    draw_keys();
}

int kb_tap(int x, int y) {
    for (int i = 0; i < n_keys; i++) {
        Key *k = &keys[i];
        if (!inside(k->r, x, y)) continue;
        switch (k->code) {
            case K_OK:     return KB_OK;
            case K_CANCEL: return KB_CANCEL;
            case K_SHIFT:
                shift = !shift;
                draw_keys();
                refresh(keys_rect(), &ui_cfg);
                return KB_NONE;
            case K_BACK:  backspace(); break;
            case K_SPACE: append(" "); break;
            default:
                append(shift ? k->up : k->lo);
                if (shift) {                      /* Grossschreibung gilt fuer ein Zeichen */
                    shift = false;
                    draw_keys();
                    refresh(keys_rect(), &ui_cfg);
                }
        }
        draw_field();
        refresh(field_rect(), &ui_cfg);
        return KB_NONE;
    }
    return KB_NONE;
}

/* eingegebener Text ohne Leerzeichen am Anfang und Ende */
const char *kb_text(void) {
    static char out[KB_MAX + 1];
    const char *s = buf;
    while (*s == ' ') s++;
    snprintf(out, sizeof out, "%s", s);
    size_t n = strlen(out);
    while (n > 0 && out[n - 1] == ' ') out[--n] = 0;
    return out;
}
