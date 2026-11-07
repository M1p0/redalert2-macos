/*
 * menu.c - F10, the settings menu: the Windows presenter's (src/runtime/
 * present.c, settings_menu), drawn by win32hle over the picture
 * (hle_screen_menu). Scaling, the bars, fullscreen and HD vehicles take
 * effect at once and are remembered in the presentation preferences; the game's resolution is for
 * the next game; a mod restarts the program with it.
 *
 * Not here: Controls (Classic / Modern). Modern controls live in the Windows
 * presenter, and this host passes the game's own input through (main.c).
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <SDL2/SDL.h>
#include "win32hle.h"
#include "recomp_types.h"
#include "recomp_target.h"
#include "resolutions.h"
#include "hdvox.h"
#include "mods.h"
#include "mapdrag.h"

#define MAX_MENU_MODS 64
enum { ID_SCALE = 100, ID_BARS = 200, ID_FULL = 300, ID_RES = 400, ID_DISPLAY = 499, ID_HDVOX = 500, ID_MOD = 600, ID_REFRESH = 700, ID_DRAG = 800 };

static const char *const k_scale[] = { "sharp", "smooth", "crt", "nearest", "integer" };
static const char *const k_scale_label[] = { "Sharp (default)", "Smooth", "CRT", "Nearest", "Integer" };
static hle_menu_item_t g_main[10], g_scale[5], g_bars[2], g_hd[4], g_res[NRES + 1], g_mods[MAX_MENU_MODS + 1], g_refresh[5], g_drag[2];
static const int k_refresh[] = { 0, 60, 120, 144, 240 };
static const char *const k_refresh_label[] = { "Display refresh (automatic)", "60 Hz", "120 Hz", "144 Hz", "240 Hz" };
static char g_res_label[NRES + 1][40], g_mod_names[MAX_MENU_MODS][128];
static int g_nmods, g_display_w, g_display_h;
static void (*g_save)(void);
static int (*g_next_key)(int sdl_scancode, int down);   /* mods.c's F9 */

/* key=val in [sec] of an INI file, in place: its line replaced (with keep,
 * left as it is), else added at the section's end, else the section added at
 * the end. */
static void ini_set(const char *path, const char *sec, const char *key, const char *val, int keep) {
    FILE *f = fopen(path, "rb");
    char *in = NULL;
    size_t n = 0;
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        rewind(f);
        in = (char *)malloc(size > 0 ? (size_t)size + 1 : 1);
        n = in ? fread(in, 1, size > 0 ? (size_t)size : 0, f) : 0;
        if (in) in[n] = 0;
        fclose(f);
    }
    FILE *o = fopen(path, "wb");
    if (!o) { free(in); fprintf(stderr, "[menu] cannot write %s\n", path); return; }
    size_t kl = strlen(key), sl = strlen(sec);
    int in_sec = 0, done = 0;
    for (char *p = in; p && *p;) {
        char *e = strchr(p, '\n');
        size_t len = e ? (size_t)(e - p + 1) : strlen(p);
        if (*p == '[') {
            if (in_sec && !done) fprintf(o, "%s=%s\r\n", key, val), done = 1;
            in_sec = !strncasecmp(p + 1, sec, sl) && p[1 + sl] == ']';
        }
        if (in_sec && !done && !strncasecmp(p, key, kl) && p[kl] == '=') {
            if (keep) fwrite(p, 1, len, o); else fprintf(o, "%s=%s\r\n", key, val);
            done = 1;
        } else fwrite(p, 1, len, o);
        p += len;
    }
    if (!done) {
        if (n && in[n - 1] != '\n') fputs("\r\n", o);
        if (!in_sec) fprintf(o, "[%s]\r\n", sec);
        fprintf(o, "%s=%s\r\n", key, val);
    }
    fclose(o);
    free(in);
}

static void set_game_resolution(int w, int h) {
    char path[PATH_MAX], v[16];
    hle_host_path_for_write(RA2_INI_NAME, path, sizeof path, 1);   /* the game runs in its folder; a mod's, over it */
    snprintf(v, sizeof v, "%d", w), ini_set(path, "Video", "ScreenWidth", v, 0);
    snprintf(v, sizeof v, "%d", h), ini_set(path, "Video", "ScreenHeight", v, 0);
    if (res_plausible((int)MEM32(RA2_OPT_W_VA), (int)MEM32(RA2_OPT_H_VA)))
        MEM32(RA2_OPT_W_VA) = (uint32_t)w, MEM32(RA2_OPT_H_VA) = (uint32_t)h;
    fprintf(stderr, "[menu] game resolution %dx%d (the next battle opens at it)\n", w, h);
}

static void switch_mod(const char *name) {
    char ask[256];
    if (!strcasecmp(name, mods_active())) return;
    snprintf(ask, sizeof ask, "Restart with %s? A game in progress is lost.", name[0] ? name : "no mod");
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
    };
    const SDL_MessageBoxData box = { SDL_MESSAGEBOX_INFORMATION, NULL, RA2_TITLE " (recomp)", ask, 2, buttons, NULL };
    int yes = 0;
    if (SDL_ShowMessageBox(&box, &yes) == 0 && yes) mods_restart_with(name);
}

static void chosen(int id) {
    if (id >= ID_SCALE && id < ID_SCALE + 5) hle_screen_scale(k_scale[id - ID_SCALE]);
    else if (id == ID_BARS || id == ID_BARS + 1) hle_screen_bars(id - ID_BARS);
    else if (id == ID_FULL) { int full; hle_screen_settings(NULL, &full); hle_screen_fullscreen(!full); return; }   /* saves, as F11 */
    else if (id >= ID_REFRESH && id < ID_REFRESH + 5) hle_screen_refresh_hz(k_refresh[id - ID_REFRESH]);
    else if (id == ID_DRAG || id == ID_DRAG + 1) ra2_mapdrag_set(id - ID_DRAG);
    else if (id >= ID_HDVOX && id <= ID_HDVOX + 3) {              /* Off / 2x / 3x / 1x smooth, next frame */
        if (id == ID_HDVOX) ra2_vox_hd_show = 0;
        else if (id == ID_HDVOX + 3) { ra2_vox_set_scale(2); ra2_vox_aa = 1; ra2_vox_hd_show = 1; }
        else { ra2_vox_set_scale(id == ID_HDVOX + 2 ? 3 : 2); ra2_vox_aa = 0; ra2_vox_hd_show = 1; }
    }
    else if (id >= ID_RES && id < ID_RES + NRES) { set_game_resolution(k_res[id - ID_RES][0], k_res[id - ID_RES][1]); return; }
    else if (id == ID_DISPLAY) { set_game_resolution(g_display_w, g_display_h); return; }
    else if (id >= ID_MOD && id <= ID_MOD + g_nmods) { switch_mod(id == ID_MOD ? "" : g_mod_names[id - ID_MOD - 1]); return; }
    if (g_save) g_save();
}

static void localize_menu(hle_menu_item_t *items, int n) {
    static const struct { const char *en, *zh; } labels[] = {
        { "Scaling", "\u753b\u9762\u7f29\u653e" },
        { "Bars beside the picture", "\u8fb9\u6761\u80cc\u666f" },
        { "Fullscreen", "\u5168\u5c4f" },
        { "Display refresh", "\u663e\u793a\u5237\u65b0\u7387" },
        { "Right mouse button", "\u53f3\u952e\u64cd\u4f5c" },
        { "HD vehicles (voxels)", "\u9ad8\u6e05\u8f7d\u5177" },
        { "Game resolution (next battle)", "\u5bf9\u5c40\u5206\u8fa8\u7387\uff08\u4e0b\u5c40\u751f\u6548\uff09" },
        { "Mod (restarts the game)", "\u6a21\u7ec4\uff08\u91cd\u542f\u6e38\u620f\uff09" },
        { "Original", "\u539f\u7248" },
        { "Drag map", "\u62d6\u62fd\u5730\u56fe" },
        { "Sharp (default)", "\u6e05\u6670\uff08\u9ed8\u8ba4\uff09" },
        { "Smooth", "\u5e73\u6ed1" },
        { "CRT", "\u663e\u50cf\u7ba1\u6548\u679c" },
        { "Nearest", "\u90bb\u8fd1\u50cf\u7d20" },
        { "Integer", "\u6574\u6570\u500d\u7f29\u653e" },
        { "Blurred", "\u6a21\u7cca\u80cc\u666f" },
        { "Black", "\u9ed1\u8272" },
        { "Off", "\u5173\u95ed" },
        { "On, 2x", "\u5f00\u542f\uff0c2 \u500d" },
        { "On, 3x (sharper, slower)", "\u5f00\u542f\uff0c3 \u500d\uff08\u66f4\u6e05\u6670\uff09" },
        { "On, 1x smooth (antialiased)", "\u5f00\u542f\uff0c\u539f\u5c3a\u5bf8\u6297\u952f\u9f7f" },
        { "Display refresh (automatic)", "\u8ddf\u968f\u663e\u793a\u5668\uff08\u81ea\u52a8\uff09" },
        { "None (the game as it shipped)", "\u65e0\uff08\u539f\u7248\u6e38\u620f\uff09" },
        { "(put mods in the mods folder: mods/README.md)", "\uff08\u5c1a\u672a\u5b89\u88c5\u6a21\u7ec4\uff09" },
    };
    for (int i = 0; i < n; i++) {
        if (items[i].label) for (unsigned j = 0; j < sizeof labels / sizeof labels[0]; j++)
            if (!strcmp(items[i].label, labels[j].en)) { items[i].label = labels[j].zh; break; }
        if (items[i].sub) localize_menu((hle_menu_item_t *)items[i].sub, items[i].nsub);
    }
}

static void open_menu(void) {
    int bars, full, n = 0, nm = 0;
    const char *scale = hle_screen_settings(&bars, &full);
    g_drag[0] = (hle_menu_item_t){ "Original", NULL, ID_DRAG, !ra2_mapdrag_enabled(), NULL, 0 };
    g_drag[1] = (hle_menu_item_t){ "Drag map", NULL, ID_DRAG + 1, ra2_mapdrag_enabled(), NULL, 0 };
    for (int i = 0; i < 5; i++) g_refresh[i] = (hle_menu_item_t){ k_refresh_label[i], NULL, ID_REFRESH + i,
        hle_screen_refresh_setting() == k_refresh[i], NULL, 0 };
    for (int i = 0; i < 5; i++) g_scale[i] = (hle_menu_item_t){ k_scale_label[i], NULL, ID_SCALE + i, !strcmp(scale, k_scale[i]), NULL, 0 };
    g_bars[0] = (hle_menu_item_t){ "Blurred", NULL, ID_BARS + 1, bars, NULL, 0 };
    g_bars[1] = (hle_menu_item_t){ "Black", NULL, ID_BARS, !bars, NULL, 0 };
    int sc = ra2_vox_scale();
    g_hd[0] = (hle_menu_item_t){ "Off", NULL, ID_HDVOX, !ra2_vox_hd_show, NULL, 0 };
    g_hd[1] = (hle_menu_item_t){ "On, 2x", NULL, ID_HDVOX + 1, ra2_vox_hd_show && !ra2_vox_aa && sc == 2, NULL, 0 };
    g_hd[2] = (hle_menu_item_t){ "On, 3x (sharper, slower)", NULL, ID_HDVOX + 2, ra2_vox_hd_show && !ra2_vox_aa && sc == 3, NULL, 0 };
    g_hd[3] = (hle_menu_item_t){ "On, 1x smooth (antialiased)", NULL, ID_HDVOX + 3, ra2_vox_hd_show && ra2_vox_aa, NULL, 0 };
    int w = (int)MEM32(RA2_OPT_W_VA), h = (int)MEM32(RA2_OPT_H_VA), nr = 0, dw = 0, dh = 0;
    /* this display's own size first, when it isn't one of the list's (a
     * MacBook's 1512 x 982); no larger than 4K, the largest tested */
    if (hle_screen_desktop(&dw, &dh) && dw >= 800 && dh >= 600 && dw <= 3840 && dh <= 2160) {
        int listed = 0;
        for (int i = 0; i < NRES; i++) listed |= dw == k_res[i][0] && dh == k_res[i][1];
        if (!listed) {
            g_display_w = dw, g_display_h = dh;
            snprintf(g_res_label[NRES], sizeof g_res_label[NRES], "%d x %d (this display)", dw, dh);
            g_res[nr++] = (hle_menu_item_t){ g_res_label[NRES], NULL, ID_DISPLAY, w == dw && h == dh, NULL, 0 };
        }
    }
    for (int i = 0; i < NRES; i++) {
        snprintf(g_res_label[i], sizeof g_res_label[i], "%d x %d%s", k_res[i][0], k_res[i][1],
                 dw == k_res[i][0] && dh == k_res[i][1] ? " (this display)" : "");
        g_res[nr++] = (hle_menu_item_t){ g_res_label[i], NULL, ID_RES + i, w == k_res[i][0] && h == k_res[i][1], NULL, 0 };
    }
    g_nmods = mods_list(g_mod_names, MAX_MENU_MODS);
    g_mods[nm++] = (hle_menu_item_t){ "None (the game as it shipped)", NULL, ID_MOD, !mods_active()[0], NULL, 0 };
    for (int i = 0; i < g_nmods; i++)
        g_mods[nm++] = (hle_menu_item_t){ g_mod_names[i], NULL, ID_MOD + 1 + i, !strcasecmp(mods_active(), g_mod_names[i]), NULL, 0 };
    if (!g_nmods) g_mods[nm++] = (hle_menu_item_t){ "(put mods in the mods folder: mods/README.md)", NULL, 0, 0, NULL, 0 };
    g_main[n++] = (hle_menu_item_t){ "Scaling", "F12", 0, 0, g_scale, 5 };
    g_main[n++] = (hle_menu_item_t){ "Bars beside the picture", NULL, 0, 0, g_bars, 2 };
    g_main[n++] = (hle_menu_item_t){ "Fullscreen", "F11", ID_FULL, full, NULL, 0 };
    g_main[n++] = (hle_menu_item_t){ "Display refresh", NULL, 0, 0, g_refresh, 5 };
    g_main[n++] = (hle_menu_item_t){ "Right mouse button", NULL, 0, 0, g_drag, 2 };
    g_main[n++] = (hle_menu_item_t){ "HD vehicles (voxels)", NULL, 0, 0, g_hd, 4 };
    g_main[n++] = (hle_menu_item_t){ NULL, NULL, 0, 0, NULL, 0 };
    g_main[n++] = (hle_menu_item_t){ "Game resolution (next battle)", NULL, 0, 0, g_res, nr };
    g_main[n++] = (hle_menu_item_t){ "Mod (restarts the game)", NULL, 0, 0, g_mods, nm };
    const char *language = getenv("RA2_UI_LANGUAGE");
    if (language && !strncmp(language, "zh", 2)) localize_menu(g_main, n);
    hle_screen_menu(g_main, n, chosen);
}

static int on_key(int scancode, int down) {
    if (scancode != SDL_SCANCODE_F10) return g_next_key ? g_next_key(scancode, down) : 0;
    if (down) open_menu();
    return 1;
}

void menu_start(void (*save)(void)) {
    /* The game's own Options screen lists only 640, 800 and 1024 wide unless
     * [Video] AllowHiResModes is on; then every mode DirectDraw offers, which
     * win32hle caps at 4K (issue #23). A player's own setting stays. Windows
     * is left alone: its DirectDraw is the display's, and can offer more than
     * the presenter shows. */
    char path[PATH_MAX];
    hle_host_path_for_write(RA2_INI_NAME, path, sizeof path, 1);
    ini_set(path, "Video", "AllowHiResModes", "yes", 1);
    fprintf(stderr, "[menu] F10 for the settings; %s has AllowHiResModes\n", path);
    g_save = save;
    g_next_key = hle_screen_key_hook;
    hle_screen_key_hook = on_key;
}
