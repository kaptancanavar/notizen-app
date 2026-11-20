/* overview.c - Notizbuch-Uebersicht und Dialoge */
#include "app.h"

enum { DLG_ZU, DLG_NEU, DLG_OPTIONEN, DLG_LOESCHEN, DLG_NAME };
static const char *dlg_neu_items[] = { "Leer", "Liniert", "Kariert", "Punkte", "Abbrechen" };
static const char *dlg_opt_items[] = { "Umbenennen", "Löschen", "Abbrechen" };
static const char *dlg_del_items[] = { "Löschen", "Abbrechen" };

static int ov_page = 0;
static int dlg = DLG_ZU, dlg_nb = -1;

static int  ov_pages(void)   { return n_nbs == 0 ? 1 : (n_nbs + OV_PER - 1) / OV_PER; }
static int  ov_visible(void) { return imax(0, imin(OV_PER, n_nbs - ov_page * OV_PER)); }
static Rect back_rect(void)  { return (Rect){ 30, 50, BTN, BTN }; }
static Rect plus_rect(void)  { return (Rect){ W - 30 - BTN, 50, BTN, BTN }; }

static Rect card_rect(int i) {
    int cw = (W - (OV_COLS + 1) * OV_MARGIN) / OV_COLS;
    int col = i % OV_COLS, row = i / OV_COLS;
    return (Rect){ OV_MARGIN + col * (cw + OV_MARGIN),
        OV_TOP + row * (TH + CARD_LABEL + OV_GAP_Y),
        cw, TH + CARD_LABEL };
}

static int dlg_count(void) {
    switch (dlg) {
        case DLG_NEU:      return 5;
        case DLG_OPTIONEN: return 3;
        default:           return 2;
    }
}

static const char **dlg_items(void) {
    switch (dlg) {
        case DLG_NEU:      return dlg_neu_items;
        case DLG_OPTIONEN: return dlg_opt_items;
        default:           return dlg_del_items;
    }
}

static Rect dlg_rect(void) {
    int h = DLG_TITLE + dlg_count() * DLG_ITEM;
    return (Rect){ (W - DLG_W) / 2, (H - h) / 2, DLG_W, h };
}

static void draw_dialog(void) {
    if (dlg == DLG_ZU || dlg == DLG_NAME) return;
    Rect d = dlg_rect();
    fill(d, 0xFF);
    frame(d, 4, 0x00);
    char title[128];
    if (dlg == DLG_NEU) snprintf(title, sizeof title, "Neues Notizbuch: Papier wählen");
    else if (dlg == DLG_OPTIONEN) snprintf(title, sizeof title, "„%s“", nbs[dlg_nb].name);
    else snprintf(title, sizeof title, "„%s“ löschen?", nbs[dlg_nb].name);
    text(title, 50, d, d.t + 48, false, 50);
    const char **items = dlg_items();
    for (int i = 0; i < dlg_count(); i++) {
        int y = d.t + DLG_TITLE + i * DLG_ITEM;
        fill((Rect){ d.l + 30, y, d.w - 60, 2 }, 0xAA);
        text(items[i], 46, d, y + 34, false, 50);
    }
}

static void make_thumb(int idx, uint8_t *out) {
    strcpy(cur_nb, nbs[idx].id);
    paper = P_LEER;
    load_page(1);
    render_canvas();
    for (int ty = 0; ty < TH; ty++) {
        for (int tx = 0; tx < TW; tx++) {
            const uint8_t *src = canvas + (size_t)ty * TH_SCALE * W + (size_t)tx * TH_SCALE;
            int sum = 0;
            for (int dy = 0; dy < TH_SCALE; dy++)
                for (int dx = 0; dx < TH_SCALE; dx++)
                    sum += src[(size_t)dy * W + dx];
            int v = sum / (TH_SCALE * TH_SCALE);
            v = 255 - (255 - v) * 2;
            out[ty * TW + tx] = v < 0 ? 0 : v;
        }
    }
}

void ov_redraw(bool thumbs_neu, const FBInkConfig *c) {
    if (dlg == DLG_NAME) {                       /* Tastatur belegt den ganzen Bildschirm */
        kb_draw();
        refresh((Rect){ 0, 0, W, H }, c);
        return;
    }

    int nvis = ov_visible();
    if (thumbs_neu) {
        long t0 = now_ms();
        for (int i = 0; i < nvis; i++)
            make_thumb(ov_page * OV_PER + i, thumbs + (size_t)i * TW * TH);
        dbg("Vorschaubilder: %d in %ld ms", nvis, now_ms() - t0);
    }

    fill((Rect){ 0, 0, W, H }, 0xFF);
    icon("zurueck", false, back_rect());
    text("Notizbücher", 64, (Rect){ 0, 0, W, 200 }, 72, false, 170);
    icon("plus", false, plus_rect());
    fill((Rect){ 0, 196, W, 2 }, 0x88);

    if (n_nbs == 0)
        text("Tippe auf + für ein neues Notizbuch", 44, (Rect){ 0, 0, W, H }, H / 2 - 40, true, 0);

    for (int i = 0; i < nvis; i++) {
        NB *nb = &nbs[ov_page * OV_PER + i];
        Rect c = card_rect(i);
        int x = c.l + (c.w - TW) / 2;
        frame((Rect){ x - 2, c.t - 2, TW + 4, TH + 4 }, 2, 0x00);
        put_gray(thumbs + (size_t)i * TW * TH, TW, TH, x, c.t);
        text(nb->name, 38, c, c.t + TH + 12, true, 0);
        char datum[32], s[96];
        time_t mt = nb->mtime;
        strftime(datum, sizeof datum, "%d.%m.%Y", localtime(&mt));
        snprintf(s, sizeof s, "%s · %d %s", datum, nb->pages, nb->pages == 1 ? "Seite" : "Seiten");
        text(s, 28, c, c.t + TH + 64, true, 0);
    }

    if (ov_pages() > 1) {
        char s[32];
        snprintf(s, sizeof s, "%d / %d", ov_page + 1, ov_pages());
        text(s, 40, (Rect){ 0, 0, W, H }, H - 90, true, 0);
    }
    draw_dialog();
    refresh((Rect){ 0, 0, W, H }, c);
}

void leave_editor(void) {
    save_page();
    write_pos();
    reset_page_state();
    screen = SCR_UEBERSICHT;
    dlg = DLG_ZU;
    scan_notebooks();
    ov_page = 0;
    ov_redraw(true, &full_cfg);
}

/* Wischen links/rechts blaettert durch die Uebersicht */
void ov_swipe(int dx) {
    if (dlg != DLG_ZU) return;
    if (dx < 0 && ov_page + 1 < ov_pages()) { ov_page++; ov_redraw(true, &ui_cfg); }
    else if (dx > 0 && ov_page > 0) { ov_page--; ov_redraw(true, &ui_cfg); }
}

/* Tipp waehrend die Tastatur offen ist */
static void name_tap(int x, int y) {
    int r = kb_tap(x, y);
    if (r == KB_NONE) return;
    if (r == KB_OK) {
        const char *neu = kb_text();
        if (neu[0]) {
            write_name(nbs[dlg_nb].id, neu);
            scan_notebooks();
        }
    }
    dlg = DLG_ZU;
    ov_redraw(true, &full_cfg);
}

void ov_tap(int x, int y, bool lang) {
    dbg("ov_tap %d,%d lang=%d dlg=%d", x, y, lang, dlg);
    if (dlg == DLG_NAME) { name_tap(x, y); return; }

    if (dlg != DLG_ZU) {
        Rect d = dlg_rect();
        int sel = -1;
        if (inside(d, x, y) && y >= d.t + DLG_TITLE) sel = (y - d.t - DLG_TITLE) / DLG_ITEM;
        if (inside(d, x, y) && sel < 0) return;
        int mode = dlg;
        dlg = DLG_ZU;

        if (mode == DLG_NEU && sel >= 0 && sel < P_ANZAHL) { new_notebook(sel); return; }

        if (mode == DLG_OPTIONEN && sel == 0) {          /* Umbenennen */
            dlg = DLG_NAME;
            kb_start(nbs[dlg_nb].name);
            ov_redraw(false, &full_cfg);
            return;
        }
        if (mode == DLG_OPTIONEN && sel == 1) {          /* Loeschen -> nachfragen */
            dlg = DLG_LOESCHEN;
            ov_redraw(false, &ui_cfg);
            return;
        }
        if (mode == DLG_LOESCHEN && sel == 0) {
            delete_notebook(dlg_nb);
            scan_notebooks();
            if (ov_page >= ov_pages()) ov_page = ov_pages() - 1;
            ov_redraw(true, &ui_cfg);
            return;
        }
        ov_redraw(false, &ui_cfg);
        return;
    }

    if (inside(back_rect(), x, y)) { run = false; return; }
    if (inside(plus_rect(), x, y)) {
        dlg = DLG_NEU;
        draw_dialog();
        refresh(dlg_rect(), &ui_cfg);
        return;
    }
    for (int i = 0; i < ov_visible(); i++) {
        if (!inside(card_rect(i), x, y)) continue;
        int idx = ov_page * OV_PER + i;
        if (lang) {
            dlg = DLG_OPTIONEN;
            dlg_nb = idx;
            draw_dialog();
            refresh(dlg_rect(), &ui_cfg);
        } else {
            strcpy(cur_nb, nbs[idx].id);
            enter_editor();
        }
        return;
    }
}
