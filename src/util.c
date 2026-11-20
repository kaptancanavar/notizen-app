/* util.c - Hilfsfunktionen und Einstellungen */
#include "app.h"

static int cap_points, cap_strokes;

long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

bool inside(Rect r, int x, int y) {
    return x >= r.l && x < r.l + r.w && y >= r.t && y < r.t + r.h;
}

Rect clip(Rect r) {
    int l = imax(r.l, 0), t = imax(r.t, 0);
    int rr = imin(r.l + r.w, W), b = imin(r.t + r.h, H);
    return (Rect){ l, t, imax(rr - l, 0), imax(b - t, 0) };
}

Rect unite(Rect a, Rect b) {
    if (a.w <= 0 || a.h <= 0) return b;
    if (b.w <= 0 || b.h <= 0) return a;
    int l = imin(a.l, b.l), t = imin(a.t, b.t);
    int r = imax(a.l + a.w, b.l + b.w), bt = imax(a.t + a.h, b.t + b.h);
    return (Rect){ l, t, r - l, bt - t };
}

bool overlap(Rect a, Rect b) {
    return a.l < b.l + b.w && b.l < a.l + a.w && a.t < b.t + b.h && b.t < a.t + a.h;
}

void ensure_strokes(int n) {
    if (n <= cap_strokes) return;
    while (cap_strokes < n) cap_strokes = cap_strokes ? cap_strokes * 2 : 256;
    Stroke *p = realloc(strokes, (size_t)cap_strokes * sizeof(Stroke));
    if (!p) { printf("Kein Speicher (Striche)\n"); exit(1); }
    strokes = p;
}

void ensure_points(int n) {
    if (n <= cap_points) return;
    while (cap_points < n) cap_points = cap_points ? cap_points * 2 : 4096;
    Point *p = realloc(points, (size_t)cap_points * sizeof(Point));
    if (!p) { printf("Kein Speicher (Punkte)\n"); exit(1); }
    points = p;
}

void load_settings(void) {
    FILE *f = fopen(SETTINGS, "r");
    if (!f) return;
    int a, b, c, d, e, g;
    int got = fscanf(f, "%d %d %d %d %d %d", &a, &b, &c, &d, &e, &g);
    if (got >= 5) {
        if (a == T_STIFT || a == T_MARKER || a == T_RADIERER || a == T_LASSO) tool = a;
        if (b == T_STIFT || b == T_FUELLER || b == T_BLEISTIFT) pen_kind = b;
        if (c >= 0 && c < 5) lvl[0] = c;
        if (d >= 0 && d < 5) lvl[1] = d;
        if (e >= 0 && e < 5) lvl[2] = e;
    }
    if (got == 6) shapes_on = g != 0;
    fclose(f);
}

void save_settings(void) {
    FILE *f = fopen(SETTINGS, "w");
    if (!f) return;
    fprintf(f, "%d %d %d %d %d %d\n", tool, pen_kind, lvl[0], lvl[1], lvl[2], shapes_on ? 1 : 0);
    fclose(f);
}

/* Protokoll nach stdout (run.sh leitet es nach log.txt um), mit Zeit in Sekunden */
void dbg(const char *fmt, ...) {
    static long t0;
    long t = now_ms();
    if (!t0) t0 = t;
    printf("[%7.2f] ", (t - t0) / 1000.0);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
}
