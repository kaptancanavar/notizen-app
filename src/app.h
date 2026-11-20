/* app.h - gemeinsame Definitionen der Notizen-App */
#ifndef APP_H
#define APP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <signal.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/input.h>
#include "fbink.h"

/* ---------- Geraet ---------- */
#define PEN_DEV     "/dev/input/event3"
#define TOUCH_DEV   "/dev/input/event4"
#define PEN_MAX_X   15624
#define PEN_MAX_Y   20832
#define PEN_MAX_P   4095
#define TIMEOUT_MS  600000
#define PEN_STALE_MS 1000
#define APP_DIR     "/mnt/us/notizen-app"
#define NOTE_DIR    "/mnt/us/notizen"
#define SETTINGS    APP_DIR "/einstellungen"

/* ---------- Werkzeuge (Werte 0-4 werden in Dateien gespeichert!) ---------- */
enum { T_STIFT = 0, T_MARKER = 1, T_RADIERER = 2, T_FUELLER = 3, T_BLEISTIFT = 4, T_LASSO = 5 };
#define MARKER_GRAU 0xC0

/* ---------- Formerkennung ---------- */
#define HOLD_MS   600
#define HOLD_TOL  10

/* ---------- Papier ---------- */
enum { P_LEER, P_LINIERT, P_KARIERT, P_PUNKTE, P_ANZAHL };
#define PAPER_TOP     220
#define PAPER_BOTTOM  150
#define LINIEN_ABST   88
#define LINIEN_GRAU   0xB0
#define KARO_ABST     60
#define KARO_GRAU     0xC8
#define PUNKT_GRAU    0x80

/* ---------- Werkzeugleiste ---------- */
enum { I_KLAPPEN, I_STIFT, I_MARKER, I_RADIERER, I_LASSO,
    I_EINFUEGEN, I_UNDO, I_REDO, I_MENU, I_ANZAHL };
#define TB_X     30
#define TB_Y     200
#define BTN      104
#define ICON     80
#define PAD      10
#define GAP      16
#define SEP      24
#define PANEL_W  (BTN + 2 * PAD)
#define PANEL_H  (2 * PAD + (I_ANZAHL - 1) * BTN + SEP)

/* ---------- Popups ---------- */
enum { POP_ZU, POP_MENU, POP_PAPIER, POP_STIFT, POP_MARKER, POP_RADIERER };
enum { M_NEU, M_ZURUECK, M_WEITER, M_PAPIER, M_LEEREN, M_FORMEN, M_UEBERSICHT, M_ANZAHL };
#define POP_W   600
#define ROW_H   100
#define SEP_H   24

/* ---------- Auswahl ---------- */
#define BAR_W   750
#define BAR_H   100
#define HIST    30
#define DRAG_MS 60

/* ---------- Seitenanzeige ---------- */
#define IND_W  300
#define IND_H  80

/* ---------- Uebersicht ---------- */
#define OV_TOP      230
#define OV_MARGIN   60
#define OV_COLS     3
#define OV_ROWS     3
#define OV_PER      (OV_COLS * OV_ROWS)
#define OV_GAP_Y    40
#define TH_SCALE    5
#define CARD_LABEL  110
#define LANG_MS     600
#define DLG_W       900
#define DLG_TITLE   150
#define DLG_ITEM    120

enum { SCR_UEBERSICHT, SCR_EDITOR };
enum { PM_NONE, PM_DRAW, PM_LASSO, PM_DRAG };

/* ---------- Typen ---------- */
typedef struct { int l, t, w, h; } Rect;
typedef struct { short x, y; unsigned char r, p; } Point;
typedef struct { int start, count; unsigned char tool, sel; } Stroke;
typedef struct { char id[40]; char name[64]; long created; time_t mtime; int pages; } NB;

/* ---------- Kleine Helfer ---------- */
static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }
static inline int iabs(int a) { return a < 0 ? -a : a; }

/* ---------- Globaler Zustand (definiert in main.c) ---------- */
extern int fbfd, W, H, TW, TH;
extern uint8_t *canvas, *bg, *tmp, *thumbs, *base;
extern FBInkConfig draw_cfg, ref_cfg, gray_cfg, ui_cfg, full_cfg;

extern Point  *points;  extern int n_points;
extern Stroke *strokes; extern int n_strokes;
extern int cur_page, n_pages, paper;

extern NB  *nbs; extern int n_nbs;
extern char cur_nb[40];

extern int  screen, tool, pen_kind, lvl[3], popup;
extern bool shapes_on, tb_open, run, dirty;

extern bool sel_active, dragging, finger_drag;
extern Rect sel_box;
extern int  dg_dx, dg_dy;

extern int  pen_mode, stroke_tool;
extern bool pen_down;
extern int  anchor_x, anchor_y;
extern long anchor_t;
extern bool shaped;

/* ---------- util.c ---------- */
long now_ms(void);
bool inside(Rect r, int x, int y);
Rect clip(Rect r);
Rect unite(Rect a, Rect b);
bool overlap(Rect a, Rect b);
void ensure_strokes(int n);
void ensure_points(int n);
void load_settings(void);
void save_settings(void);
void dbg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* ---------- gfx.c ---------- */
void put_gray(const uint8_t *buf, int w, int h, int x, int y);
void blit(Rect r);
void refresh(Rect r, const FBInkConfig *c);
void fill(Rect r, uint8_t g);
void frame(Rect r, int b, uint8_t g);
void icon(const char *name, bool aktiv, Rect r);
void text(const char *s, int size, Rect area, int top, bool center, int left_pad);
void draw_sample(Rect box, int r, uint8_t g, bool outline);

/* ---------- canvas.c ---------- */
void render_bg(void);
void seg(int x0, int y0, int x1, int y1, int r, int tl, int p);
void draw_stroke(const Stroke *s, int ox, int oy);
void render_canvas(void);
Rect stroke_box(const Stroke *s);
void flush(const FBInkConfig *c);
void full_redraw(void);
void redraw_page_soft(void);

/* ---------- history.c ---------- */
void clear_history(void);
void push_undo(void);
void begin_stroke(int tl);
void add_point(int x, int y, int r, int p);
void do_undo(void);
void do_redo(void);

/* ---------- shapes.c ---------- */
bool hold_possible(void);
void check_hold(void);

/* ---------- storage.c ---------- */
void save_page(void);
void load_page(int page);
int  count_pages(void);
int  read_pos(void);
void write_pos(void);
void write_name(const char *id, const char *name);
int  next_number(void);
void scan_notebooks(void);
void delete_notebook(int idx);

/* ---------- selection.c ---------- */
Rect sel_frame(void);
Rect sel_bar_rect(void);
bool bar_visible(void);
void draw_selection(void);
void sel_clear(bool redraw);
void sel_bar_tap(int x);
void paste(void);
void lasso_begin(int x, int y);
void lasso_add(int x, int y);
void lasso_end(void);
void drag_begin(int x, int y);
void drag_update(int x, int y);
void drag_end(void);

/* ---------- editor.c ---------- */
Rect tb_rect(void);
Rect ind_rect(void);
Rect pop_rect(void);
void draw_overlays(void);
bool ui_hit(int x, int y);
void ui_tap(int x, int y);
void reset_page_state(void);
void enter_editor(void);
void new_notebook(int p);
void go_page(int p);

/* ---------- overview.c ---------- */
void ov_redraw(bool thumbs_neu, const FBInkConfig *c);
void ov_tap(int x, int y, bool lang);
void ov_swipe(int dx);
void leave_editor(void);

/* ---------- keyboard.c ---------- */
enum { KB_NONE, KB_OK, KB_CANCEL };
void kb_start(const char *initial);
void kb_draw(void);
int  kb_tap(int x, int y);
const char *kb_text(void);

/* ---------- input.c ---------- */
void pen_event(const struct input_event *ev);
void touch_event(const struct input_event *ev);
void pen_resync(int fd);
bool pen_stale(void);
bool pen_busy(void);
void touch_resync(int fd);

#endif
