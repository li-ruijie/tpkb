/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#include "waiter.h"
#include "config.h"
#include "scroll.h"
#include <process.h>

/* ========== Synchronous queue (capacity 1) ========== */

static MouseEvent g_offer_slot;
static volatile BOOL g_waiting = FALSE;
static HANDLE g_offer_event = NULL; /* Signaled when an event is offered */

static void sync_set_waiting(void) {
    g_waiting = TRUE;
}

/* Poll: wait for an offered event or timeout. Returns TRUE if event received. */
static BOOL sync_poll(int timeout_ms, MouseEvent *out) {
    DWORD result = WaitForSingleObject(g_offer_event, (DWORD)timeout_ms);
    g_waiting = FALSE;
    if (result == WAIT_OBJECT_0) {
        *out = g_offer_slot;
        return TRUE;
    }
    return FALSE;
}

/* Offer: provide an event to the polling thread. Returns TRUE if accepted. */
static BOOL sync_offer(const MouseEvent *me) {
    if (!g_waiting) return FALSE;

    g_offer_slot = *me;
    SetEvent(g_offer_event);

    /* Spin until the waiter thread clears the waiting flag */
    while (g_waiting)
        Sleep(0);

    return TRUE;
}

/* ========== Waiter thread ========== */

static MouseEvent g_waiting_event;
static volatile BOOL g_has_waiting_event = FALSE;

static HANDLE g_waiter_event = NULL; /* Signaled when a new wait starts */
static HANDLE g_waiter_thread = NULL;

static void set_flags_offer(const MouseEvent *me) {
    if (me->type == ME_MOVE) {
        cfg_last_flags_set_resent(&g_waiting_event);
    } else if (me->type == ME_LEFT_UP || me->type == ME_RIGHT_UP) {
        cfg_last_flags_set_resent(&g_waiting_event);
    } else if (me->type == ME_LEFT_DOWN || me->type == ME_RIGHT_DOWN) {
        cfg_last_flags_set_suppressed(&g_waiting_event);
        cfg_last_flags_set_suppressed(me);
        cfg_set_starting_scroll();
    }
}

static void from_move(const MouseEvent *down) {
    scroll_resend_down(down);
}

static void from_up(const MouseEvent *down, const MouseEvent *up) {
    /* If same point, resend as click; otherwise resend down+up */
    BOOL same = (down->info.pt.x == up->info.pt.x && down->info.pt.y == up->info.pt.y);

    if (same) {
        /* Resend as click */
        if (down->type == ME_LEFT_DOWN)
            scroll_resend_click(MC_LEFT, &down->info);
        else if (down->type == ME_RIGHT_DOWN)
            scroll_resend_click(MC_RIGHT, &down->info);
        else {
            scroll_resend_down(down);
            scroll_resend_up(up);
        }
    } else {
        scroll_resend_down(down);
        scroll_resend_up(up);
    }
}

static void from_down(const MouseEvent *d1, const MouseEvent *d2) {
    (void)d1;
    cfg_start_scroll(&d2->info);
}

static void from_timeout(const MouseEvent *down) {
    cfg_last_flags_set_resent(down);
    scroll_resend_down(down);
}

static void dispatch_event(const MouseEvent *down, const MouseEvent *res) {
    if (res->type == ME_MOVE) {
        from_move(down);
    } else if (res->type == ME_LEFT_UP || res->type == ME_RIGHT_UP) {
        from_up(down, res);
    } else if (res->type == ME_LEFT_DOWN || res->type == ME_RIGHT_DOWN) {
        from_down(down, res);
    } else if (res->type == ME_CANCEL) {
        /* cancelled */
    }
}

static unsigned __stdcall waiter_proc(void *arg) {
    (void)arg;
    while (1) {
        WaitForSingleObject(g_waiter_event, INFINITE);

        MouseEvent down = g_waiting_event;
        int timeout = cfg_get_poll_timeout();

        MouseEvent result;
        if (sync_poll(timeout, &result)) {
            dispatch_event(&down, &result);
        } else {
            from_timeout(&down);
        }
    }
    return 0;
}

/* ========== Public API ========== */

BOOL waiter_offer(const MouseEvent *me) {
    if (sync_offer(me)) {
        set_flags_offer(me);
        return TRUE;
    }
    return FALSE;
}

void waiter_start(const MouseEvent *down) {
    g_waiting_event = *down;
    sync_set_waiting();
    SetEvent(g_waiter_event);
}

void waiter_init(void) {
    g_offer_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    g_waiter_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    g_waiter_thread = (HANDLE)_beginthreadex(NULL, 0, waiter_proc, NULL, 0, NULL);
    SetThreadPriority(g_waiter_thread, THREAD_PRIORITY_ABOVE_NORMAL);
}
