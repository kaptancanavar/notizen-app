/* history.c - Strich-Daten, Rueckgaengig und Wiederholen */
#include "app.h"

typedef struct { Stroke *s; Point *p; int ns, np; } Snap;

static Snap h_undo[HIST], h_redo[HIST];
static int  n_undo, n_redo;

static Snap snap_take(void) {
    Snap s = { NULL, NULL, n_strokes, n_points };
    if (s.ns) s.s = malloc((size_t)s.ns * sizeof(Stroke));
    if (s.np) s.p = malloc((size_t)s.np * sizeof(Point));
    if ((s.ns && !s.s) || (s.np && !s.p)) { printf("Kein Speicher (Verlauf)\n"); exit(1); }
    if (s.ns) memcpy(s.s, strokes, (size_t)s.ns * sizeof(Stroke));
    if (s.np) memcpy(s.p, points,  (size_t)s.np * sizeof(Point));
    return s;
}

static void snap_free(Snap *s) { free(s->s); free(s->p); s->s = NULL; s->p = NULL; }

static void snap_restore(const Snap *s) {
    ensure_strokes(s->ns);
    ensure_points(s->np);
    if (s->ns) memcpy(strokes, s->s, (size_t)s->ns * sizeof(Stroke));
    if (s->np) memcpy(points, s->p, (size_t)s->np * sizeof(Point));
    n_strokes = s->ns;
    n_points = s->np;
    for (int i = 0; i < n_strokes; i++) strokes[i].sel = 0;
}

static void stack_push(Snap *st, int *n, Snap s) {
    if (*n == HIST) {
        snap_free(&st[0]);
        memmove(st, st + 1, (HIST - 1) * sizeof(Snap));
        (*n)--;
    }
    st[(*n)++] = s;
}

static void clear_stack(Snap *st, int *n) { while (*n) snap_free(&st[--(*n)]); }

void clear_history(void) {
    clear_stack(h_undo, &n_undo);
    clear_stack(h_redo, &n_redo);
}

void push_undo(void) {
    stack_push(h_undo, &n_undo, snap_take());
    clear_stack(h_redo, &n_redo);
}

void begin_stroke(int tl) {
    push_undo();
    ensure_strokes(n_strokes + 1);
    strokes[n_strokes++] = (Stroke){ n_points, 0, (unsigned char)tl, 0 };
}

void add_point(int x, int y, int r, int p) {
    ensure_points(n_points + 1);
    points[n_points++] = (Point){ x, y, r, p };
    strokes[n_strokes - 1].count++;
}

void do_undo(void) {
    if (n_undo == 0) return;
    sel_clear(false);
    stack_push(h_redo, &n_redo, snap_take());
    Snap s = h_undo[--n_undo];
    snap_restore(&s);
    snap_free(&s);
    save_page();
    redraw_page_soft();
}

void do_redo(void) {
    if (n_redo == 0) return;
    sel_clear(false);
    stack_push(h_undo, &n_undo, snap_take());
    Snap s = h_redo[--n_redo];
    snap_restore(&s);
    snap_free(&s);
    save_page();
    redraw_page_soft();
}
