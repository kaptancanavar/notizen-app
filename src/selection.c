/* selection.c - Lasso, Auswahl, Verschieben und Zwischenablage */
#include "app.h"

typedef struct { short x, y; } LP;

static const char *bar_text[3] = { "Ausschneiden", "Kopieren", "Löschen" };

/* Lasso */
static LP  *lasso; static int n_lasso, cap_lasso;
static Rect ls_box;
static int  ls_lx, ls_ly;

/* Verschieben */
static int  dg_x0, dg_y0, dg_lx, dg_ly;
static long dg_t;

/* Zwischenablage */
static Stroke *clip_s; static Point *clip_p;
static int clip_ns, clip_np, paste_off;

/* ---------- Geometrie ---------- */

Rect sel_frame(void) {
    return (Rect){ sel_box.l - 12, sel_box.t - 12, sel_box.w + 24, sel_box.h + 24 };
}

Rect sel_bar_rect(void) {
    Rect f = sel_frame();
    int t = f.t + f.h + 20;
    if (t + BAR_H > H - 120) t = f.t - BAR_H - 20;
    if (t < 220) t = 220;
    int l = f.l + f.w / 2 - BAR_W / 2;
    l = imax(TB_X + PANEL_W + 20, imin(l, W - BAR_W - 20));
    return (Rect){ l, t, BAR_W, BAR_H };
}

bool bar_visible(void) { return sel_active && !dragging; }

/* ---------- Zeichnen ---------- */

static void dashed_h(int x0, int x1, int y) {
    for (int x = x0; x < x1; x += 26) fill((Rect){ x, y, imin(16, x1 - x), 3 }, 0x00);
}
static void dashed_v(int x, int y0, int y1) {
    for (int y = y0; y < y1; y += 26) fill((Rect){ x, y, 3, imin(16, y1 - y) }, 0x00);
}

void draw_selection(void) {
    if (!sel_active) return;
    Rect f = sel_frame();
    f.l += dg_dx;
    f.t += dg_dy;
    dashed_h(f.l, f.l + f.w, f.t);
    dashed_h(f.l, f.l + f.w, f.t + f.h - 3);
    dashed_v(f.l, f.t, f.t + f.h);
    dashed_v(f.l + f.w - 3, f.t, f.t + f.h);
    if (!bar_visible()) return;
    Rect b = sel_bar_rect();
    fill(b, 0xFF);
    frame(b, 3, 0x00);
    int cw = b.w / 3;
    for (int i = 0; i < 3; i++) {
        Rect c = { b.l + i * cw, b.t, cw, b.h };
        if (i > 0) fill((Rect){ c.l, b.t + 15, 2, b.h - 30 }, 0xAA);
        text(bar_text[i], 38, c, b.t + 28, true, 0);
    }
}

/* ---------- Auswahl ---------- */

static void update_sel_box(void) {
    Rect b = { 0, 0, 0, 0 };
    for (int i = 0; i < n_strokes; i++)
        if (strokes[i].sel) b = unite(b, stroke_box(&strokes[i]));
    sel_box = b;
}

void sel_clear(bool redraw) {
    if (!sel_active) return;
    Rect a = unite(sel_frame(), sel_bar_rect());
    for (int i = 0; i < n_strokes; i++) strokes[i].sel = 0;
    sel_active = false;
    if (redraw) {
        blit(a);
        draw_overlays();
        refresh(a, &ui_cfg);
    }
}

static void sel_copy(void) {
    free(clip_s); free(clip_p);
    clip_s = NULL; clip_p = NULL;
    clip_ns = clip_np = 0;
    for (int i = 0; i < n_strokes; i++)
        if (strokes[i].sel) { clip_ns++; clip_np += strokes[i].count; }
    if (clip_ns == 0) return;
    clip_s = malloc((size_t)clip_ns * sizeof(Stroke));
    clip_p = malloc((size_t)imax(clip_np, 1) * sizeof(Point));
    if (!clip_s || !clip_p) {
        free(clip_s); free(clip_p);
        clip_s = NULL; clip_p = NULL;
        clip_ns = clip_np = 0;
        return;
    }
    int k = 0, np = 0;
    for (int i = 0; i < n_strokes; i++) {
        Stroke *s = &strokes[i];
        if (!s->sel) continue;
        clip_s[k++] = (Stroke){ np, s->count, s->tool, 0 };
        memcpy(clip_p + np, points + s->start, (size_t)s->count * sizeof(Point));
        np += s->count;
    }
    paste_off = 0;
}

static void delete_selected(void) {
    int ns = 0, np = 0;
    for (int i = 0; i < n_strokes; i++) {
        Stroke s = strokes[i];
        if (s.sel) continue;
        memmove(&points[np], &points[s.start], (size_t)s.count * sizeof(Point));
        s.start = np;
        np += s.count;
        strokes[ns++] = s;
    }
    n_strokes = ns;
    n_points = np;
}

static void sel_delete(void) {
    push_undo();
    delete_selected();
    sel_active = false;
    save_page();
    redraw_page_soft();
}

/* Tipp auf die Leiste "Ausschneiden | Kopieren | Löschen" */
void sel_bar_tap(int x) {
    Rect b = sel_bar_rect();
    int i = (x - b.l) * 3 / b.w;
    if (i == 0) { sel_copy(); sel_delete(); }
    else if (i == 1) { sel_copy(); sel_clear(true); }
    else sel_delete();
}

void paste(void) {
    if (clip_ns == 0) return;
    sel_clear(false);
    push_undo();
    /* jedes Einfuegen 40 px weiter versetzt, aber nie ueber den Rand hinaus */
    int maxx = 0, maxy = 0;
    for (int k = 0; k < clip_np; k++) {
        maxx = imax(maxx, clip_p[k].x + clip_p[k].r);
        maxy = imax(maxy, clip_p[k].y + clip_p[k].r);
    }
    paste_off += 40;
    if (maxx + paste_off >= W || maxy + paste_off >= H) paste_off = 0;
    ensure_strokes(n_strokes + clip_ns);
    ensure_points(n_points + clip_np);
    for (int i = 0; i < n_strokes; i++) strokes[i].sel = 0;
    for (int i = 0; i < clip_ns; i++) {
        Stroke s = clip_s[i];
        int start = n_points;
        for (int k = 0; k < s.count; k++) {
            Point q = clip_p[s.start + k];
            q.x = (short)imax(0, imin(W - 1, q.x + paste_off));
            q.y = (short)imax(0, imin(H - 1, q.y + paste_off));
            points[n_points++] = q;
        }
        strokes[n_strokes++] = (Stroke){ start, s.count, s.tool, 1 };
    }
    tool = T_LASSO;
    save_settings();
    sel_active = true;
    update_sel_box();
    save_page();
    redraw_page_soft();
}

/* ---------- Lasso ---------- */

static bool in_poly(int x, int y) {
    bool c = false;
    for (int i = 0, j = n_lasso - 1; i < n_lasso; j = i++) {
        int xi = lasso[i].x, yi = lasso[i].y, xj = lasso[j].x, yj = lasso[j].y;
        if ((yi > y) != (yj > y) && x < (long)(xj - xi) * (y - yi) / (yj - yi) + xi) c = !c;
    }
    return c;
}

void lasso_add(int x, int y) {
    if (n_lasso >= cap_lasso) {
        cap_lasso = cap_lasso ? cap_lasso * 2 : 1024;
        lasso = realloc(lasso, (size_t)cap_lasso * sizeof(LP));
    }
    lasso[n_lasso++] = (LP){ x, y };
    ls_box = unite(ls_box, (Rect){ x - 4, y - 4, 9, 9 });
    int dx = x - ls_lx, dy = y - ls_ly;
    if (n_lasso == 1 || dx * dx + dy * dy >= 14 * 14) {
        Rect d = { x - 2, y - 2, 5, 5 };
        fill(d, 0x00);
        refresh(d, &ref_cfg);
        ls_lx = x; ls_ly = y;
    }
}

void lasso_begin(int x, int y) {
    n_lasso = 0;
    ls_box = (Rect){ x - 4, y - 4, 9, 9 };
    lasso_add(x, y);
}

void lasso_end(void) {
    int cnt = 0;
    for (int i = 0; i < n_strokes; i++) {
        Stroke *s = &strokes[i];
        s->sel = 0;
        if (n_lasso < 3 || s->tool == T_RADIERER || s->count == 0) continue;
        if (!overlap(stroke_box(s), ls_box)) continue;
        int in = 0;
        for (int k = 0; k < s->count; k++)
            if (in_poly(points[s->start + k].x, points[s->start + k].y)) in++;
        if (in * 2 >= s->count) { s->sel = 1; cnt++; }
    }
    Rect area = ls_box;
    if (cnt) {
        sel_active = true;
        update_sel_box();
        area = unite(area, unite(sel_frame(), sel_bar_rect()));
    }
    blit(area);
    draw_overlays();
    refresh(area, &ui_cfg);
}

/* ---------- Verschieben ---------- */

/* Versatz so begrenzen, dass die Auswahl ganz auf dem Bildschirm bleibt */
static void clamp_offset(Rect box, int *dx, int *dy) {
    int lo_x = imin(0, -box.l), hi_x = imax(0, W - (box.l + box.w));
    int lo_y = imin(0, -box.t), hi_y = imax(0, H - (box.t + box.h));
    *dx = imax(lo_x, imin(hi_x, *dx));
    *dy = imax(lo_y, imin(hi_y, *dy));
}

void drag_begin(int x, int y) {
    dragging = true;
    dg_x0 = dg_lx = x; dg_y0 = dg_ly = y;
    dg_dx = dg_dy = 0;
    dg_t = 0;
    render_bg();
    memcpy(canvas, bg, (size_t)W * H);
    for (int i = 0; i < n_strokes; i++)
        if (!strokes[i].sel) draw_stroke(&strokes[i], 0, 0);
    memcpy(base, canvas, (size_t)W * H);
    for (int i = 0; i < n_strokes; i++)
        if (strokes[i].sel) draw_stroke(&strokes[i], 0, 0);
    dirty = false;
    Rect b = sel_bar_rect();
    blit(b);
    refresh(b, &ui_cfg);
}

void drag_update(int x, int y) {
    dg_lx = x; dg_ly = y;                  /* letzte Position merken, auch wenn gedrosselt */
    long t = now_ms();
    if (t - dg_t < DRAG_MS) return;
    dg_t = t;

    Rect f = sel_frame();
    Rect old = { f.l + dg_dx, f.t + dg_dy, f.w, f.h };
    dg_dx = x - dg_x0;
    dg_dy = y - dg_y0;
    clamp_offset(sel_box, &dg_dx, &dg_dy);
    Rect nw = { f.l + dg_dx, f.t + dg_dy, f.w, f.h };
    Rect u = clip(unite(old, nw));
    if (u.w <= 0 || u.h <= 0) return;

    for (int yy = u.t; yy < u.t + u.h; yy++)
        memcpy(canvas + (size_t)yy * W + u.l, base + (size_t)yy * W + u.l, u.w);
    for (int i = 0; i < n_strokes; i++)
        if (strokes[i].sel) draw_stroke(&strokes[i], dg_dx, dg_dy);
    dirty = false;

    blit(u);
    draw_selection();
    refresh(u, &ref_cfg);
}

/* verschiebt die ausgewaehlten Striche ans Ende der Liste (werden also zuletzt
 * gezeichnet, wie in der Vorschau beim Ziehen); Punkte bleiben aufsteigend sortiert */
static void selected_on_top(void) {
    Stroke *ns = malloc((size_t)imax(n_strokes, 1) * sizeof(Stroke));
    Point  *np = malloc((size_t)imax(n_points, 1) * sizeof(Point));
    if (!ns || !np) { free(ns); free(np); return; }
    int k = 0, p = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < n_strokes; i++) {
            Stroke s = strokes[i];
            if ((s.sel != 0) != (pass == 1)) continue;
            memcpy(np + p, points + s.start, (size_t)s.count * sizeof(Point));
            s.start = p;
            p += s.count;
            ns[k++] = s;
        }
    memcpy(strokes, ns, (size_t)k * sizeof(Stroke));
    memcpy(points, np, (size_t)p * sizeof(Point));
    n_strokes = k;
    n_points = p;
    free(ns); free(np);
}

void drag_end(void) {
    dbg("drag_end %d,%d", dg_lx - dg_x0, dg_ly - dg_y0);
    dragging = false;
    dg_dx = dg_lx - dg_x0;
    dg_dy = dg_ly - dg_y0;
    clamp_offset(sel_box, &dg_dx, &dg_dy);
    if (dg_dx != 0 || dg_dy != 0) {
        push_undo();
        selected_on_top();
        for (int i = 0; i < n_strokes; i++) {
            Stroke *s = &strokes[i];
            if (!s->sel) continue;
            for (int k = 0; k < s->count; k++) {
                Point *q = &points[s->start + k];
                q->x = (short)imax(0, imin(W - 1, q->x + dg_dx));
                q->y = (short)imax(0, imin(H - 1, q->y + dg_dy));
            }
        }
        update_sel_box();
        save_page();
    }
    dg_dx = dg_dy = 0;
    redraw_page_soft();
}
