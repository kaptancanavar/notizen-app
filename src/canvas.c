/* canvas.c - Papier, Pinsel und Striche auf der Leinwand */
#include "app.h"

static int d_l, d_t, d_r, d_b;   /* geaenderter Bereich seit dem letzten flush */

static void mark(int l, int t, int r, int b) {
    if (!dirty) { d_l = l; d_t = t; d_r = r; d_b = b; dirty = true; return; }
    d_l = imin(d_l, l); d_t = imin(d_t, t);
    d_r = imax(d_r, r); d_b = imax(d_b, b);
}

static void hline(int y, int th, uint8_t g) {
    for (int k = 0; k < th; k++)
        if (y + k >= 0 && y + k < H) memset(bg + (size_t)(y + k) * W, g, W);
}

void render_bg(void) {
    memset(bg, 0xFF, (size_t)W * H);
    int top = PAPER_TOP, bottom = H - PAPER_BOTTOM;
    if (paper == P_LINIERT) {
        for (int y = top; y < bottom; y += LINIEN_ABST) hline(y, 2, LINIEN_GRAU);
    } else if (paper == P_KARIERT) {
        int last = top;
        for (int y = top; y < bottom; y += KARO_ABST) { hline(y, 1, KARO_GRAU); last = y; }
        for (int y = top; y <= last; y++)
            for (int x = KARO_ABST / 2; x < W; x += KARO_ABST)
                bg[(size_t)y * W + x] = KARO_GRAU;
    } else if (paper == P_PUNKTE) {
        for (int y = top; y < bottom; y += KARO_ABST)
            for (int x = KARO_ABST / 2; x < W - 2; x += KARO_ABST)
                for (int dy = -2; dy < 2; dy++)
                    for (int dx = -2; dx < 2; dx++)
                        bg[(size_t)(y + dy) * W + x + dx] = PUNKT_GRAU;
    }
}

static void brush(int cx, int cy, int r, int tl, int p) {
    int r2 = r * r + r;
    int x0 = imax(cx - r, 0), x1 = imin(cx + r, W - 1);
    int y0 = imax(cy - r, 0), y1 = imin(cy + r, H - 1);
    if (x0 > x1 || y0 > y1) return;
    uint8_t pencil = (uint8_t)(0xA0 - p * 0x80 / 255);
    for (int y = y0; y <= y1; y++) {
        uint8_t *row = canvas + (size_t)y * W;
        uint8_t *bgr = bg + (size_t)y * W;
        int dy = y - cy;
        for (int x = x0; x <= x1; x++) {
            int dx = x - cx;
            if (dx * dx + dy * dy > r2) continue;
            switch (tl) {
                case T_STIFT:
                case T_FUELLER:
                    row[x] = 0x00;
                    break;
                case T_BLEISTIFT:
                    if ((((unsigned)x * 73856093u) ^ ((unsigned)y * 19349663u)) % 4 == 0) break;
                    if (row[x] > pencil) row[x] = pencil;
                    break;
                case T_MARKER:
                    if (row[x] > MARKER_GRAU) row[x] = MARKER_GRAU;
                    break;
                default:
                    row[x] = bgr[x];
            }
        }
    }
    mark(x0, y0, x1, y1);
}

void seg(int x0, int y0, int x1, int y1, int r, int tl, int p) {
    int dx = x1 - x0, dy = y1 - y0;
    int steps = imax(iabs(dx), iabs(dy));
    if (steps == 0) { brush(x0, y0, r, tl, p); return; }
    int step = r > 3 ? r / 2 : 1;
    for (int i = 0; i <= steps; i += step)
        brush(x0 + dx * i / steps, y0 + dy * i / steps, r, tl, p);
    brush(x1, y1, r, tl, p);
}

void draw_stroke(const Stroke *s, int ox, int oy) {
    for (int i = 0; i < s->count; i++) {
        Point *q = &points[s->start + i];
        if (i == 0) brush(q->x + ox, q->y + oy, q->r, s->tool, q->p);
        else seg(q[-1].x + ox, q[-1].y + oy, q->x + ox, q->y + oy, q->r, s->tool, q->p);
    }
}

void render_canvas(void) {
    render_bg();
    memcpy(canvas, bg, (size_t)W * H);
    for (int i = 0; i < n_strokes; i++) draw_stroke(&strokes[i], 0, 0);
    dirty = false;
}

Rect stroke_box(const Stroke *s) {
    if (s->count == 0) return (Rect){ 0, 0, 0, 0 };
    int l = W, t = H, r = 0, b = 0;
    for (int i = 0; i < s->count; i++) {
        Point *q = &points[s->start + i];
        l = imin(l, q->x - q->r); t = imin(t, q->y - q->r);
        r = imax(r, q->x + q->r); b = imax(b, q->y + q->r);
    }
    return (Rect){ l - 2, t - 2, r - l + 5, b - t + 5 };
}

void flush(const FBInkConfig *c) {
    if (!dirty) return;
    Rect r = { d_l, d_t, d_r - d_l + 1, d_b - d_t + 1 };
    blit(r);
    refresh(r, c);
    dirty = false;
}

void full_redraw(void) {
    render_canvas();
    blit((Rect){ 0, 0, W, H });
    draw_overlays();
    refresh((Rect){ 0, 0, W, H }, &full_cfg);
}

void redraw_page_soft(void) {
    render_canvas();
    blit((Rect){ 0, 0, W, H });
    draw_overlays();
    refresh((Rect){ 0, 0, W, H }, &ui_cfg);
}
