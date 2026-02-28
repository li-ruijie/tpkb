/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "waiter.h"
#include "config.h"
#include "scroll.h"
#include <process.h>

/* ========== Synchronous queue (capacity 1) ========== */

enum { SYNC_IDLE = 0, SYNC_WAITING = 1, SYNC_OFFERED = 2, SYNC_DONE = 3 };

static MouseEvent g_offer_slot;
static volatile LONG g_sync_state = SYNC_IDLE;
static HANDLE g_offer_event = NULL; /* Signaled when an event is offered */

static void sync_set_waiting(void) {
    ResetEvent(g_offer_event);
    InterlockedExchange(&g_sync_state, SYNC_WAITING);
}

/* Poll: wait for an offered event or timeout. Returns TRUE if event received. */
static BOOL sync_poll(int timeout_ms, MouseEvent *out) {
    DWORD result = WaitForSingleObject(g_offer_event, (DWORD)timeout_ms);
    if (result == WAIT_OBJECT_0) {
        /* Event was signaled: read slot before unblocking producer */
        *out = g_offer_slot;
        if (InterlockedCompareExchange(&g_sync_state, SYNC_DONE, SYNC_OFFERED) == SYNC_OFFERED) {
            WakeByAddressSingle((void *)&g_sync_state);
            return TRUE;
        }
    }
    /* Timeout: try to transition WAITING -> IDLE */
    if (InterlockedCompareExchange(&g_sync_state, SYNC_IDLE, SYNC_WAITING) != SYNC_WAITING) {
        /* Lost race: offerer already moved us to OFFERED, read slot before unblocking */
        *out = g_offer_slot;
        InterlockedExchange(&g_sync_state, SYNC_DONE);
        WakeByAddressSingle((void *)&g_sync_state);
        return TRUE;
    }
    return FALSE;
}

/* Offer: provide an event to the polling thread. Returns TRUE if accepted. */
static BOOL sync_offer(const MouseEvent *me) {
    /* Write data before state transition to prevent consumer reading stale slot */
    g_offer_slot = *me;
    MemoryBarrier();

    /* Try to transition WAITING -> OFFERED */
    if (InterlockedCompareExchange(&g_sync_state, SYNC_OFFERED, SYNC_WAITING) != SYNC_WAITING)
        return FALSE;

    SetEvent(g_offer_event);

    /* Wait until the waiter thread transitions away from OFFERED */
    LONG offered = SYNC_OFFERED;
    WaitOnAddress((volatile void *)&g_sync_state, &offered, sizeof(LONG), 150);

    return TRUE;
}

/* ========== Waiter queue (ring buffer) ========== */

#define WAITER_QUEUE_SIZE 64

static MouseEvent g_wq[WAITER_QUEUE_SIZE];
static volatile LONG g_wq_head = 0;
static volatile LONG g_wq_tail = 0;
static HANDLE g_wq_sem = NULL;       /* counts available items */
static HANDLE g_wq_space_sem = NULL; /* counts available slots */
static CRITICAL_SECTION g_wq_cs;

/* Current waiting event for setFlagsOffer (only accessed from hook thread) */
static MouseEvent g_waiting_event;

static HANDLE g_waiter_thread = NULL;
static volatile BOOL g_waiter_running = FALSE;

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
    }
}

static unsigned __stdcall waiter_proc(void *arg) {
    (void)arg;
    while (g_waiter_running) {
        WaitForSingleObject(g_wq_sem, INFINITE);
        if (!g_waiter_running) break;

        EnterCriticalSection(&g_wq_cs);
        MouseEvent down = g_wq[g_wq_tail];
        g_wq_tail = (g_wq_tail + 1) % WAITER_QUEUE_SIZE;
        LeaveCriticalSection(&g_wq_cs);
        ReleaseSemaphore(g_wq_space_sem, 1, NULL);
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

BOOL waiter_start(const MouseEvent *down) {
    /* Check for queue space before entering waiting state */
    if (WaitForSingleObject(g_wq_space_sem, 0) != WAIT_OBJECT_0)
        return FALSE;

    g_waiting_event = *down;
    sync_set_waiting();

    EnterCriticalSection(&g_wq_cs);
    g_wq[g_wq_head] = *down;
    g_wq_head = (g_wq_head + 1) % WAITER_QUEUE_SIZE;
    ReleaseSemaphore(g_wq_sem, 1, NULL);
    LeaveCriticalSection(&g_wq_cs);
    return TRUE;
}

void waiter_init(void) {
    g_offer_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    g_wq_sem = CreateSemaphoreW(NULL, 0, WAITER_QUEUE_SIZE, NULL);
    g_wq_space_sem = CreateSemaphoreW(NULL, WAITER_QUEUE_SIZE, WAITER_QUEUE_SIZE, NULL);
    InitializeCriticalSection(&g_wq_cs);
    g_waiter_running = TRUE;
    g_waiter_thread = (HANDLE)_beginthreadex(NULL, 0, waiter_proc, NULL, 0, NULL);
    SetThreadPriority(g_waiter_thread, THREAD_PRIORITY_ABOVE_NORMAL);
}

void waiter_cleanup(void) {
    g_waiter_running = FALSE;
    if (g_wq_sem) ReleaseSemaphore(g_wq_sem, 1, NULL); /* Unblock thread */
    if (g_waiter_thread) {
        WaitForSingleObject(g_waiter_thread, 2000);
        CloseHandle(g_waiter_thread);
        g_waiter_thread = NULL;
    }
    if (g_offer_event) {
        CloseHandle(g_offer_event);
        g_offer_event = NULL;
    }
    if (g_wq_sem) {
        CloseHandle(g_wq_sem);
        g_wq_sem = NULL;
    }
    if (g_wq_space_sem) {
        CloseHandle(g_wq_space_sem);
        g_wq_space_sem = NULL;
    }
    DeleteCriticalSection(&g_wq_cs);
}
