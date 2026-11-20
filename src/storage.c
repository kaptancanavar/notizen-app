/* storage.c - Seiten und Notizbuecher speichern und laden */
#include "app.h"

static int cap_nbs;

static void page_path(char *buf, size_t n, int page) {
    snprintf(buf, n, NOTE_DIR "/%s/seite_%03d.kno", cur_nb, page);
}

/* schreibt erst in eine temporaere Datei und benennt sie dann um, damit eine
 * volle Platte oder ein Absturz nie eine halb geschriebene Seite hinterlaesst */
void save_page(void) {
    long t0 = now_ms();
    char fn[160], tmpfn[170];
    page_path(fn, sizeof fn, cur_page);
    snprintf(tmpfn, sizeof tmpfn, "%s.tmp", fn);
    FILE *f = fopen(tmpfn, "wb");
    if (!f) { printf("FEHLER Speichern %s: %s\n", tmpfn, strerror(errno)); return; }
    bool ok = fwrite("KNO4", 1, 4, f) == 4
        && fwrite(&paper, sizeof paper, 1, f) == 1
        && fwrite(&n_strokes, sizeof n_strokes, 1, f) == 1
        && fwrite(strokes, sizeof(Stroke), n_strokes, f) == (size_t)n_strokes
        && fwrite(&n_points, sizeof n_points, 1, f) == 1
        && fwrite(points, sizeof(Point), n_points, f) == (size_t)n_points
        && fflush(f) == 0
        && fsync(fileno(f)) == 0;
    if (fclose(f) != 0) ok = false;
    if (!ok || rename(tmpfn, fn) != 0) {
        printf("FEHLER Speichern %s: %s\n", fn, strerror(errno));
        unlink(tmpfn);
    }
    long dt = now_ms() - t0;
    if (dt > 300) dbg("Speichern dauerte %ld ms (%d Punkte)", dt, n_points);
}

void load_page(int page) {
    n_strokes = n_points = 0;
    char fn[160];
    page_path(fn, sizeof fn, page);
    FILE *f = fopen(fn, "rb");
    if (!f) return;
    do {
        char magic[4];
        int ns = 0, np = 0, pp = P_LEER;
        if (fread(magic, 1, 4, f) != 4) break;
        bool v1 = memcmp(magic, "KNO1", 4) == 0;
        bool v2 = memcmp(magic, "KNO2", 4) == 0;
        bool v3 = memcmp(magic, "KNO3", 4) == 0;
        bool v4 = memcmp(magic, "KNO4", 4) == 0;
        if (!v1 && !v2 && !v3 && !v4) break;

        if ((v3 || v4) && fread(&pp, sizeof pp, 1, f) != 1) break;
        paper = (pp >= 0 && pp < P_ANZAHL) ? pp : P_LEER;

        /* Groessen gegen die Dateigroesse pruefen (kaputte Datei -> kein Riesen-malloc) */
        struct stat st;
        long fsize = fstat(fileno(f), &st) == 0 ? (long)st.st_size : 0;

        if (fread(&ns, sizeof ns, 1, f) != 1 || ns < 0) break;
        if ((long long)ns * (long long)sizeof(Stroke) > fsize) break;
        ensure_strokes(ns);
        if (fread(strokes, sizeof(Stroke), ns, f) != (size_t)ns) break;

        if (fread(&np, sizeof np, 1, f) != 1 || np < 0) break;
        if ((long long)np * (long long)sizeof(Point) > fsize) break;
        ensure_points(np);
        if (fread(points, sizeof(Point), np, f) != (size_t)np) break;

        if (!v4)
            for (int i = 0; i < np; i++) points[i].p = 128;

        /* Striche pruefen: Punktbereiche muessen gueltig und aufsteigend sein
         * (delete_selected und try_shape verlassen sich darauf) */
        int k = 0, end = 0;
        for (int i = 0; i < ns; i++) {
            Stroke s = strokes[i];
            if (s.start < end || s.count < 0 || s.start > np - s.count) continue;
            if (v1) s.tool = s.tool ? T_RADIERER : T_STIFT;
            if (s.tool > T_BLEISTIFT) s.tool = T_STIFT;
            s.sel = 0;
            strokes[k++] = s;
            end = s.start + s.count;
        }
        n_strokes = k;
        n_points = np;
    } while (0);
    fclose(f);
}

int count_pages(void) {
    char fn[160];
    int n = 0;
    for (;;) {
        page_path(fn, sizeof fn, n + 1);
        if (access(fn, F_OK) != 0) break;
        n++;
    }
    return n;
}

int read_pos(void) {
    char fn[160];
    snprintf(fn, sizeof fn, NOTE_DIR "/%s/position", cur_nb);
    int p = 1;
    FILE *f = fopen(fn, "r");
    if (f) { if (fscanf(f, "%d", &p) != 1) p = 1; fclose(f); }
    return p;
}

void write_pos(void) {
    char fn[160];
    snprintf(fn, sizeof fn, NOTE_DIR "/%s/position", cur_nb);
    FILE *f = fopen(fn, "w");
    if (f) { fprintf(f, "%d\n", cur_page); fclose(f); }
}

static void read_name(const char *id, char *out, size_t n) {
    char fn[160];
    snprintf(fn, sizeof fn, NOTE_DIR "/%s/name", id);
    out[0] = 0;
    FILE *f = fopen(fn, "r");
    if (!f) return;
    if (fgets(out, (int)n, f)) out[strcspn(out, "\r\n")] = 0;
    fclose(f);
}

void write_name(const char *id, const char *name) {
    char fn[160];
    snprintf(fn, sizeof fn, NOTE_DIR "/%s/name", id);
    FILE *f = fopen(fn, "w");
    if (f) { fprintf(f, "%s\n", name); fclose(f); }
}

int next_number(void) {
    int mx = 0;
    for (int i = 0; i < n_nbs; i++) {
        int k;
        if (sscanf(nbs[i].name, "Notiz %d", &k) == 1 && k > mx) mx = k;
    }
    return mx + 1;
}

static int cmp_nb(const void *a, const void *b) {
    const NB *x = a, *y = b;
    if (x->mtime != y->mtime) return x->mtime < y->mtime ? 1 : -1;
    return strcmp(y->id, x->id);
}

void scan_notebooks(void) {
    n_nbs = 0;
    DIR *d = opendir(NOTE_DIR);
    if (!d) { printf("FEHLER Ordner %s: %s\n", NOTE_DIR, strerror(errno)); return; }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "nb_", 3) != 0 || strlen(e->d_name) >= sizeof(((NB *)0)->id)) continue;
        if (n_nbs >= cap_nbs) {
            cap_nbs = cap_nbs ? cap_nbs * 2 : 32;
            nbs = realloc(nbs, (size_t)cap_nbs * sizeof(NB));
        }
        NB *nb = &nbs[n_nbs];
        strcpy(nb->id, e->d_name);
        nb->created = atol(e->d_name + 3);
        read_name(nb->id, nb->name, sizeof nb->name);
        strcpy(cur_nb, nb->id);
        nb->pages = count_pages();
        nb->mtime = nb->created;
        char fn[160];
        struct stat s;
        for (int p = 1; p <= nb->pages; p++) {
            page_path(fn, sizeof fn, p);
            if (stat(fn, &s) == 0 && s.st_mtime > nb->mtime) nb->mtime = s.st_mtime;
        }
        n_nbs++;
    }
    closedir(d);

    /* Notizbuecher ohne Namen durchnummerieren */
    for (;;) {
        int best = -1;
        for (int i = 0; i < n_nbs; i++)
            if (nbs[i].name[0] == 0 && (best < 0 || nbs[i].created < nbs[best].created)) best = i;
        if (best < 0) break;
        snprintf(nbs[best].name, sizeof nbs[best].name, "Notiz %d", next_number());
        write_name(nbs[best].id, nbs[best].name);
    }
    qsort(nbs, n_nbs, sizeof(NB), cmp_nb);
}

void delete_notebook(int idx) {
    if (idx < 0 || idx >= n_nbs) return;
    char dir[160], fn[512];
    snprintf(dir, sizeof dir, NOTE_DIR "/%s", nbs[idx].id);
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            snprintf(fn, sizeof fn, "%s/%s", dir, e->d_name);
            unlink(fn);
        }
        closedir(d);
    }
    rmdir(dir);
}
