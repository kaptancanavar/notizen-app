/* input.c - Stift- und Touch-Eingaben */
#include "app.h"

static const int stift_r[5]    = { 1, 2, 3, 5, 7 };
static const int marker_r[5]   = { 8, 12, 16, 22, 30 };
static const int radierer_r[5] = { 8, 14, 22, 32, 45 };

/* Stift */
static int  rx, ry, rp, px, py;
static bool pen_was_down, rubber, pen_near;
static long pen_last_t;                   /* Zeit des letzten Stift-Ereignisses */

/* Finger */
static int  t_slot, tx, ty, tx0, ty0;
static bool t_active, t_new;
static long t_start;

static int radius(int tl) {
    switch (tl) {
        case T_MARKER:    return marker_r[lvl[1]];
        case T_RADIERER:  return radierer_r[lvl[2]];
        case T_BLEISTIFT: return stift_r[lvl[0]] + 1;
        case T_FUELLER:   return imax(1, stift_r[lvl[0]] * (20 + 180 * rp / PEN_MAX_P) / 100);
        default:          return imax(1, stift_r[lvl[0]] * (60 + 80 * rp / PEN_MAX_P) / 100);
    }
}

/* ================= Stift ================= */

/* Stift gilt als unten/nah, meldet sich aber seit ueber 1 s nicht mehr?
 * Ein echter Stift sendet in dem Zustand laufend Daten -> Zustand ist veraltet */
bool pen_stale(void) {
    return (pen_down || pen_near) && now_ms() - pen_last_t > PEN_STALE_MS;
}

bool pen_busy(void) { return pen_down || pen_near; }

void pen_event(const struct input_event *ev) {
    pen_last_t = now_ms();
    if (ev->type == EV_ABS) {
        if (ev->code == ABS_X) rx = ev->value;
        else if (ev->code == ABS_Y) ry = ev->value;
        else if (ev->code == ABS_PRESSURE) rp = imax(0, imin(PEN_MAX_P, ev->value));
        return;
    }
    if (ev->type == EV_KEY) {
        if (ev->code == BTN_TOUCH) pen_down = ev->value;
        else if (ev->code == BTN_TOOL_RUBBER) { rubber = ev->value; pen_near = ev->value; }
        else if (ev->code == BTN_TOOL_PEN) pen_near = ev->value;
        return;
    }
    if (ev->type != EV_SYN || ev->code != SYN_REPORT) return;

    int x = rx * W / (PEN_MAX_X + 1);
    int y = ry * H / (PEN_MAX_Y + 1);

    if (screen == SCR_UEBERSICHT) {
        if (pen_down && !pen_was_down) { dbg("Stift ab (Uebersicht) x=%d y=%d", x, y); ov_tap(x, y, false); }
        pen_was_down = pen_down;
        return;
    }

    if (pen_down && !pen_was_down) dbg("Stift ab x=%d y=%d tool=%d%s%s", x, y, tool, rubber ? " radierer" : "", finger_drag ? " (finger_drag!)" : "");
    if (pen_down && !pen_was_down && !finger_drag) {                 /* aufgesetzt */
        if (ui_hit(x, y)) {
            pen_mode = PM_NONE;
            ui_tap(x, y);
        } else if (tool == T_LASSO && !rubber) {
            if (sel_active && inside(sel_frame(), x, y)) { pen_mode = PM_DRAG; drag_begin(x, y); }
            else { sel_clear(true); pen_mode = PM_LASSO; lasso_begin(x, y); }
        } else {
            sel_clear(true);
            pen_mode = PM_DRAW;
            stroke_tool = rubber ? T_RADIERER : (tool == T_STIFT ? pen_kind : tool);
            begin_stroke(stroke_tool);
            px = x; py = y;
            anchor_x = x; anchor_y = y;
            anchor_t = now_ms();
            shaped = false;
        }
    }

    if (pen_down) {
        if (pen_mode == PM_DRAG) drag_update(x, y);
        else if (pen_mode == PM_LASSO) lasso_add(x, y);
        else if (pen_mode == PM_DRAW && !shaped) {
            int r = radius(stroke_tool);
            int p = rp * 255 / PEN_MAX_P;
            seg(px, py, x, y, r, stroke_tool, p);
            add_point(x, y, r, p);
            px = x; py = y;
            bool sw = stroke_tool == T_STIFT || stroke_tool == T_FUELLER;
            flush(sw ? &ref_cfg : &gray_cfg);
            int ax = x - anchor_x, ay = y - anchor_y;
            if (ax * ax + ay * ay > HOLD_TOL * HOLD_TOL) {
                anchor_x = x; anchor_y = y;
                anchor_t = now_ms();
            }
            check_hold();
        }
    }

    if (!pen_down && pen_was_down) {                                  /* abgehoben */
        dbg("Stift hoch mode=%d striche=%d punkte=%d", pen_mode, n_strokes, n_points);
        if (pen_mode == PM_DRAG) drag_end();
        else if (pen_mode == PM_LASSO) lasso_end();
        else if (pen_mode == PM_DRAW) save_page();
        pen_mode = PM_NONE;
    }
    pen_was_down = pen_down;
}

/* ================= Finger ================= */

static void touch_end(void) {
    dbg("Finger hoch %d,%d -> %d,%d drag=%d", tx0, ty0, tx, ty, finger_drag);
    if (finger_drag) { finger_drag = false; drag_end(); return; }
    if (pen_near || pen_down) {
        dbg("Fingertipp ignoriert (Stift %s)", pen_down ? "unten" : "nah");
        return;
    }
    int dx = tx - tx0, dy = ty - ty0;
    bool swipe = iabs(dx) > 250 && iabs(dx) > 2 * iabs(dy);
    bool tap = iabs(dx) < 40 && iabs(dy) < 40;

    if (screen == SCR_EDITOR) {
        if (swipe && popup == POP_ZU) {
            if (dx < 0 && cur_page < n_pages) go_page(cur_page + 1);
            else if (dx > 0 && cur_page > 1) go_page(cur_page - 1);
        } else if (tap && ui_hit(tx0, ty0)) {
            ui_tap(tx0, ty0);
        } else if (tap && sel_active) {
            sel_clear(true);
        }
        return;
    }

    if (swipe) ov_swipe(dx);
    else if (tap) ov_tap(tx0, ty0, now_ms() - t_start >= LANG_MS);
}

void touch_event(const struct input_event *ev) {
    if (ev->type == EV_ABS) {
        if (ev->code == ABS_MT_SLOT) { t_slot = ev->value; return; }
        if (t_slot != 0) return;
        if (ev->code == ABS_MT_POSITION_X) tx = ev->value;
        else if (ev->code == ABS_MT_POSITION_Y) ty = ev->value;
        else if (ev->code == ABS_MT_TRACKING_ID) {
            if (ev->value >= 0) { t_active = true; t_new = true; }
            else if (t_active) { t_active = false; touch_end(); }
        }
    } else if (ev->type == EV_SYN && ev->code == SYN_REPORT) {
        if (t_new) {
            tx0 = tx; ty0 = ty;
            t_start = now_ms();
            t_new = false;
            if (screen == SCR_EDITOR && sel_active && pen_mode == PM_NONE && !pen_near
                && popup == POP_ZU && inside(sel_frame(), tx, ty)) {
                finger_drag = true;
                drag_begin(tx, ty);
            }
        } else if (finger_drag && t_active) {
            drag_update(tx, ty);
        }
    }
}

/* ================= Nach verlorenen Eingaben (SYN_DROPPED) ================= */

#define NBITS(x) ((x) / (8 * sizeof(long)) + 1)
static bool bit(const unsigned long *b, int i) {
    return (b[i / (8 * sizeof(long))] >> (i % (8 * sizeof(long)))) & 1;
}

void pen_resync(int fd) {
    bool alt_down = pen_down, alt_near = pen_near;
    unsigned long keys[NBITS(KEY_MAX)] = { 0 };
    if (ioctl(fd, EVIOCGKEY(sizeof keys), keys) >= 0) {
        pen_down = bit(keys, BTN_TOUCH);
        rubber   = bit(keys, BTN_TOOL_RUBBER);
        pen_near = rubber || bit(keys, BTN_TOOL_PEN);
    }
    struct input_absinfo a;
    if (ioctl(fd, EVIOCGABS(ABS_X), &a) >= 0) rx = a.value;
    if (ioctl(fd, EVIOCGABS(ABS_Y), &a) >= 0) ry = a.value;
    if (ioctl(fd, EVIOCGABS(ABS_PRESSURE), &a) >= 0) rp = imax(0, imin(PEN_MAX_P, a.value));
    pen_last_t = now_ms();
    if (pen_down != alt_down || pen_near != alt_near)
        dbg("Stift korrigiert: unten %d->%d, nah %d->%d", alt_down, pen_down, alt_near, pen_near);
    struct input_event syn = { .type = EV_SYN, .code = SYN_REPORT };
    pen_event(&syn);                      /* Aufsetzen/Abheben normal verarbeiten */
}

void touch_resync(int fd) {
    unsigned long caps[NBITS(KEY_MAX)] = { 0 }, keys[NBITS(KEY_MAX)] = { 0 };
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof caps), caps) < 0 || !bit(caps, BTN_TOUCH)) return;
    if (ioctl(fd, EVIOCGKEY(sizeof keys), keys) < 0) return;
    bool down = bit(keys, BTN_TOUCH);
    dbg("Touch synchronisiert: unten=%d aktiv=%d", down, t_active);
    if (!down && t_active) { t_active = false; t_new = false; touch_end(); }
}
