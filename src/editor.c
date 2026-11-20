/* editor.c - Werkzeugleiste, Popups, Menue und Seitenwechsel */
#include "app.h"

enum { R_SEP, R_MENU, R_PAPIER, R_ART, R_STAERKE, R_SEITE_WEG };
typedef struct { int kind, val; const char *label; } Row;

static const char *lvl_name[5]  = { "Fein", "Dünn", "Mittel", "Dick", "Sehr dick" };
static const int samp_stift[5]  = { 2, 3, 5, 8, 11 };
static const int samp_marker[5] = { 6, 9, 13, 18, 24 };
static const int samp_radier[5] = { 6, 10, 15, 21, 28 };
static const char *paper_name[P_ANZAHL] = { "Leer", "Liniert", "Kariert", "Punkte" };
static const char *icon_name[I_ANZAHL] = {
    "einklappen", "stift", "marker", "radierer", "lasso", "einfuegen", "undo", "redo", "menu"
};
static const char *menu_text[M_ANZAHL] = {
    "Neue Seite", "Vorherige Seite", "Nächste Seite", "Papiervorlage", "Seite leeren",
    "Formen erkennen", "Zur Übersicht"
};

/* ================= Geometrie ================= */

static Rect item_rect(int i) {
    int y = (i == I_KLAPPEN) ? TB_Y
          : TB_Y + BTN + GAP + PAD + (i - 1) * BTN + (i >= I_EINFUEGEN ? SEP : 0);
    return (Rect){ TB_X + PAD, y, BTN, BTN };
}
static Rect panel_rect(void)   { return (Rect){ TB_X, TB_Y + BTN + GAP, PANEL_W, PANEL_H }; }
static Rect tb_rect_open(void) { return (Rect){ TB_X, TB_Y, PANEL_W, BTN + GAP + PANEL_H }; }
Rect tb_rect(void)             { return tb_open ? tb_rect_open() : item_rect(I_KLAPPEN); }
Rect ind_rect(void)            { return (Rect){ (W - IND_W) / 2, H - IND_H - 20, IND_W, IND_H }; }

/* ================= Popups ================= */

static int pop_group(int p) { return p == POP_STIFT ? 0 : p == POP_MARKER ? 1 : 2; }

static int pop_anchor(void) {
    switch (popup) {
        case POP_STIFT:    return I_STIFT;
        case POP_MARKER:   return I_MARKER;
        case POP_RADIERER: return I_RADIERER;
        default:           return I_MENU;
    }
}

static int pop_rows(Row *r) {
    int n = 0;
    switch (popup) {
        case POP_MENU:
            for (int i = 0; i < M_ANZAHL; i++) {
                const char *l = menu_text[i];
                if (i == M_FORMEN) l = shapes_on ? "Formen erkennen: an" : "Formen erkennen: aus";
                r[n++] = (Row){ R_MENU, i, l };
            }
            break;
        case POP_PAPIER:
            for (int i = 0; i < P_ANZAHL; i++) r[n++] = (Row){ R_PAPIER, i, paper_name[i] };
            break;
        case POP_STIFT:
            r[n++] = (Row){ R_ART, T_STIFT, "Stift" };
            r[n++] = (Row){ R_ART, T_FUELLER, "Füller" };
            r[n++] = (Row){ R_ART, T_BLEISTIFT, "Bleistift" };
            r[n++] = (Row){ R_SEP, 0, NULL };
            for (int i = 0; i < 5; i++) r[n++] = (Row){ R_STAERKE, i, lvl_name[i] };
            break;
        case POP_MARKER:
            for (int i = 0; i < 5; i++) r[n++] = (Row){ R_STAERKE, i, lvl_name[i] };
            break;
        case POP_RADIERER:
            for (int i = 0; i < 5; i++) r[n++] = (Row){ R_STAERKE, i, lvl_name[i] };
            r[n++] = (Row){ R_SEP, 0, NULL };
            r[n++] = (Row){ R_SEITE_WEG, 0, "Seite löschen" };
            break;
    }
    return n;
}

Rect pop_rect(void) {
    Row r[12];
    int n = pop_rows(r), h = 0;
    for (int i = 0; i < n; i++) h += r[i].kind == R_SEP ? SEP_H : ROW_H;
    int t = item_rect(pop_anchor()).t;
    if (t + h > H - 40) t = H - 40 - h;
    return (Rect){ TB_X + PANEL_W + 20, t, POP_W, h };
}

static int pop_hit(int x, int y, Row *out) {
    Rect m = pop_rect();
    if (!inside(m, x, y)) return -1;
    Row r[12];
    int n = pop_rows(r), yy = m.t;
    for (int i = 0; i < n; i++) {
        int h = r[i].kind == R_SEP ? SEP_H : ROW_H;
        if (y >= yy && y < yy + h) { *out = r[i]; return r[i].kind == R_SEP ? -1 : i; }
        yy += h;
    }
    return -1;
}

static bool row_selected(const Row *r) {
    if (r->kind == R_PAPIER) return r->val == paper;
    if (r->kind == R_ART) return r->val == pen_kind;
    if (r->kind == R_STAERKE) return r->val == lvl[pop_group(popup)];
    return false;
}

/* ================= Zeichnen der Bedienelemente ================= */

static const char *pen_icon(void) {
    return pen_kind == T_FUELLER ? "fueller" : pen_kind == T_BLEISTIFT ? "bleistift" : "stift";
}

static bool tool_active(int i) {
    return (i == I_STIFT && tool == T_STIFT) || (i == I_MARKER && tool == T_MARKER)
        || (i == I_RADIERER && tool == T_RADIERER) || (i == I_LASSO && tool == T_LASSO)
        || (i == I_MENU && (popup == POP_MENU || popup == POP_PAPIER));
}

static void draw_toolbar(void) {
    if (!tb_open) {
        Rect k = item_rect(I_KLAPPEN);
        fill(k, 0xFF);
        icon("ausklappen", false, k);
        return;
    }
    fill(tb_rect(), 0xFF);
    icon("einklappen", false, item_rect(I_KLAPPEN));
    frame(panel_rect(), 3, 0x00);
    for (int i = I_STIFT; i < I_ANZAHL; i++) {
        Rect r = item_rect(i);
        bool a = tool_active(i);
        if (a) fill(r, 0x00);
        icon(i == I_STIFT ? pen_icon() : icon_name[i], a, r);
    }
    Rect u = item_rect(I_EINFUEGEN);
    fill((Rect){ u.l + 12, u.t - SEP / 2 - 1, BTN - 24, 2 }, 0x00);
}

static void draw_popup(void) {
    if (popup == POP_ZU) return;
    Rect m = pop_rect();
    fill(m, 0xFF);
    frame(m, 3, 0x00);
    Row r[12];
    int n = pop_rows(r), y = m.t;
    for (int i = 0; i < n; i++) {
        if (r[i].kind == R_SEP) {
            fill((Rect){ m.l + 3, y + SEP_H / 2 - 1, m.w - 6, 2 }, 0x00);
            y += SEP_H;
            continue;
        }
        if (i > 0 && r[i - 1].kind != R_SEP) fill((Rect){ m.l + 24, y, m.w - 48, 1 }, 0xAA);
        if (row_selected(&r[i])) text("•", 44, m, y + 23, false, 30);
        text(r[i].label, 44, m, y + 23, false, 80);
        if (r[i].kind == R_STAERKE) {
            Rect box = { m.l + m.w - 230, y + 10, 200, 80 };
            int g = pop_group(popup), v = r[i].val;
            if (g == 0) draw_sample(box, samp_stift[v], 0x00, false);
            else if (g == 1) draw_sample(box, samp_marker[v], MARKER_GRAU, false);
            else draw_sample(box, samp_radier[v], 0x00, true);
        }
        y += ROW_H;
    }
}

static void draw_indicator(void) {
    Rect r = ind_rect();
    fill(r, 0xFF);
    char s[32];
    snprintf(s, sizeof s, "%d / %d", cur_page, n_pages);
    text(s, 40, r, r.t + 16, true, 0);
}

void draw_overlays(void) {
    draw_toolbar();
    draw_indicator();
    draw_selection();
    draw_popup();
}

static void ui_update(Rect area) {
    blit(area);
    draw_overlays();
    refresh(area, &ui_cfg);
}

/* ================= Editor betreten / Seiten ================= */

void reset_page_state(void) {
    clear_history();
    sel_active = false;
    dragging = finger_drag = false;
    dg_dx = dg_dy = 0;
    popup = POP_ZU;
    pen_mode = PM_NONE;
}

void enter_editor(void) {
    dbg("Editor %s", cur_nb);
    screen = SCR_EDITOR;
    reset_page_state();
    n_pages = count_pages();
    if (n_pages == 0) {
        n_pages = 1; cur_page = 1;
        n_strokes = n_points = 0;
        save_page();
    } else {
        cur_page = read_pos();
        if (cur_page < 1 || cur_page > n_pages) cur_page = n_pages;
        load_page(cur_page);
    }
    full_redraw();
}

void new_notebook(int p) {
    long t = (long)time(NULL);
    char dir[160], name[64];
    for (;;) {
        snprintf(cur_nb, sizeof cur_nb, "nb_%ld", t);
        snprintf(dir, sizeof dir, NOTE_DIR "/%s", cur_nb);
        if (access(dir, F_OK) != 0) break;
        t++;
    }
    if (mkdir(dir, 0755) != 0) printf("FEHLER Ordner anlegen %s: %s\n", dir, strerror(errno));
    snprintf(name, sizeof name, "Notiz %d", next_number());
    write_name(cur_nb, name);
    paper = p;
    n_pages = 1; cur_page = 1;
    n_strokes = n_points = 0;
    save_page();
    write_pos();
    enter_editor();
}

void go_page(int p) {
    dbg("Seite %d -> %d", cur_page, p);
    save_page();
    cur_page = p;
    load_page(p);
    reset_page_state();
    full_redraw();
}

/* ================= Bedienung ================= */

static void menu_action(int m) {
    switch (m) {
        case M_NEU:
            if (n_strokes == 0 && cur_page == n_pages) break;
            save_page();
            n_pages++;
            cur_page = n_pages;
            n_strokes = n_points = 0;
            reset_page_state();
            save_page();
            full_redraw();
            break;
        case M_ZURUECK: if (cur_page > 1) go_page(cur_page - 1); break;
        case M_WEITER:  if (cur_page < n_pages) go_page(cur_page + 1); break;
        case M_LEEREN:
            sel_clear(false);
            push_undo();
            n_strokes = n_points = 0;
            save_page();
            full_redraw();
            break;
        case M_FORMEN:
            shapes_on = !shapes_on;
            save_settings();
            break;
        case M_UEBERSICHT:
            leave_editor();
            break;
    }
}

static void set_paper(int p) {
    if (p == paper) return;
    paper = p;
    save_page();
    full_redraw();
}

static void open_popup(int p) {
    Rect tb_alt = tb_rect();
    popup = p;
    ui_update(unite(pop_rect(), tb_alt));
}

bool ui_hit(int x, int y) {
    if (popup != POP_ZU) return true;
    if (bar_visible() && inside(sel_bar_rect(), x, y)) return true;
    if (inside(item_rect(I_KLAPPEN), x, y)) return true;
    return tb_open && inside(panel_rect(), x, y);
}

static void select_tool(int t, int pop, Rect tb_alt) {
    if (tool == t) { if (pop != POP_ZU) open_popup(pop); return; }
    sel_clear(true);
    tool = t;
    save_settings();
    ui_update(tb_alt);
}

void ui_tap(int x, int y) {
    dbg("ui_tap %d,%d popup=%d", x, y, popup);
    Rect tb_alt = tb_rect();

    if (popup != POP_ZU) {
        Rect m = pop_rect();
        int old = popup;
        Row row;
        int hit = pop_hit(x, y, &row);

        if (hit >= 0 && row.kind == R_MENU && row.val == M_PAPIER) {
            popup = POP_PAPIER;
            ui_update(unite(m, pop_rect()));
            return;
        }
        if (inside(m, x, y) && hit < 0) return;

        popup = POP_ZU;
        if (hit >= 0 && row.kind == R_ART) { pen_kind = row.val; save_settings(); }
        if (hit >= 0 && row.kind == R_STAERKE) { lvl[pop_group(old)] = row.val; save_settings(); }

        bool leave = hit >= 0 && row.kind == R_MENU && row.val == M_UEBERSICHT;
        if (!leave) ui_update(unite(m, tb_alt));

        if (hit >= 0) {
            if (row.kind == R_MENU) menu_action(row.val);
            else if (row.kind == R_PAPIER) set_paper(row.val);
            else if (row.kind == R_SEITE_WEG) menu_action(M_LEEREN);
        }
        return;
    }

    if (bar_visible() && inside(sel_bar_rect(), x, y)) {
        sel_bar_tap(x);
        return;
    }

    int hit = -1;
    if (inside(item_rect(I_KLAPPEN), x, y)) hit = I_KLAPPEN;
    else if (tb_open)
        for (int i = I_STIFT; i < I_ANZAHL; i++)
            if (inside(item_rect(i), x, y)) hit = i;

    switch (hit) {
        case I_KLAPPEN:   tb_open = !tb_open; ui_update(tb_rect_open()); break;
        case I_STIFT:     select_tool(T_STIFT, POP_STIFT, tb_alt); break;
        case I_MARKER:    select_tool(T_MARKER, POP_MARKER, tb_alt); break;
        case I_RADIERER:  select_tool(T_RADIERER, POP_RADIERER, tb_alt); break;
        case I_LASSO:     select_tool(T_LASSO, POP_ZU, tb_alt); break;
        case I_EINFUEGEN: paste(); break;
        case I_UNDO:      do_undo(); break;
        case I_REDO:      do_redo(); break;
        case I_MENU:      sel_clear(true); open_popup(POP_MENU); break;
    }
}
