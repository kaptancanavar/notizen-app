/* gfx.c - Zeichnen auf den Bildschirm ueber FBInk */
#include "app.h"

static uint8_t samp[200 * 80];

void put_gray(const uint8_t *buf, int w, int h, int x, int y) {
    fbink_print_raw_data(fbfd, (unsigned char *)buf, w, h, (size_t)w * h, x, y, &draw_cfg);
}

static void blit_raw(Rect r) {
    if (r.w <= 0 || r.h <= 0) return;
    for (int y = 0; y < r.h; y++)
        memcpy(tmp + (size_t)y * r.w, canvas + (size_t)(r.t + y) * W + r.l, r.w);
    put_gray(tmp, r.w, r.h, r.l, r.t);
}

/* zeichnet r, spart dabei die Rechtecke in ex aus */
static void blit_excl(Rect r, const Rect *ex, int n) {
    if (r.w <= 0 || r.h <= 0) return;
    if (n == 0) { blit_raw(r); return; }
    Rect e = ex[0];
    int il = imax(r.l, e.l), it = imax(r.t, e.t);
    int ir = imin(r.l + r.w, e.l + e.w), ib = imin(r.t + r.h, e.t + e.h);
    if (il >= ir || it >= ib) { blit_excl(r, ex + 1, n - 1); return; }
    blit_excl((Rect){ r.l, r.t, r.w, it - r.t }, ex + 1, n - 1);
    blit_excl((Rect){ r.l, ib, r.w, r.t + r.h - ib }, ex + 1, n - 1);
    blit_excl((Rect){ r.l, it, il - r.l, ib - it }, ex + 1, n - 1);
    blit_excl((Rect){ ir, it, r.l + r.w - ir, ib - it }, ex + 1, n - 1);
}

void blit(Rect r) {
    Rect ex[4];
    int n = 0;
    ex[n++] = tb_rect();
    ex[n++] = ind_rect();
    if (bar_visible()) ex[n++] = sel_bar_rect();
    if (popup != POP_ZU) ex[n++] = pop_rect();
    blit_excl(clip(r), ex, n);
}

void refresh(Rect r, const FBInkConfig *c) {
    r = clip(r);
    if (r.w > 0 && r.h > 0) fbink_refresh(fbfd, r.t, r.l, r.w, r.h, c);
}

void fill(Rect r, uint8_t g) {
    r = clip(r);
    if (r.w <= 0 || r.h <= 0) return;
    FBInkRect rc = { .left = r.l, .top = r.t, .width = r.w, .height = r.h };
    fbink_fill_rect_gray(fbfd, &draw_cfg, &rc, false, g);
}

void frame(Rect r, int b, uint8_t g) {
    fill((Rect){ r.l, r.t, r.w, b }, g);
    fill((Rect){ r.l, r.t + r.h - b, r.w, b }, g);
    fill((Rect){ r.l, r.t, b, r.h }, g);
    fill((Rect){ r.l + r.w - b, r.t, b, r.h }, g);
}

void icon(const char *name, bool aktiv, Rect r) {
    char p[256];
    snprintf(p, sizeof p, APP_DIR "/icons/%s%s.png", name, aktiv ? "_aktiv" : "");
    fbink_print_image(fbfd, p, r.l + (r.w - ICON) / 2, r.t + (r.h - ICON) / 2, &draw_cfg);
}

void text(const char *s, int size, Rect area, int top, bool center, int left_pad) {
    FBInkOTConfig ot = {0};
    ot.size_px = size;
    ot.is_centered = center;
    ot.margins.top = top;
    ot.margins.left = area.l + left_pad;
    ot.margins.right = W - (area.l + area.w);
    fbink_print_ot(fbfd, s, &ot, &draw_cfg, NULL);
}

void draw_sample(Rect box, int r, uint8_t g, bool outline) {
    int w = box.w, h = box.h;
    int x0 = r + 6, x1 = w - r - 6, cy = h / 2;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int cx = x < x0 ? x0 : (x > x1 ? x1 : x);
            int dx = x - cx, dy = y - cy, d2 = dx * dx + dy * dy;
            uint8_t v = 0xFF;
            if (d2 <= r * r) v = outline ? (d2 >= (r - 3) * (r - 3) ? 0x00 : 0xFF) : g;
            samp[y * w + x] = v;
        }
    }
    put_gray(samp, w, h, box.l, box.t);
}
