/* main.c - globaler Zustand und Hauptschleife */
#include "app.h"

int fbfd, W, H, TW, TH;
uint8_t *canvas, *bg, *tmp, *thumbs, *base;
FBInkConfig draw_cfg = {0}, ref_cfg = {0}, gray_cfg = {0}, ui_cfg = {0}, full_cfg = {0};

Point  *points;  int n_points;
Stroke *strokes; int n_strokes;
int cur_page = 1, n_pages = 1, paper = P_LINIERT;

NB  *nbs; int n_nbs;
char cur_nb[40];

int  screen = SCR_UEBERSICHT, tool = T_STIFT, pen_kind = T_STIFT;
int  lvl[3] = { 2, 2, 2 };
int  popup = POP_ZU;
bool shapes_on = true, tb_open = true, run = true, dirty = false;

bool sel_active = false, dragging = false, finger_drag = false;
Rect sel_box;
int  dg_dx, dg_dy;

int  pen_mode = PM_NONE, stroke_tool;
bool pen_down;
int  anchor_x, anchor_y;
long anchor_t;
bool shaped;

static void on_signal(int sig) { (void)sig; run = false; }

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    struct sigaction sa = { .sa_handler = on_signal };   /* ohne SA_RESTART: poll bricht ab */
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    if (mkdir(NOTE_DIR, 0755) != 0 && errno != EEXIST)
        printf("FEHLER Ordner %s: %s\n", NOTE_DIR, strerror(errno));

    int pen = open(PEN_DEV, O_RDONLY);
    if (pen < 0) { perror("Stift"); return 1; }
    int touch = open(TOUCH_DEV, O_RDONLY);

    full_cfg.is_quiet = true;
    full_cfg.is_flashing = true;
    fbfd = fbink_open();
    if (fbfd < 0 || fbink_init(fbfd, &full_cfg) < 0) { printf("FBInk-Fehler\n"); return 1; }
    FBInkState st = {0};
    fbink_get_state(&full_cfg, &st);
    W = st.screen_width;
    H = st.screen_height;
    TW = W / TH_SCALE;
    TH = H / TH_SCALE;

    draw_cfg.is_quiet = true;  draw_cfg.no_refresh = true;
    ref_cfg.is_quiet  = true;  ref_cfg.wfm_mode  = WFM_DU;
    gray_cfg.is_quiet = true;  gray_cfg.wfm_mode = WFM_GL16;
    ui_cfg.is_quiet   = true;  ui_cfg.wfm_mode   = WFM_GC16;

    if (fbink_add_ot_font(APP_DIR "/font.ttf", FNT_REGULAR) < 0) printf("Schrift fehlt\n");

    canvas = malloc((size_t)W * H);
    bg     = malloc((size_t)W * H);
    tmp    = malloc((size_t)W * H);
    base   = malloc((size_t)W * H);
    thumbs = malloc((size_t)OV_PER * TW * TH);
    if (!canvas || !bg || !tmp || !base || !thumbs) { printf("Kein Speicher\n"); return 1; }

    load_settings();

    if (ioctl(pen, EVIOCGRAB, 1) < 0) printf("Stift-Grab fehlgeschlagen: %s\n", strerror(errno));
    if (touch >= 0 && ioctl(touch, EVIOCGRAB, 1) < 0)
        printf("Touch-Grab fehlgeschlagen: %s\n", strerror(errno));

    dbg("Start %dx%d, Touch %s", W, H, touch >= 0 ? "ok" : "FEHLT");
    scan_notebooks();
    screen = SCR_UEBERSICHT;
    ov_redraw(true, &full_cfg);

    struct pollfd pfd[2] = { { .fd = pen, .events = POLLIN }, { .fd = touch, .events = POLLIN } };
    int nfds = touch >= 0 ? 2 : 1;
    struct input_event evs[64];
    bool dropped[2] = { false, false };

    long last_input = now_ms();
    while (run) {
        long now = now_ms();
        int to = (int)imax(1, TIMEOUT_MS - (now - last_input));
        /* beim Zeichnen kurz aufwachen, um "Stift still halten" zu erkennen */
        if (hold_possible()) {
            long rem = HOLD_MS - (now - anchor_t);
            to = imin(to, rem > 0 ? (int)rem + 5 : 1);
        }
        /* solange der Stift als unten/nah gilt, regelmaessig pruefen, ob das noch stimmt */
        if (pen_busy()) to = imin(to, PEN_STALE_MS + 50);

        int pr = poll(pfd, nfds, to);
        if (pr < 0) {
            if (errno == EINTR && run) continue;
            dbg("poll beendet: %s", strerror(errno));
            break;
        }
        if (pen_stale()) pen_resync(pen);
        if (pr == 0) {
            if (hold_possible()) { check_hold(); continue; }
            if (now_ms() - last_input >= TIMEOUT_MS) {
                dbg("lange keine Eingabe -> Ende");
                break;
            }
            continue;
        }
        last_input = now_ms();
        for (int k = 0; k < nfds && run; k++) {
            if (!(pfd[k].revents & POLLIN)) continue;
            ssize_t n = read(pfd[k].fd, evs, sizeof evs);
            if (n <= 0) { dbg("Lesefehler Geraet %d: %s", k, n < 0 ? strerror(errno) : "EOF"); run = false; break; }
            int cnt = n / (ssize_t)sizeof evs[0];
            for (int i = 0; i < cnt && run; i++) {
                const struct input_event *ev = &evs[i];
                /* Kernel-Puffer uebergelaufen: bis zum naechsten SYN_REPORT alles
                 * verwerfen, dann den echten Zustand vom Geraet abfragen */
                if (ev->type == EV_SYN && ev->code == SYN_DROPPED) {
                    dbg("Eingaben verloren (Geraet %d), synchronisiere", k);
                    dropped[k] = true;
                    continue;
                }
                if (dropped[k]) {
                    if (ev->type == EV_SYN && ev->code == SYN_REPORT) {
                        dropped[k] = false;
                        if (k == 0) pen_resync(pfd[k].fd);
                        else touch_resync(pfd[k].fd);
                    }
                    continue;
                }
                if (k == 0) pen_event(ev);
                else touch_event(ev);
            }
        }
    }

    dbg("Ende");
    if (screen == SCR_EDITOR) { save_page(); write_pos(); }
    ioctl(pen, EVIOCGRAB, 0);
    close(pen);
    if (touch >= 0) { ioctl(touch, EVIOCGRAB, 0); close(touch); }
    fbink_free_ot_fonts();
    fbink_cls(fbfd, &full_cfg, NULL, false);
    fbink_close(fbfd);
    clear_history();
    free(canvas); free(bg); free(tmp); free(base); free(thumbs);
    free(points); free(strokes); free(nbs);
    return 0;
}
