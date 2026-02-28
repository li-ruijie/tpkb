/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "scroll.h"
#include "config.h"
#include "cursor.h"
#include "rawinput.h"
#include <math.h>
#include <process.h>

/* ========== Async input queue (sender thread) ========== */

typedef struct {
    INPUT msg;
} InputItem;

#define INPUT_QUEUE_SIZE 256

static InputItem g_input_queue[INPUT_QUEUE_SIZE];
static volatile LONG g_iq_head = 0;
static volatile LONG g_iq_tail = 0;
static HANDLE g_iq_sem = NULL;       /* counts available items */
static HANDLE g_iq_space_sem = NULL; /* counts available space */
static HANDLE g_sender_thread = NULL;
static volatile BOOL g_sender_running = FALSE;
static CRITICAL_SECTION g_iq_cs;

static void enqueue_input(const INPUT *inp) {
    if (WaitForSingleObject(g_iq_space_sem, 0) != WAIT_OBJECT_0)
        return;
    EnterCriticalSection(&g_iq_cs);
    g_input_queue[g_iq_head].msg = *inp;
    g_iq_head = (g_iq_head + 1) % INPUT_QUEUE_SIZE;
    ReleaseSemaphore(g_iq_sem, 1, NULL);
    LeaveCriticalSection(&g_iq_cs);
}

static BOOL enqueue_inputs(const INPUT *msgs, int count) {
    for (int i = 0; i < count; i++) {
        if (WaitForSingleObject(g_iq_space_sem, 0) != WAIT_OBJECT_0) {
            if (i > 0) ReleaseSemaphore(g_iq_space_sem, i, NULL);
            return FALSE;
        }
    }
    EnterCriticalSection(&g_iq_cs);
    LONG head = g_iq_head;
    for (int i = 0; i < count; i++) {
        g_input_queue[head].msg = msgs[i];
        head = (head + 1) % INPUT_QUEUE_SIZE;
    }
    g_iq_head = head;
    ReleaseSemaphore(g_iq_sem, count, NULL);
    LeaveCriticalSection(&g_iq_cs);
    return TRUE;
}

static unsigned __stdcall sender_proc(void *arg) {
    (void)arg;
    INPUT batch[INPUT_QUEUE_SIZE];
    while (g_sender_running) {
        WaitForSingleObject(g_iq_sem, INFINITE);
        if (!g_sender_running) break;
        LONG tail = g_iq_tail;
        int count = 0;
        batch[count++] = g_input_queue[tail].msg;
        tail = (tail + 1) % INPUT_QUEUE_SIZE;
        while (count < INPUT_QUEUE_SIZE &&
               WaitForSingleObject(g_iq_sem, 0) == WAIT_OBJECT_0) {
            batch[count++] = g_input_queue[tail].msg;
            tail = (tail + 1) % INPUT_QUEUE_SIZE;
        }
        InterlockedExchange(&g_iq_tail, tail);
        ReleaseSemaphore(g_iq_space_sem, count, NULL);
        SendInput((UINT)count, batch, sizeof(INPUT));
    }
    return 0;
}

/* ========== Fixed tags for identifying resent events ========== */

static const DWORD g_resend_tag = 0x57313057;       /* ASCII "W10W" */
static const DWORD g_resend_click_tag = 0x57314357;  /* ASCII "W1CW" */

/* ========== Event detection ========== */

BOOL scroll_is_injected(const MouseEvent *me) {
    return me->info.flags == 1 || me->info.flags == 2;
}

BOOL scroll_is_resend(const MouseEvent *me) {
    return (DWORD)(ULONG_PTR)me->info.dwExtraInfo == g_resend_tag;
}

BOOL scroll_is_resend_click(const MouseEvent *me) {
    return (DWORD)(ULONG_PTR)me->info.dwExtraInfo == g_resend_click_tag;
}

/* ========== Input creation ========== */

static INPUT create_input(POINT pt, int data, int flags, DWORD time, DWORD extra) {
    INPUT inp;
    inp.type = INPUT_MOUSE;
    inp.mi.dx = pt.x;
    inp.mi.dy = pt.y;
    inp.mi.mouseData = (DWORD)data;
    inp.mi.dwFlags = (DWORD)flags;
    inp.mi.time = time;
    inp.mi.dwExtraInfo = (ULONG_PTR)extra;
    return inp;
}

void scroll_send_input(POINT pt, int data, int flags, DWORD time, DWORD extra) {
    INPUT inp = create_input(pt, data, flags, time, extra);
    enqueue_input(&inp);
}

/* ========== Click resend ========== */

void scroll_resend_click(MouseClickType type, const MSLLHOOKSTRUCT *info) {
    DWORD extra = g_resend_click_tag;
    int down_flag, up_flag, mouse_data = 0;

    switch (type) {
    case MC_LEFT:   down_flag = TPKB_MOUSEEVENTF_LEFTDOWN;  up_flag = TPKB_MOUSEEVENTF_LEFTUP;    break;
    case MC_RIGHT:  down_flag = TPKB_MOUSEEVENTF_RIGHTDOWN; up_flag = TPKB_MOUSEEVENTF_RIGHTUP;   break;
    case MC_MIDDLE: down_flag = TPKB_MOUSEEVENTF_MIDDLEDOWN; up_flag = TPKB_MOUSEEVENTF_MIDDLEUP; break;
    case MC_X1:     down_flag = TPKB_MOUSEEVENTF_XDOWN; up_flag = TPKB_MOUSEEVENTF_XUP; mouse_data = TPKB_XBUTTON1; break;
    case MC_X2:     down_flag = TPKB_MOUSEEVENTF_XDOWN; up_flag = TPKB_MOUSEEVENTF_XUP; mouse_data = TPKB_XBUTTON2; break;
    default: return;
    }

    INPUT msgs[2];
    msgs[0] = create_input(info->pt, mouse_data, down_flag, 0, extra);
    msgs[1] = create_input(info->pt, mouse_data, up_flag, 0, extra);

    /* Enqueue atomically; drop if queue is full (preferable to blocking hook) */
    enqueue_inputs(msgs, 2);
}

void scroll_resend_down(const MouseEvent *me) {
    int flag;
    switch (me->type) {
    case ME_LEFT_DOWN:  flag = TPKB_MOUSEEVENTF_LEFTDOWN;  break;
    case ME_RIGHT_DOWN: flag = TPKB_MOUSEEVENTF_RIGHTDOWN; break;
    default: return;
    }
    scroll_send_input(me->info.pt, 0, flag, 0, g_resend_tag);
}

void scroll_resend_up(const MouseEvent *me) {
    int flag;
    switch (me->type) {
    case ME_LEFT_UP:  flag = TPKB_MOUSEEVENTF_LEFTUP;  break;
    case ME_RIGHT_UP: flag = TPKB_MOUSEEVENTF_RIGHTUP; break;
    default: return;
    }
    scroll_send_input(me->info.pt, 0, flag, 0, g_resend_tag);
}

/* ========== Modifier key detection ========== */

static BOOL check_async_key(int vk) {
    return (GetAsyncKeyState(vk) & 0xF000) != 0;
}

BOOL scroll_check_shift(void) { return check_async_key(VK_SHIFT); }
BOOL scroll_check_ctrl(void)  { return check_async_key(VK_CONTROL); }
BOOL scroll_check_alt(void)   { return check_async_key(VK_MENU); }
BOOL scroll_check_esc(void)   { return check_async_key(VK_ESCAPE); }

/* ========== Scroll engine state ========== */

static int (*add_accel_fn)(int) = NULL;
static int (*reverse_v_fn)(int) = NULL;
static int (*reverse_h_fn)(int) = NULL;
static int (*reverse_delta_fn)(int) = NULL;

static int pass_int(int d) { return d; }
static int flip_int(int d) { return -d; }

static BOOL swap_enabled = FALSE;

/* Acceleration lookup */
static const int *accel_threshold = NULL;
static const double *accel_multiplier = NULL;
static int accel_count = 0;

static int get_nearest_index(int d) {
    int ad = abs(d);
    for (int i = 0; i < accel_count; i++) {
        if (accel_threshold[i] == ad) return i;
        if (accel_threshold[i] > ad) {
            if (i == 0) return 0;
            return (accel_threshold[i] - ad < abs(accel_threshold[i - 1] - ad)) ? i : i - 1;
        }
    }
    return accel_count - 1;
}

static int add_accel(int d) {
    int i = get_nearest_index(d);
    return (int)((double)d * accel_multiplier[i]);
}

/* Scroll state lock (protects scroll_start, raw_total, prev_d across threads) */
static CRITICAL_SECTION g_scroll_state_cs;

/* Real wheel state */
static int vw_count, hw_count;
static MoveDirection v_last_move, h_last_move;
static int v_wheel_move, h_wheel_move;
static BOOL quick_turn;
static int wheel_delta;
static int scroll_start_x, scroll_start_y;

/* Raw input accumulators */
static int raw_total_x, raw_total_y;

static BOOL is_turn_move(MoveDirection last, int d) {
    if (last == DIR_ZERO) return FALSE;
    if (last == DIR_PLUS) return d < 0;
    return d > 0;
}

static int get_v_wheel_delta(int input) {
    int delta = wheel_delta;
    int res = input > 0 ? -delta : delta;
    return reverse_delta_fn(res);
}

static int get_h_wheel_delta(int input) {
    return -get_v_wheel_delta(input);
}

/* Send wheel functions */
static void send_real_v_wheel(POINT pt, int d) {
    vw_count += abs(d);
    if (quick_turn && is_turn_move(v_last_move, d)) {
        vw_count = abs(d);
        scroll_send_input(pt, get_v_wheel_delta(d), TPKB_MOUSEEVENTF_WHEEL, 0, 0);
    } else while (vw_count >= v_wheel_move) {
        scroll_send_input(pt, get_v_wheel_delta(d), TPKB_MOUSEEVENTF_WHEEL, 0, 0);
        vw_count -= v_wheel_move;
    }
    v_last_move = d > 0 ? DIR_PLUS : DIR_MINUS;
}

static void send_real_h_wheel(POINT pt, int d) {
    hw_count += abs(d);
    if (quick_turn && is_turn_move(h_last_move, d)) {
        hw_count = abs(d);
        scroll_send_input(pt, get_h_wheel_delta(d), TPKB_MOUSEEVENTF_HWHEEL, 0, 0);
    } else while (hw_count >= h_wheel_move) {
        scroll_send_input(pt, get_h_wheel_delta(d), TPKB_MOUSEEVENTF_HWHEEL, 0, 0);
        hw_count -= h_wheel_move;
    }
    h_last_move = d > 0 ? DIR_PLUS : DIR_MINUS;
}

static void send_direct_v_wheel(POINT pt, int d) {
    scroll_send_input(pt, reverse_v_fn(add_accel_fn(d)), TPKB_MOUSEEVENTF_WHEEL, 0, 0);
}

static void send_direct_h_wheel(POINT pt, int d) {
    scroll_send_input(pt, reverse_h_fn(add_accel_fn(d)), TPKB_MOUSEEVENTF_HWHEEL, 0, 0);
}

static void (*send_v_wheel)(POINT, int) = send_direct_v_wheel;
static void (*send_h_wheel)(POINT, int) = send_direct_h_wheel;

/* VH adjuster */
static VHDirection fixed_vhd, latest_vhd;
static int switching_threshold_val;

static VHDirection get_first_vhd(int adx, int ady) {
    int mthr = cfg_get_first_min_threshold();
    if (adx > mthr || ady > mthr) {
        int y = cfg_is_first_prefer_vertical() ? ady * 2 : ady;
        return y >= adx ? VHD_VERTICAL : VHD_HORIZONTAL;
    }
    return VHD_NONE;
}

static VHDirection switch_vhd(int adx, int ady) {
    if (ady > switching_threshold_val) return VHD_VERTICAL;
    if (adx > switching_threshold_val) return VHD_HORIZONTAL;
    return VHD_NONE;
}

static VHDirection switch_vhd_fixed(int adx, int ady) {
    (void)adx; (void)ady;
    return fixed_vhd;
}

static VHDirection (*switch_vhd_fn)(int, int) = switch_vhd;

static void change_cursor_vhd(VHDirection vhd) {
    if (cfg_is_cursor_change()) {
        if (vhd == VHD_VERTICAL) cursor_change_v();
        else if (vhd == VHD_HORIZONTAL) cursor_change_h();
    }
}

static void send_wheel_vha(POINT wspt, int dx, int dy, int fdx, int fdy) {
    int adx = abs(dx), ady = abs(dy);
    VHDirection cur_vhd;

    if (fixed_vhd == VHD_NONE) {
        fixed_vhd = get_first_vhd(adx, ady);
        cur_vhd = fixed_vhd;
    } else {
        cur_vhd = switch_vhd_fn(adx, ady);
    }

    if (cur_vhd != VHD_NONE && cur_vhd != latest_vhd) {
        change_cursor_vhd(cur_vhd);
        latest_vhd = cur_vhd;
    }

    if (latest_vhd == VHD_VERTICAL && fdy != 0) send_v_wheel(wspt, fdy);
    else if (latest_vhd == VHD_HORIZONTAL && fdx != 0) send_h_wheel(wspt, fdx);
}

/* Standard mode thresholds */
static int vert_thr, horiz_thr;
static BOOL horiz_enabled;

static void send_wheel_std(POINT wspt, int dx, int dy, int fdx, int fdy) {
    if (abs(dy) > vert_thr && fdy != 0) send_v_wheel(wspt, fdy);
    if (horiz_enabled && abs(dx) > horiz_thr && fdx != 0) send_h_wheel(wspt, fdx);
}

static void (*send_wheel_fn)(POINT, int, int, int, int) = send_wheel_std;

/* ========== Public scroll function ========== */

static void send_wheel_raw(int x, int y) {
    if (x != 0 || y != 0) {
        EnterCriticalSection(&g_scroll_state_cs);
        raw_total_x += x;
        raw_total_y += y;
        int dx = raw_total_x, dy = raw_total_y;
        int ssx = scroll_start_x, ssy = scroll_start_y;
        LeaveCriticalSection(&g_scroll_state_cs);
        int fdx = x, fdy = y;
        if (swap_enabled) { int t = dx; dx = dy; dy = t; t = fdx; fdx = fdy; fdy = t; }
        POINT wspt;
        wspt.x = ssx;
        wspt.y = ssy;
        send_wheel_fn(wspt, dx, dy, fdx, fdy);
    }
}

/* ========== Init scroll (called when entering scroll mode) ========== */

void scroll_init_scroll(void) {
    EnterCriticalSection(&g_scroll_state_cs);
    cfg_get_scroll_start_point(&scroll_start_x, &scroll_start_y);
    raw_total_x = 0;
    raw_total_y = 0;
    LeaveCriticalSection(&g_scroll_state_cs);

    /* Function pointers */
    add_accel_fn = cfg_is_accel_table() ? add_accel : pass_int;
    swap_enabled = cfg_is_swap_scroll();
    reverse_v_fn = cfg_is_reverse_scroll() ? pass_int : flip_int;
    reverse_h_fn = cfg_is_reverse_scroll() ? flip_int : pass_int;

    send_v_wheel = cfg_is_real_wheel_mode() ? send_real_v_wheel : send_direct_v_wheel;
    send_h_wheel = cfg_is_real_wheel_mode() ? send_real_h_wheel : send_direct_h_wheel;

    send_wheel_fn = (cfg_is_horizontal_scroll() && cfg_is_vh_adjuster_mode()) ?
                    send_wheel_vha : send_wheel_std;

    /* Acceleration */
    if (cfg_is_accel_table()) {
        accel_threshold = cfg_get_accel_threshold(&accel_count);
        accel_multiplier = cfg_get_accel_multiplier(&accel_count);
    }

    /* Real wheel mode */
    if (cfg_is_real_wheel_mode()) {
        v_wheel_move = cfg_get_v_wheel_move();
        h_wheel_move = cfg_get_h_wheel_move();
        quick_turn = cfg_is_quick_turn();
        wheel_delta = cfg_get_wheel_delta();
        reverse_delta_fn = cfg_is_reverse_scroll() ? flip_int : pass_int;
        vw_count = cfg_is_quick_first() ? v_wheel_move : v_wheel_move / 2;
        hw_count = cfg_is_quick_first() ? h_wheel_move : h_wheel_move / 2;
        v_last_move = DIR_ZERO;
        h_last_move = DIR_ZERO;
    }

    /* VH adjuster */
    if (cfg_is_vh_adjuster_mode()) {
        fixed_vhd = VHD_NONE;
        latest_vhd = VHD_NONE;
        switching_threshold_val = cfg_get_switching_threshold();
        switch_vhd_fn = cfg_is_vh_adjuster_switching() ? switch_vhd : switch_vhd_fixed;
    } else {
        vert_thr = cfg_get_vertical_threshold();
        horiz_thr = cfg_get_horizontal_threshold();
        horiz_enabled = cfg_is_horizontal_scroll();
    }
}

/* ========== Init (called once at startup) ========== */

void scroll_init(void) {
    InitializeCriticalSection(&g_iq_cs);
    InitializeCriticalSection(&g_scroll_state_cs);

    add_accel_fn = pass_int;
    reverse_v_fn = flip_int;
    reverse_h_fn = pass_int;
    reverse_delta_fn = flip_int;

    /* Start sender thread */
    g_iq_sem = CreateSemaphoreW(NULL, 0, INPUT_QUEUE_SIZE, NULL);
    g_iq_space_sem = CreateSemaphoreW(NULL, INPUT_QUEUE_SIZE - 1, INPUT_QUEUE_SIZE - 1, NULL);
    g_sender_running = TRUE;
    g_sender_thread = (HANDLE)_beginthreadex(NULL, 0, sender_proc, NULL, 0, NULL);
    SetThreadPriority(g_sender_thread, THREAD_PRIORITY_ABOVE_NORMAL);

    /* Register raw input callback */
    rawinput_set_send_wheel_raw(send_wheel_raw);

    /* Register scroll init callback */
    cfg_set_init_scroll_cb(scroll_init_scroll);
}

void scroll_cleanup(void) {
    g_sender_running = FALSE;
    if (g_iq_sem) ReleaseSemaphore(g_iq_sem, 1, NULL); /* Unblock sender */
    if (g_sender_thread) {
        WaitForSingleObject(g_sender_thread, 2000);
        CloseHandle(g_sender_thread);
        g_sender_thread = NULL;
    }
    if (g_iq_sem) { CloseHandle(g_iq_sem); g_iq_sem = NULL; }
    if (g_iq_space_sem) { CloseHandle(g_iq_space_sem); g_iq_space_sem = NULL; }
    DeleteCriticalSection(&g_iq_cs);
}

