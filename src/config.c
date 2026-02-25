/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "config.h"
#include "util.h"
#include "cursor.h"
#include "rawinput.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

static _locale_t g_c_locale = NULL;

static _locale_t get_c_locale(void) {
    if (!g_c_locale)
        g_c_locale = _create_locale(LC_NUMERIC, "C");
    return g_c_locale;
}

/* ========== Global config state ========== */

static volatile Trigger  g_trigger        = TRIGGER_LR;
static volatile int      g_poll_timeout   = 200;
static volatile int      g_drag_threshold = 0;
static volatile BOOL     g_keyboard_hook  = FALSE;
static volatile int      g_target_vk_code = 0x1D; /* VK_NONCONVERT */
static volatile BOOL     g_send_middle_click = FALSE;
static volatile BOOL     g_pass_mode      = FALSE;

/* Scroll state */
static volatile BOOL     g_scroll_mode      = FALSE;
static volatile BOOL     g_scroll_starting  = FALSE;
static volatile BOOL     g_scroll_released  = FALSE;
static volatile DWORD    g_scroll_start_time = 0;
static volatile int      g_scroll_start_x   = 0;
static volatile int      g_scroll_start_y   = 0;
static volatile int      g_scroll_locktime  = 200;
static volatile BOOL     g_cursor_change    = TRUE;
static volatile BOOL     g_reverse_scroll   = FALSE;
static volatile BOOL     g_horizontal_scroll = TRUE;
static volatile BOOL     g_dragged_lock     = FALSE;
static volatile BOOL     g_swap_scroll      = FALSE;

/* Real wheel */
static volatile BOOL     g_real_wheel_mode = FALSE;
static volatile int      g_wheel_delta     = 120;
static volatile int      g_v_wheel_move    = 60;
static volatile int      g_h_wheel_move    = 60;
static volatile BOOL     g_quick_first     = FALSE;
static volatile BOOL     g_quick_turn      = FALSE;

/* Acceleration */
static volatile BOOL     g_accel_table     = TRUE;
static volatile AccelPreset g_accel_preset = ACCEL_PRESET_M5;
static volatile BOOL     g_custom_accel    = FALSE;
static volatile BOOL     g_custom_accel_disabled = TRUE;
static int               g_custom_threshold[64];
static double            g_custom_multiplier[64];
static int               g_custom_accel_count = 0;

/* VH adjuster */
static volatile BOOL     g_vh_adjuster_mode = FALSE;
static volatile VHMethod g_vh_method        = VH_SWITCHING;
static volatile BOOL     g_first_prefer_vertical = TRUE;
static volatile int      g_first_min_threshold   = 5;
static volatile int      g_switching_threshold   = 50;

/* Thresholds */
static volatile int      g_vertical_threshold   = 0;
static volatile int      g_horizontal_threshold = 75;

/* Hook health check */
static volatile int      g_hook_health_check = 0;

/* Filter Keys */
static volatile BOOL     g_filter_keys        = FALSE;
static volatile BOOL     g_fk_lock            = FALSE;
static volatile int      g_fk_acceptance_delay = 1000;
static volatile int      g_fk_repeat_delay    = 1000;
static volatile int      g_fk_repeat_rate     = 500;
static volatile int      g_fk_bounce_time     = 0;
static volatile int      g_kb_repeat_delay    = 1;
static volatile int      g_kb_repeat_speed    = 31;

static volatile Priority g_priority = PRIO_ABOVE_NORMAL;

/* Properties profile */
static wchar_t           g_selected_props[256] = L"Default";
static wchar_t           g_config_dir[MAX_PATH];

/* LastFlags */
static LastFlags         g_last_flags;

/* Scroll state lock */
static CRITICAL_SECTION  g_scroll_cs;

/* Callbacks */
static VoidCallback      g_init_scroll_cb    = NULL;
static VoidCallback      g_change_trigger_cb = NULL;
static VoidCallback      g_init_state_meh_cb = NULL;
static VoidCallback      g_init_state_keh_cb = NULL;

/* ========== Properties file I/O (simple key=value) ========== */

#define MAX_PROPS 128
#define MAX_KEY_LEN 64
#define MAX_VAL_LEN 1024

typedef struct {
    wchar_t key[MAX_KEY_LEN];
    wchar_t value[MAX_VAL_LEN];
} PropEntry;

static PropEntry g_props[MAX_PROPS];
static int       g_prop_count = 0;

static int prop_find(const wchar_t *key) {
    for (int i = 0; i < g_prop_count; i++)
        if (wcscmp(g_props[i].key, key) == 0) return i;
    return -1;
}

static const wchar_t *prop_get(const wchar_t *key) {
    int i = prop_find(key);
    return i >= 0 ? g_props[i].value : NULL;
}

static void prop_set(const wchar_t *key, const wchar_t *value) {
    int i = prop_find(key);
    if (i >= 0) {
        wcsncpy(g_props[i].value, value, MAX_VAL_LEN - 1);
        g_props[i].value[MAX_VAL_LEN - 1] = L'\0';
    } else if (g_prop_count < MAX_PROPS) {
        wcsncpy(g_props[g_prop_count].key, key, MAX_KEY_LEN - 1);
        g_props[g_prop_count].key[MAX_KEY_LEN - 1] = L'\0';
        wcsncpy(g_props[g_prop_count].value, value, MAX_VAL_LEN - 1);
        g_props[g_prop_count].value[MAX_VAL_LEN - 1] = L'\0';
        g_prop_count++;
    }
}

static void prop_clear(void) {
    g_prop_count = 0;
}

/* ========== INI mapping (section + ini_key -> internal_key) ========== */

typedef struct {
    const wchar_t *section;
    const wchar_t *ini_key;
    const wchar_t *internal_key;
} IniMapping;

static const IniMapping g_ini_map[] = {
    /* General */
    { L"General", L"trigger",               L"firstTrigger" },
    { L"General", L"send_middle_click",      L"sendMiddleClick" },
    { L"General", L"dragged_lock",           L"draggedLock" },
    { L"General", L"keyboard_hook",          L"keyboardHook" },
    { L"General", L"vk_code",               L"targetVKCode" },
    { L"General", L"priority",              L"processPriority" },
    { L"General", L"health_check_interval",  L"hookHealthCheck" },
    /* Scroll */
    { L"Scroll", L"cursor_change",          L"cursorChange" },
    { L"Scroll", L"horizontal_scroll",      L"horizontalScroll" },
    { L"Scroll", L"reverse_scroll",         L"reverseScroll" },
    { L"Scroll", L"swap_scroll",            L"swapScroll" },
    { L"Scroll", L"poll_timeout",           L"pollTimeout" },
    { L"Scroll", L"scroll_lock_time",       L"scrollLocktime" },
    { L"Scroll", L"vertical_threshold",     L"verticalThreshold" },
    { L"Scroll", L"horizontal_threshold",   L"horizontalThreshold" },
    { L"Scroll", L"drag_threshold",         L"dragThreshold" },
    /* Acceleration */
    { L"Acceleration", L"accel_table",             L"accelTable" },
    { L"Acceleration", L"multiplier",              L"accelMultiplier" },
    { L"Acceleration", L"custom_accel_table",      L"customAccelTable" },
    { L"Acceleration", L"custom_accel_threshold",  L"customAccelThreshold" },
    { L"Acceleration", L"custom_accel_multiplier", L"customAccelMultiplier" },
    /* Real Wheel */
    { L"Real Wheel", L"real_wheel_mode",    L"realWheelMode" },
    { L"Real Wheel", L"wheel_delta",        L"wheelDelta" },
    { L"Real Wheel", L"vertical_speed",     L"vWheelMove" },
    { L"Real Wheel", L"horizontal_speed",   L"hWheelMove" },
    { L"Real Wheel", L"quick_first",        L"quickFirst" },
    { L"Real Wheel", L"quick_turn",         L"quickTurn" },
    /* VH Adjuster */
    { L"VH Adjuster", L"vh_adjuster_mode",   L"vhAdjusterMode" },
    { L"VH Adjuster", L"method",             L"vhAdjusterMethod" },
    { L"VH Adjuster", L"prefer_vertical",    L"firstPreferVertical" },
    { L"VH Adjuster", L"min_threshold",      L"firstMinThreshold" },
    { L"VH Adjuster", L"switching_threshold", L"switchingThreshold" },
    /* Keyboard */
    { L"Keyboard", L"character_repeat_delay",        L"kbRepeatDelay" },
    { L"Keyboard", L"character_repeat_speed",        L"kbRepeatSpeed" },
    { L"Keyboard", L"filter_keys",                   L"filterKeys" },
    { L"Keyboard", L"filter_keys_lock",              L"fkLock" },
    { L"Keyboard", L"filter_keys_acceptance_delay",  L"fkAcceptanceDelay" },
    { L"Keyboard", L"filter_keys_repeat_delay",      L"fkRepeatDelay" },
    { L"Keyboard", L"filter_keys_repeat_rate",       L"fkRepeatRate" },
    { L"Keyboard", L"filter_keys_bounce_time",       L"fkBounceTime" },
};
#define INI_MAP_COUNT (sizeof(g_ini_map) / sizeof(g_ini_map[0]))

static const wchar_t *INI_SECTIONS[] = {
    L"General", L"Scroll", L"Acceleration", L"Real Wheel", L"VH Adjuster", L"Keyboard"
};
#define INI_SECTION_COUNT (sizeof(INI_SECTIONS) / sizeof(INI_SECTIONS[0]))

static const wchar_t *ini_to_internal(const wchar_t *section, const wchar_t *ini_key) {
    for (int i = 0; i < (int)INI_MAP_COUNT; i++) {
        if (wcscmp(g_ini_map[i].section, section) == 0 &&
            wcscmp(g_ini_map[i].ini_key, ini_key) == 0)
            return g_ini_map[i].internal_key;
    }
    return NULL;
}

static void prop_load(const wchar_t *path) {
    FILE *f = _wfopen(path, L"r");
    if (!f) return;

    char line[2048];
    wchar_t section[64] = L"";

    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        nl = strchr(line, '\r');
        if (nl) *nl = '\0';

        /* Skip leading whitespace */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == ';' || *p == '\0') continue;

        /* Section header */
        if (*p == '[') {
            char *end = strchr(p, ']');
            if (end) {
                int len = (int)(end - p - 1);
                if (len > 0 && len < 64)
                    MultiByteToWideChar(CP_UTF8, 0, p + 1, len, section, 63);
                section[len] = L'\0';
            }
            continue;
        }

        /* Need a current section */
        if (section[0] == L'\0') continue;

        /* Split on '=' */
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';

        /* Trim key */
        char *k = p; while (*k == ' ') k++;
        size_t klen = strlen(k);
        if (klen > 0) { char *ke = k + klen - 1; while (ke > k && *ke == ' ') *ke-- = '\0'; }

        /* Trim value */
        char *v = eq + 1; while (*v == ' ') v++;
        size_t vlen = strlen(v);
        if (vlen > 0) { char *ve = v + vlen - 1; while (ve > v && *ve == ' ') *ve-- = '\0'; }

        if (!*k || !*v) continue;

        /* Convert to wide and translate */
        wchar_t wkey[MAX_KEY_LEN], wval[MAX_VAL_LEN];
        MultiByteToWideChar(CP_UTF8, 0, k, -1, wkey, MAX_KEY_LEN);
        MultiByteToWideChar(CP_UTF8, 0, v, -1, wval, MAX_VAL_LEN);

        const wchar_t *internal = ini_to_internal(section, wkey);
        if (internal)
            prop_set(internal, wval);
    }
    fclose(f);
}

static void prop_store(const wchar_t *path) {
    wchar_t tmp[MAX_PATH + 8];
    _snwprintf(tmp, MAX_PATH + 8, L"%s.tmp", path);

    FILE *f = _wfopen(tmp, L"w");
    if (!f) return;

    for (int s = 0; s < (int)INI_SECTION_COUNT; s++) {
        BOOL header_written = FALSE;
        for (int i = 0; i < (int)INI_MAP_COUNT; i++) {
            if (wcscmp(g_ini_map[i].section, INI_SECTIONS[s]) != 0) continue;
            const wchar_t *v = prop_get(g_ini_map[i].internal_key);
            if (!v) continue;
            if (!header_written) {
                if (s > 0) fprintf(f, "\n");
                fprintf(f, "[%ls]\n", INI_SECTIONS[s]);
                header_written = TRUE;
            }
            fprintf(f, "%ls=%ls\n", g_ini_map[i].ini_key, v);
        }
    }

    fclose(f);

    if (!MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING))
        DeleteFileW(tmp);
}

/* ========== Path helpers ========== */

void cfg_get_properties_path(const wchar_t *name, wchar_t *buf, int bufsize) {
    if (wcscmp(name, L"Default") == 0)
        _snwprintf(buf, bufsize, L"%s\\tpkb.ini", g_config_dir);
    else
        _snwprintf(buf, bufsize, L"%s\\tpkb.%s.ini", g_config_dir, name);
    buf[bufsize - 1] = L'\0';
}

BOOL cfg_properties_exists(const wchar_t *name) {
    wchar_t path[MAX_PATH];
    cfg_get_properties_path(name, path, MAX_PATH);
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

void cfg_properties_copy(const wchar_t *src, const wchar_t *dest) {
    wchar_t sp[MAX_PATH], dp[MAX_PATH];
    cfg_get_properties_path(src, sp, MAX_PATH);
    cfg_get_properties_path(dest, dp, MAX_PATH);
    CopyFileW(sp, dp, TRUE);
}

void cfg_properties_delete(const wchar_t *name) {
    wchar_t path[MAX_PATH];
    cfg_get_properties_path(name, path, MAX_PATH);
    DeleteFileW(path);
}

int cfg_get_prop_files(wchar_t names[][256], int maxcount) {
    wchar_t pattern[MAX_PATH];
    _snwprintf(pattern, MAX_PATH, L"%s\\tpkb.*.ini", g_config_dir);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    int count = 0;
    do {
        /* Skip Default and --prefixed */
        const wchar_t *fn = fd.cFileName;
        /* Extract name from tpkb.{name}.ini */
        const wchar_t *start = fn + 5; /* skip "tpkb." */
        const wchar_t *end = wcsstr(start, L".ini");
        if (!end || start >= end) continue;
        if (wcsncmp(start, L"--", 2) == 0) continue;

        int len = (int)(end - start);
        if (len <= 0 || len >= 256) continue;
        /* Skip "Default" */
        if (len == 7 && wcsncmp(start, L"Default", 7) == 0) continue;

        wcsncpy(names[count], start, len);
        names[count][len] = L'\0';
        count++;
    } while (FindNextFileW(hFind, &fd) && count < maxcount);

    FindClose(hFind);
    return count;
}

const wchar_t *cfg_get_user_dir(void) { return g_config_dir; }

/* ========== Initialization ========== */

void cfg_init(void) {
    InitializeCriticalSection(&g_scroll_cs);
    memset(&g_last_flags, 0, sizeof(g_last_flags));

    /* Build config dir path and ensure it exists */
    wchar_t home[MAX_PATH];
    ExpandEnvironmentStringsW(L"%USERPROFILE%", home, MAX_PATH);
    wchar_t dotconfig[MAX_PATH];
    _snwprintf(dotconfig, MAX_PATH, L"%s\\.config", home);
    CreateDirectoryW(dotconfig, NULL);
    _snwprintf(g_config_dir, MAX_PATH, L"%s\\tpkb", dotconfig);
    CreateDirectoryW(g_config_dir, NULL);
}

/* ========== Callback setters ========== */

void cfg_set_init_scroll_cb(VoidCallback f) { g_init_scroll_cb = f; }
void cfg_set_change_trigger_cb(VoidCallback f) { g_change_trigger_cb = f; }
void cfg_set_init_state_meh_cb(VoidCallback f) { g_init_state_meh_cb = f; }
void cfg_set_init_state_keh_cb(VoidCallback f) { g_init_state_keh_cb = f; }

/* ========== Trigger getters/setters ========== */

Trigger cfg_get_trigger(void) { return g_trigger; }

void cfg_set_trigger(Trigger t) {
    g_trigger = t;
    if (g_change_trigger_cb) g_change_trigger_cb();
}

void cfg_set_trigger_name(const wchar_t *name) {
    cfg_set_trigger(trigger_from_name(name));
}

int  cfg_get_poll_timeout(void) { return g_poll_timeout; }
int  cfg_get_drag_threshold(void) { return g_drag_threshold; }
BOOL cfg_is_keyboard_hook(void) { return g_keyboard_hook; }
int  cfg_get_target_vk_code(void) { return g_target_vk_code; }
BOOL cfg_is_send_middle_click(void) { return g_send_middle_click; }

BOOL cfg_is_trigger(Trigger t) { return g_trigger == t; }
BOOL cfg_is_trigger_event(MouseEventType t) { return cfg_is_trigger(me_get_trigger(t)); }

BOOL cfg_is_drag_trigger_event(MouseEventType t) {
    return cfg_is_trigger(me_get_drag_trigger(t));
}

BOOL cfg_is_lr_trigger(void)     { return cfg_is_trigger(TRIGGER_LR); }
BOOL cfg_is_single_trigger(void) { return trigger_is_single(g_trigger); }
BOOL cfg_is_double_trigger(void) { return trigger_is_double(g_trigger); }
BOOL cfg_is_drag_trigger(void)   { return trigger_is_drag(g_trigger); }
BOOL cfg_is_none_trigger(void)   { return trigger_is_none(g_trigger); }

BOOL cfg_is_trigger_key(const KeyboardEvent *ke) {
    return ke_vk_code(ke) == g_target_vk_code;
}

/* ========== Pass mode ========== */

BOOL cfg_is_pass_mode(void)   { return g_pass_mode; }
void cfg_set_pass_mode(BOOL b) { g_pass_mode = b; }

/* ========== Scroll state ========== */

BOOL cfg_is_scroll_mode(void) { return g_scroll_mode; }

void cfg_start_scroll(const MSLLHOOKSTRUCT *info) {
    EnterCriticalSection(&g_scroll_cs);
    g_scroll_start_time = info->time;
    g_scroll_start_x = info->pt.x;
    g_scroll_start_y = info->pt.y;

    if (g_init_scroll_cb) g_init_scroll_cb();
    rawinput_register();

    if (g_cursor_change && !trigger_is_drag(g_trigger))
        cursor_change_v();

    g_scroll_mode = TRUE;
    g_scroll_starting = FALSE;
    LeaveCriticalSection(&g_scroll_cs);
}

void cfg_start_scroll_k(const KBDLLHOOKSTRUCT *info) {
    EnterCriticalSection(&g_scroll_cs);
    g_scroll_start_time = info->time;

    POINT pt;
    GetCursorPos(&pt);
    g_scroll_start_x = pt.x;
    g_scroll_start_y = pt.y;

    if (g_init_scroll_cb) g_init_scroll_cb();
    rawinput_register();

    if (g_cursor_change)
        cursor_change_v();

    g_scroll_mode = TRUE;
    g_scroll_starting = FALSE;
    LeaveCriticalSection(&g_scroll_cs);
}

void cfg_exit_scroll(void) {
    EnterCriticalSection(&g_scroll_cs);
    rawinput_unregister();
    g_scroll_mode = FALSE;
    g_scroll_released = FALSE;
    if (g_cursor_change)
        cursor_restore();
    LeaveCriticalSection(&g_scroll_cs);
}

BOOL cfg_check_exit_scroll(DWORD time) {
    DWORD dt = time - g_scroll_start_time;
    return dt > (DWORD)g_scroll_locktime;
}

void cfg_get_scroll_start_point(int *x, int *y) {
    *x = g_scroll_start_x;
    *y = g_scroll_start_y;
}

BOOL cfg_is_released_scroll(void) { return g_scroll_released; }

BOOL cfg_is_pressed_scroll(void) {
    return g_scroll_mode && !g_scroll_released;
}

void cfg_set_released_scroll(void) { g_scroll_released = TRUE; }

void cfg_set_starting_scroll(void) {
    EnterCriticalSection(&g_scroll_cs);
    g_scroll_starting = !g_scroll_mode;
    LeaveCriticalSection(&g_scroll_cs);
}

BOOL cfg_is_starting_scroll(void) { return g_scroll_starting; }

/* ========== Scroll options ========== */

int  cfg_get_scroll_locktime(void)    { return g_scroll_locktime; }
BOOL cfg_is_cursor_change(void)       { return g_cursor_change; }
BOOL cfg_is_reverse_scroll(void)      { return g_reverse_scroll; }
BOOL cfg_is_horizontal_scroll(void)   { return g_horizontal_scroll; }
BOOL cfg_is_dragged_lock(void)        { return g_dragged_lock; }
BOOL cfg_is_swap_scroll(void)         { return g_swap_scroll; }

/* ========== Real wheel ========== */

BOOL cfg_is_real_wheel_mode(void) { return g_real_wheel_mode; }
int  cfg_get_wheel_delta(void)    { return g_wheel_delta; }
int  cfg_get_v_wheel_move(void)   { return g_v_wheel_move; }
int  cfg_get_h_wheel_move(void)   { return g_h_wheel_move; }
BOOL cfg_is_quick_first(void)     { return g_quick_first; }
BOOL cfg_is_quick_turn(void)      { return g_quick_turn; }

/* ========== Acceleration ========== */

BOOL cfg_is_accel_table(void) { return g_accel_table; }
AccelPreset cfg_get_accel_preset(void) { return g_accel_preset; }
BOOL cfg_is_custom_accel(void) { return g_custom_accel; }

const int *cfg_get_accel_threshold(int *count) {
    if (!g_custom_accel_disabled && g_custom_accel) {
        *count = g_custom_accel_count;
        return g_custom_threshold;
    }
    *count = ACCEL_TABLE_SIZE;
    return DEFAULT_ACCEL_THRESHOLD;
}

const double *cfg_get_accel_multiplier(int *count) {
    if (!g_custom_accel_disabled && g_custom_accel) {
        *count = g_custom_accel_count;
        return g_custom_multiplier;
    }
    *count = ACCEL_TABLE_SIZE;
    return accel_preset_array(g_accel_preset);
}

void cfg_get_custom_accel_strings(wchar_t *thr_buf, int thr_size,
                                  wchar_t *mul_buf, int mul_size) {
    thr_buf[0] = L'\0';
    mul_buf[0] = L'\0';
    if (g_custom_accel_count == 0) return;

    int thr_off = 0, mul_off = 0;
    for (int i = 0; i < g_custom_accel_count; i++) {
        int n;
        if (i > 0) {
            if (thr_off < thr_size - 1)
                { n = _snwprintf(thr_buf + thr_off, thr_size - thr_off, L","); if (n > 0) thr_off += n; }
            if (mul_off < mul_size - 1)
                { n = _snwprintf(mul_buf + mul_off, mul_size - mul_off, L","); if (n > 0) mul_off += n; }
        }
        if (thr_off < thr_size - 1)
            { n = _snwprintf(thr_buf + thr_off, thr_size - thr_off, L"%d", g_custom_threshold[i]); if (n > 0) thr_off += n; }
        if (mul_off < mul_size - 1)
            { n = _snwprintf(mul_buf + mul_off, mul_size - mul_off, L"%.1f", g_custom_multiplier[i]); if (n > 0) mul_off += n; }
    }
    thr_buf[thr_size - 1] = L'\0';
    mul_buf[mul_size - 1] = L'\0';
}

BOOL cfg_set_custom_accel_strings(const wchar_t *thresholds, const wchar_t *multipliers) {
    wchar_t tbuf[1024], mbuf[1024];
    wcsncpy(tbuf, thresholds, 1023); tbuf[1023] = L'\0';
    wcsncpy(mbuf, multipliers, 1023); mbuf[1023] = L'\0';

    int tc = 0, mc = 0;
    int tmp_thr[64];
    double tmp_mul[64];
    wchar_t *ctx;
    wchar_t *tok = wcstok(tbuf, L",", &ctx);
    while (tok && tc < 64) {
        tmp_thr[tc++] = _wtoi(tok);
        tok = wcstok(NULL, L",", &ctx);
    }
    tok = wcstok(mbuf, L",", &ctx);
    while (tok && mc < 64) {
        tmp_mul[mc++] = _wcstod_l(tok, NULL, get_c_locale());
        tok = wcstok(NULL, L",", &ctx);
    }

    if (tc > 0 && tc == mc) {
        memcpy(g_custom_threshold, tmp_thr, tc * sizeof(int));
        memcpy(g_custom_multiplier, tmp_mul, tc * sizeof(double));
        g_custom_accel_count = tc;
        g_custom_accel_disabled = FALSE;
        prop_set(L"customAccelThreshold", thresholds);
        prop_set(L"customAccelMultiplier", multipliers);
        return TRUE;
    }
    return FALSE;
}

/* ========== VH adjuster ========== */

BOOL cfg_is_vh_adjuster_mode(void) {
    return g_horizontal_scroll && g_vh_adjuster_mode;
}
BOOL cfg_is_vh_adjuster_switching(void) { return g_vh_method == VH_SWITCHING; }
BOOL cfg_is_first_prefer_vertical(void) { return g_first_prefer_vertical; }
int  cfg_get_first_min_threshold(void)  { return g_first_min_threshold; }
int  cfg_get_switching_threshold(void)  { return g_switching_threshold; }

/* ========== Thresholds ========== */

int cfg_get_vertical_threshold(void)   { return g_vertical_threshold; }
int cfg_get_horizontal_threshold(void) { return g_horizontal_threshold; }
int cfg_get_hook_health_check(void)    { return g_hook_health_check; }

/* ========== Priority ========== */

Priority cfg_get_priority(void) { return g_priority; }

void cfg_set_priority_name(const wchar_t *name) {
    g_priority = priority_from_name(name);
    util_set_priority(g_priority);
}

void cfg_set_accel_multiplier_name(const wchar_t *name) {
    g_accel_preset = accel_preset_from_name(name);
}

void cfg_set_vk_code_name(const wchar_t *name) {
    g_target_vk_code = vk_code_from_name(name);
}

void cfg_set_vh_method_name(const wchar_t *name) {
    g_vh_method = vh_method_from_name(name);
}

/* ========== LastFlags ========== */

LastFlags *cfg_last_flags(void) { return &g_last_flags; }

void cfg_last_flags_init(void) {
    memset(&g_last_flags, 0, sizeof(g_last_flags));
}

void cfg_last_flags_set_resent(const MouseEvent *me) {
    if (me->type == ME_LEFT_DOWN) g_last_flags.ld_resent = TRUE;
    else if (me->type == ME_RIGHT_DOWN) g_last_flags.rd_resent = TRUE;
}

BOOL cfg_last_flags_get_reset_resent(const MouseEvent *me) {
    BOOL *flag = NULL;
    if (me->type == ME_LEFT_UP) flag = (BOOL *)&g_last_flags.ld_resent;
    else if (me->type == ME_RIGHT_UP) flag = (BOOL *)&g_last_flags.rd_resent;
    if (!flag) return FALSE;
    BOOL val = *flag; *flag = FALSE; return val;
}

void cfg_last_flags_set_passed(const MouseEvent *me) {
    if (me->type == ME_LEFT_DOWN) g_last_flags.ld_passed = TRUE;
    else if (me->type == ME_RIGHT_DOWN) g_last_flags.rd_passed = TRUE;
}

BOOL cfg_last_flags_get_reset_passed(const MouseEvent *me) {
    BOOL *flag = NULL;
    if (me->type == ME_LEFT_UP) flag = (BOOL *)&g_last_flags.ld_passed;
    else if (me->type == ME_RIGHT_UP) flag = (BOOL *)&g_last_flags.rd_passed;
    if (!flag) return FALSE;
    BOOL val = *flag; *flag = FALSE; return val;
}

void cfg_last_flags_set_suppressed(const MouseEvent *me) {
    switch (me->type) {
    case ME_LEFT_DOWN: g_last_flags.ld_suppressed = TRUE; break;
    case ME_RIGHT_DOWN: g_last_flags.rd_suppressed = TRUE; break;
    case ME_MIDDLE_DOWN: case ME_X1_DOWN: case ME_X2_DOWN:
        g_last_flags.sd_suppressed = TRUE; break;
    default: break;
    }
}

void cfg_last_flags_set_suppressed_k(const KeyboardEvent *ke) {
    if (ke->type == KE_KEY_DOWN)
        g_last_flags.kd_suppressed[ke_vk_code(ke) & 0xFF] = TRUE;
}

BOOL cfg_last_flags_get_reset_suppressed(const MouseEvent *me) {
    BOOL *flag = NULL;
    switch (me->type) {
    case ME_LEFT_UP: flag = (BOOL *)&g_last_flags.ld_suppressed; break;
    case ME_RIGHT_UP: flag = (BOOL *)&g_last_flags.rd_suppressed; break;
    case ME_MIDDLE_UP: case ME_X1_UP: case ME_X2_UP:
        flag = (BOOL *)&g_last_flags.sd_suppressed; break;
    default: return FALSE;
    }
    BOOL val = *flag; *flag = FALSE; return val;
}

BOOL cfg_last_flags_get_reset_suppressed_k(const KeyboardEvent *ke) {
    if (ke->type != KE_KEY_UP) return FALSE;
    int idx = ke_vk_code(ke) & 0xFF;
    BOOL val = g_last_flags.kd_suppressed[idx];
    g_last_flags.kd_suppressed[idx] = FALSE;
    return val;
}

void cfg_last_flags_reset_lr(const MouseEvent *me) {
    if (me->type == ME_LEFT_DOWN) {
        g_last_flags.ld_resent = FALSE;
        g_last_flags.ld_suppressed = FALSE;
        g_last_flags.ld_passed = FALSE;
    } else if (me->type == ME_RIGHT_DOWN) {
        g_last_flags.rd_resent = FALSE;
        g_last_flags.rd_suppressed = FALSE;
        g_last_flags.rd_passed = FALSE;
    }
}

/* ========== Number settings by name ========== */

int cfg_get_number(const wchar_t *name) {
    if (wcscmp(name, L"pollTimeout") == 0) return g_poll_timeout;
    if (wcscmp(name, L"scrollLocktime") == 0) return g_scroll_locktime;
    if (wcscmp(name, L"verticalThreshold") == 0) return g_vertical_threshold;
    if (wcscmp(name, L"horizontalThreshold") == 0) return g_horizontal_threshold;
    if (wcscmp(name, L"wheelDelta") == 0) return g_wheel_delta;
    if (wcscmp(name, L"vWheelMove") == 0) return g_v_wheel_move;
    if (wcscmp(name, L"hWheelMove") == 0) return g_h_wheel_move;
    if (wcscmp(name, L"firstMinThreshold") == 0) return g_first_min_threshold;
    if (wcscmp(name, L"switchingThreshold") == 0) return g_switching_threshold;
    if (wcscmp(name, L"dragThreshold") == 0) return g_drag_threshold;
    if (wcscmp(name, L"hookHealthCheck") == 0) return g_hook_health_check;
    if (wcscmp(name, L"fkAcceptanceDelay") == 0) return g_fk_acceptance_delay;
    if (wcscmp(name, L"fkRepeatDelay") == 0) return g_fk_repeat_delay;
    if (wcscmp(name, L"fkRepeatRate") == 0) return g_fk_repeat_rate;
    if (wcscmp(name, L"fkBounceTime") == 0) return g_fk_bounce_time;
    if (wcscmp(name, L"kbRepeatDelay") == 0) return g_kb_repeat_delay;
    if (wcscmp(name, L"kbRepeatSpeed") == 0) return g_kb_repeat_speed;
    return 0;
}

void cfg_set_number(const wchar_t *name, int n) {
    if (wcscmp(name, L"pollTimeout") == 0) g_poll_timeout = n;
    else if (wcscmp(name, L"scrollLocktime") == 0) g_scroll_locktime = n;
    else if (wcscmp(name, L"verticalThreshold") == 0) g_vertical_threshold = n;
    else if (wcscmp(name, L"horizontalThreshold") == 0) g_horizontal_threshold = n;
    else if (wcscmp(name, L"wheelDelta") == 0) g_wheel_delta = n;
    else if (wcscmp(name, L"vWheelMove") == 0) g_v_wheel_move = n;
    else if (wcscmp(name, L"hWheelMove") == 0) g_h_wheel_move = n;
    else if (wcscmp(name, L"firstMinThreshold") == 0) g_first_min_threshold = n;
    else if (wcscmp(name, L"switchingThreshold") == 0) g_switching_threshold = n;
    else if (wcscmp(name, L"dragThreshold") == 0) g_drag_threshold = n;
    else if (wcscmp(name, L"hookHealthCheck") == 0) g_hook_health_check = n;
    else if (wcscmp(name, L"fkAcceptanceDelay") == 0) g_fk_acceptance_delay = n;
    else if (wcscmp(name, L"fkRepeatDelay") == 0) g_fk_repeat_delay = n;
    else if (wcscmp(name, L"fkRepeatRate") == 0) g_fk_repeat_rate = n;
    else if (wcscmp(name, L"fkBounceTime") == 0) g_fk_bounce_time = n;
    else if (wcscmp(name, L"kbRepeatDelay") == 0) g_kb_repeat_delay = n;
    else if (wcscmp(name, L"kbRepeatSpeed") == 0) g_kb_repeat_speed = n;
}

/* ========== Boolean settings by name ========== */

BOOL cfg_get_boolean(const wchar_t *name) {
    if (wcscmp(name, L"realWheelMode") == 0) return g_real_wheel_mode;
    if (wcscmp(name, L"cursorChange") == 0) return g_cursor_change;
    if (wcscmp(name, L"horizontalScroll") == 0) return g_horizontal_scroll;
    if (wcscmp(name, L"reverseScroll") == 0) return g_reverse_scroll;
    if (wcscmp(name, L"quickFirst") == 0) return g_quick_first;
    if (wcscmp(name, L"quickTurn") == 0) return g_quick_turn;
    if (wcscmp(name, L"accelTable") == 0) return g_accel_table;
    if (wcscmp(name, L"customAccelTable") == 0) return g_custom_accel;
    if (wcscmp(name, L"draggedLock") == 0) return g_dragged_lock;
    if (wcscmp(name, L"swapScroll") == 0) return g_swap_scroll;
    if (wcscmp(name, L"sendMiddleClick") == 0) return g_send_middle_click;
    if (wcscmp(name, L"keyboardHook") == 0) return g_keyboard_hook;
    if (wcscmp(name, L"vhAdjusterMode") == 0) return g_vh_adjuster_mode;
    if (wcscmp(name, L"firstPreferVertical") == 0) return g_first_prefer_vertical;
    if (wcscmp(name, L"passMode") == 0) return g_pass_mode;
    if (wcscmp(name, L"filterKeys") == 0) return g_filter_keys;
    if (wcscmp(name, L"fkLock") == 0) return g_fk_lock;
    return FALSE;
}

void cfg_set_boolean(const wchar_t *name, BOOL b) {
    if (wcscmp(name, L"realWheelMode") == 0) g_real_wheel_mode = b;
    else if (wcscmp(name, L"cursorChange") == 0) g_cursor_change = b;
    else if (wcscmp(name, L"horizontalScroll") == 0) g_horizontal_scroll = b;
    else if (wcscmp(name, L"reverseScroll") == 0) g_reverse_scroll = b;
    else if (wcscmp(name, L"quickFirst") == 0) g_quick_first = b;
    else if (wcscmp(name, L"quickTurn") == 0) g_quick_turn = b;
    else if (wcscmp(name, L"accelTable") == 0) g_accel_table = b;
    else if (wcscmp(name, L"customAccelTable") == 0) g_custom_accel = b;
    else if (wcscmp(name, L"draggedLock") == 0) g_dragged_lock = b;
    else if (wcscmp(name, L"swapScroll") == 0) g_swap_scroll = b;
    else if (wcscmp(name, L"sendMiddleClick") == 0) g_send_middle_click = b;
    else if (wcscmp(name, L"keyboardHook") == 0) g_keyboard_hook = b;
    else if (wcscmp(name, L"vhAdjusterMode") == 0) g_vh_adjuster_mode = b;
    else if (wcscmp(name, L"firstPreferVertical") == 0) g_first_prefer_vertical = b;
    else if (wcscmp(name, L"passMode") == 0) g_pass_mode = b;
    else if (wcscmp(name, L"filterKeys") == 0) g_filter_keys = b;
    else if (wcscmp(name, L"fkLock") == 0) g_fk_lock = b;
}

/* ========== Properties I/O ========== */

static const wchar_t *BOOLEAN_NAMES[] = {
    L"realWheelMode", L"cursorChange", L"horizontalScroll", L"reverseScroll",
    L"quickFirst", L"quickTurn", L"accelTable", L"customAccelTable",
    L"draggedLock", L"swapScroll", L"sendMiddleClick", L"keyboardHook",
    L"vhAdjusterMode", L"firstPreferVertical",
    L"filterKeys", L"fkLock"
};
#define BOOLEAN_COUNT (sizeof(BOOLEAN_NAMES) / sizeof(BOOLEAN_NAMES[0]))

typedef struct { const wchar_t *name; int low; int up; } NumberRange;
static const NumberRange NUMBER_RANGES[] = {
    { L"pollTimeout", 50, 500 },
    { L"scrollLocktime", 150, 500 },
    { L"verticalThreshold", 0, 500 },
    { L"horizontalThreshold", 0, 500 },
    { L"wheelDelta", 10, 500 },
    { L"vWheelMove", 10, 500 },
    { L"hWheelMove", 10, 500 },
    { L"firstMinThreshold", 1, 10 },
    { L"switchingThreshold", 10, 500 },
    { L"dragThreshold", 0, 500 },
    { L"hookHealthCheck", 0, 300 },
    { L"fkAcceptanceDelay", 0, 10000 },
    { L"fkRepeatDelay", 0, 10000 },
    { L"fkRepeatRate", 0, 10000 },
    { L"fkBounceTime", 0, 10000 },
    { L"kbRepeatDelay", 0, 3 },
    { L"kbRepeatSpeed", 0, 31 },
};
#define NUMBER_COUNT (sizeof(NUMBER_RANGES) / sizeof(NUMBER_RANGES[0]))

void cfg_set_selected_properties(const wchar_t *name) {
    wcsncpy(g_selected_props, name, 255);
    g_selected_props[255] = L'\0';
}

const wchar_t *cfg_get_selected_properties(void) {
    return g_selected_props;
}

static void apply_string_prop(const wchar_t *key, void (*setter)(const wchar_t *)) {
    const wchar_t *v = prop_get(key);
    if (v) setter(v);
}

static void apply_bool_props(void) {
    for (int i = 0; i < (int)BOOLEAN_COUNT; i++) {
        const wchar_t *v = prop_get(BOOLEAN_NAMES[i]);
        if (v) cfg_set_boolean(BOOLEAN_NAMES[i], _wcsicmp(v, L"True") == 0);
    }
}

static void apply_number_props(void) {
    for (int i = 0; i < (int)NUMBER_COUNT; i++) {
        const wchar_t *v = prop_get(NUMBER_RANGES[i].name);
        if (v) {
            int n = _wtoi(v);
            if (n >= NUMBER_RANGES[i].low && n <= NUMBER_RANGES[i].up)
                cfg_set_number(NUMBER_RANGES[i].name, n);
        }
    }
}

static void apply_custom_accel(void) {
    const wchar_t *ts = prop_get(L"customAccelThreshold");
    const wchar_t *ms = prop_get(L"customAccelMultiplier");
    if (!ts || !ms) return;

    /* Parse comma-separated int array */
    wchar_t tbuf[1024], mbuf[1024];
    wcsncpy(tbuf, ts, 1023); tbuf[1023] = L'\0';
    wcsncpy(mbuf, ms, 1023); mbuf[1023] = L'\0';

    int tc = 0, mc = 0;
    wchar_t *ctx;
    wchar_t *tok = wcstok(tbuf, L",", &ctx);
    while (tok && tc < 64) {
        g_custom_threshold[tc++] = _wtoi(tok);
        tok = wcstok(NULL, L",", &ctx);
    }
    tok = wcstok(mbuf, L",", &ctx);
    while (tok && mc < 64) {
        g_custom_multiplier[mc++] = _wcstod_l(tok, NULL, get_c_locale());
        tok = wcstok(NULL, L",", &ctx);
    }

    if (tc > 0 && tc == mc) {
        g_custom_accel_count = tc;
        g_custom_accel_disabled = FALSE;
    }
}

static void cfg_set_defaults(void) {
    /* String settings — use setters that fire callbacks */
    cfg_set_trigger(TRIGGER_LR);
    g_accel_preset = ACCEL_PRESET_M5;
    g_target_vk_code = 0x1D;
    g_vh_method = VH_SWITCHING;

    /* Booleans (match compile-time initializers) */
    g_real_wheel_mode = FALSE;
    g_cursor_change = TRUE;
    g_horizontal_scroll = TRUE;
    g_reverse_scroll = FALSE;
    g_quick_first = FALSE;
    g_quick_turn = FALSE;
    g_accel_table = TRUE;
    g_custom_accel = FALSE;
    g_dragged_lock = FALSE;
    g_swap_scroll = FALSE;
    g_send_middle_click = FALSE;
    g_keyboard_hook = FALSE;
    g_vh_adjuster_mode = FALSE;
    g_first_prefer_vertical = TRUE;
    g_filter_keys = FALSE;
    g_fk_lock = FALSE;

    /* Numbers (match compile-time initializers) */
    g_poll_timeout = 200;
    g_scroll_locktime = 200;
    g_vertical_threshold = 0;
    g_horizontal_threshold = 75;
    g_wheel_delta = 120;
    g_v_wheel_move = 60;
    g_h_wheel_move = 60;
    g_first_min_threshold = 5;
    g_switching_threshold = 50;
    g_drag_threshold = 0;
    g_hook_health_check = 0;
    g_fk_acceptance_delay = 1000;
    g_fk_repeat_delay = 1000;
    g_fk_repeat_rate = 500;
    g_fk_bounce_time = 0;
    g_kb_repeat_delay = 1;
    g_kb_repeat_speed = 31;

    /* Custom accel — disable, clear count */
    g_custom_accel_disabled = TRUE;
    g_custom_accel_count = 0;

    /* Priority — use setter for side effect (util_set_priority) */
    cfg_set_priority_name(L"AboveNormal");
}

void cfg_load_properties_file_only(void) {
    wchar_t path[MAX_PATH];
    cfg_get_properties_path(g_selected_props, path, MAX_PATH);
    prop_clear();
    prop_load(path);
}

void cfg_load_properties(BOOL update) {
    wchar_t path[MAX_PATH];
    cfg_get_properties_path(g_selected_props, path, MAX_PATH);

    if (update) {
        prop_clear();
        cfg_set_defaults();
    }
    prop_load(path);

    apply_string_prop(L"firstTrigger", cfg_set_trigger_name);
    apply_string_prop(L"accelMultiplier", cfg_set_accel_multiplier_name);
    apply_custom_accel();
    apply_string_prop(L"processPriority", cfg_set_priority_name);
    apply_string_prop(L"targetVKCode", cfg_set_vk_code_name);
    apply_string_prop(L"vhAdjusterMethod", cfg_set_vh_method_name);
    apply_bool_props();
    apply_number_props();

    /* Set default priority if not specified */
    if (!prop_get(L"processPriority"))
        cfg_set_priority_name(L"AboveNormal");
}

void cfg_store_properties(void) {
    wchar_t buf[32];

    /* Strings */
    prop_set(L"firstTrigger", trigger_to_name(g_trigger));
    prop_set(L"accelMultiplier", accel_preset_to_name(g_accel_preset));
    prop_set(L"processPriority", priority_to_name(g_priority));
    prop_set(L"targetVKCode", vk_name_from_code(g_target_vk_code));
    prop_set(L"vhAdjusterMethod", vh_method_to_name(g_vh_method));
    /* Booleans */
    for (int i = 0; i < (int)BOOLEAN_COUNT; i++)
        prop_set(BOOLEAN_NAMES[i], cfg_get_boolean(BOOLEAN_NAMES[i]) ? L"True" : L"False");

    /* Numbers */
    for (int i = 0; i < (int)NUMBER_COUNT; i++) {
        _snwprintf(buf, 32, L"%d", cfg_get_number(NUMBER_RANGES[i].name));
        prop_set(NUMBER_RANGES[i].name, buf);
    }

    wchar_t path[MAX_PATH];
    cfg_get_properties_path(g_selected_props, path, MAX_PATH);
    prop_store(path);
}

void cfg_reload_properties(void) {
    prop_clear();
    cfg_load_properties(TRUE);
}

/* ========== Init state (reset all tracking) ========== */

void cfg_init_state(void) {
    if (g_init_state_meh_cb) g_init_state_meh_cb();
    if (g_init_state_keh_cb) g_init_state_keh_cb();
    cfg_last_flags_init();
    cfg_exit_scroll();
}

void cfg_exit_action(void) {
    /* Will be implemented by tray module - post WM_QUIT */
    PostQuitMessage(0);
}
