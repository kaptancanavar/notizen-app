/* shapes.c - Formerkennung (Linie, Kreis/Ellipse, Dreieck, Viereck) */
#include "app.h"

static Point *gp; static int gp_n, gp_cap;

static void gp_add(double x, double y, int r, int p) {
    if (gp_n >= gp_cap) {
        gp_cap = gp_cap ? gp_cap * 2 : 1024;
        gp = realloc(gp, (size_t)gp_cap * sizeof(Point));
    }
    int ix = (int)lround(x), iy = (int)lround(y);
    gp[gp_n++] = (Point){ (short)imax(0, imin(W - 1, ix)), (short)imax(0, imin(H - 1, iy)),
        (unsigned char)r, (unsigned char)p };
}

static void gp_seg(double x0, double y0, double x1, double y1, int r, int p, bool first) {
    int k = imax(1, (int)(hypot(x1 - x0, y1 - y0) / 6));
    for (int i = first ? 0 : 1; i <= k; i++) {
        double t = (double)i / k;
        gp_add(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, r, p);
    }
}

/* Ramer-Douglas-Peucker: markiert die Eckpunkte */
static void rdp(const double *xs, const double *ys, int a, int b, double eps, char *keep) {
    if (b <= a + 1) return;
    double dx = xs[b] - xs[a], dy = ys[b] - ys[a], L = hypot(dx, dy);
    double md = -1;
    int mi = -1;
    for (int i = a + 1; i < b; i++) {
        double d = L < 1e-6 ? hypot(xs[i] - xs[a], ys[i] - ys[a])
                            : fabs(dy * (xs[i] - xs[a]) - dx * (ys[i] - ys[a])) / L;
        if (d > md) { md = d; mi = i; }
    }
    if (md > eps) {
        keep[mi] = 1;
        rdp(xs, ys, a, mi, eps, keep);
        rdp(xs, ys, mi, b, eps, keep);
    }
}

/* versucht, den letzten Strich durch eine saubere Form zu ersetzen */
static bool try_shape(void) {
    Stroke *s = &strokes[n_strokes - 1];
    int n = s->count;
    if (n < 8) return false;
    Point *pt = &points[s->start];

    int minx = W, miny = H, maxx = 0, maxy = 0;
    long sr = 0, sp = 0;
    for (int i = 0; i < n; i++) {
        minx = imin(minx, pt[i].x); maxx = imax(maxx, pt[i].x);
        miny = imin(miny, pt[i].y); maxy = imax(maxy, pt[i].y);
        sr += pt[i].r; sp += pt[i].p;
    }
    int w = maxx - minx, h = maxy - miny, size = imax(w, h);
    if (size < 40) return false;
    int rr = imax(1, (int)(sr / n)), pp = (int)(sp / n);

    double *xs = malloc((size_t)n * sizeof(double));
    double *ys = malloc((size_t)n * sizeof(double));
    char *keep = calloc((size_t)n, 1);
    for (int i = 0; i < n; i++) { xs[i] = pt[i].x; ys[i] = pt[i].y; }
    gp_n = 0;
    bool ok = false;

    double sx = xs[0], sy = ys[0], ex = xs[n - 1], ey = ys[n - 1];
    double D = hypot(ex - sx, ey - sy);

    /* 1. Gerade Linie */
    if (D > 0.7 * size) {
        double md = 0;
        for (int i = 0; i < n; i++) {
            double d = fabs((ey - sy) * (xs[i] - sx) - (ex - sx) * (ys[i] - sy)) / D;
            if (d > md) md = d;
        }
        if (md < fmax(12.0, 0.06 * D)) {
            double dx = ex - sx, dy = ey - sy;
            if (fabs(dy) < 0.09 * fabs(dx)) ey = sy;          /* waagrecht */
            else if (fabs(dx) < 0.09 * fabs(dy)) ex = sx;     /* senkrecht */
            gp_seg(sx, sy, ex, ey, rr, pp, true);
            ok = true;
        }
    }

    /* 2. Geschlossene Formen */
    if (!ok && D < 0.3 * size) {
        double cx = (minx + maxx) / 2.0, cy = (miny + maxy) / 2.0;
        double rx = w / 2.0, ry = h / 2.0;

        /* Kreis / Ellipse */
        if (rx >= 10 && ry >= 10) {
            double e = 0;
            for (int i = 0; i < n; i++) {
                double dx = (xs[i] - cx) / rx, dy = (ys[i] - cy) / ry;
                e += fabs(sqrt(dx * dx + dy * dy) - 1);
            }
            e /= n;
            if (e < 0.09) {
                if (fabs(rx - ry) < 0.12 * fmax(rx, ry)) rx = ry = (rx + ry) / 2;
                int N = imax(36, (int)(M_PI * (rx + ry) / 6));
                for (int i = 0; i <= N; i++) {
                    double a = 2 * M_PI * i / N;
                    gp_add(cx + rx * cos(a), cy + ry * sin(a), rr, pp);
                }
                ok = true;
            }
        }

        /* Dreieck / Viereck */
        if (!ok) {
            keep[0] = keep[n - 1] = 1;
            rdp(xs, ys, 0, n - 1, fmax(8.0, 0.035 * (w + h)), keep);
            double vx[64], vy[64];
            int k = 0;
            for (int i = 0; i < n && k < 64; i++)
                if (keep[i]) { vx[k] = xs[i]; vy[k] = ys[i]; k++; }
            if (k > 1 && hypot(vx[k - 1] - vx[0], vy[k - 1] - vy[0]) < 0.3 * size) k--;

            /* fast gerade Ecken und zu kurze Kanten entfernen */
            bool changed = true;
            while (changed && k > 3) {
                changed = false;
                for (int i = 0; i < k; i++) {
                    int a = (i + k - 1) % k, b = (i + 1) % k;
                    double ux = vx[a] - vx[i], uy = vy[a] - vy[i];
                    double wx = vx[b] - vx[i], wy = vy[b] - vy[i];
                    double lu = hypot(ux, uy), lw = hypot(wx, wy);
                    if (lu < 0.12 * size || lw < 0.12 * size ||
                        (ux * wx + uy * wy) / (lu * lw + 1e-9) < -0.85) {
                        for (int j = i; j < k - 1; j++) { vx[j] = vx[j + 1]; vy[j] = vy[j + 1]; }
                        k--;
                        changed = true;
                        break;
                    }
                }
            }

            if (k == 4) {                      /* fast achsparallel -> echtes Rechteck */
                bool axis = true;
                for (int i = 0; i < 4; i++) {
                    double dx = fabs(vx[(i + 1) % 4] - vx[i]), dy = fabs(vy[(i + 1) % 4] - vy[i]);
                    if (fmin(dx, dy) > 0.2 * fmax(dx, dy)) axis = false;
                }
                if (axis) {
                    double l = vx[0], r = vx[0], t = vy[0], b = vy[0];
                    for (int i = 1; i < 4; i++) {
                        l = fmin(l, vx[i]); r = fmax(r, vx[i]);
                        t = fmin(t, vy[i]); b = fmax(b, vy[i]);
                    }
                    vx[0] = l; vy[0] = t; vx[1] = r; vy[1] = t;
                    vx[2] = r; vy[2] = b; vx[3] = l; vy[3] = b;
                }
            }
            if (k == 3 || k == 4) {
                for (int i = 0; i < k; i++)
                    gp_seg(vx[i], vy[i], vx[(i + 1) % k], vy[(i + 1) % k], rr, pp, i == 0);
                ok = true;
            }
        }
    }

    free(xs); free(ys); free(keep);
    if (!ok) return false;

    /* Strich durch die Form ersetzen */
    Rect old = stroke_box(s);
    n_points = s->start;
    ensure_points(n_points + gp_n);
    memcpy(points + n_points, gp, (size_t)gp_n * sizeof(Point));
    n_points += gp_n;
    strokes[n_strokes - 1].count = gp_n;
    Rect a = unite(old, stroke_box(&strokes[n_strokes - 1]));
    render_canvas();
    blit(a);
    refresh(a, &ui_cfg);
    return true;
}

bool hold_possible(void) {
    return shapes_on && screen == SCR_EDITOR && pen_mode == PM_DRAW && pen_down
        && !shaped && stroke_tool != T_RADIERER;
}

void check_hold(void) {
    if (!hold_possible() || now_ms() - anchor_t < HOLD_MS) return;
    dbg("Form pruefen");
    if (try_shape()) { shaped = true; dbg("Form erkannt"); }
    else anchor_t = now_ms();                    /* nichts erkannt: weiterzeichnen */
}
