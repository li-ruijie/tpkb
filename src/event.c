/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#include "event.h"
#include "config.h"
#include "scroll.h"
#include "waiter.h"
#include "cursor.h"
#include <math.h>

/* ========== Checker result convention ========== */
/* Return 0 = call next hook, 1 = suppress, -1 = continue checking (no result) */
#define CHECK_NEXT      (-1)
#define HOOK_PASS       0
#define HOOK_SUPPRESS   1

static LRESULT (*g_call_next_hook)(void) = NULL;

void event_set_call_next_hook(LRESULT (*fn)(void)) {
    g_call_next_hook = fn;
}

static LRESULT call_next_hook(void) {
    return g_call_next_hook ? g_call_next_hook() : 0;
}

/* ========== State ========== */

static MouseEvent g_last_event = { ME_NON_EVENT };
static MouseEvent g_last_resend_left = { ME_NON_EVENT };
static MouseEvent g_last_resend_right = { ME_NON_EVENT };
static BOOL g_resent_down_up = FALSE;
static BOOL g_second_trigger_up = FALSE;

/* Drag state */
static void (*g_drag_fn)(const MSLLHOOKSTRUCT *) = NULL;
static BOOL g_dragged = FALSE;
static BOOL g_drag_pre_scroll = FALSE;
static int g_drag_start_x, g_drag_start_y;
static int g_drag_move_x, g_drag_move_y;

static void drag_default(const MSLLHOOKSTRUCT *info) { (void)info; }

static void init_state(void) {
    g_last_event.type = ME_NON_EVENT;
    g_last_resend_left.type = ME_NON_EVENT;
    g_last_resend_right.type = ME_NON_EVENT;
    g_resent_down_up = FALSE;
    g_second_trigger_up = FALSE;
    g_drag_fn = drag_default;
    g_dragged = FALSE;
    g_drag_pre_scroll = FALSE;
    g_drag_start_x = 0; g_drag_start_y = 0;
    g_drag_move_x = 0; g_drag_move_y = 0;
}

/* ========== Checker functions ========== */

/* Returns a pointer to the last resend event for left or right events */
static MouseEvent *get_last_resend(const MouseEvent *me) {
    if (me_is_left(me->type)) return &g_last_resend_left;
    if (me_is_right(me->type)) return &g_last_resend_right;
    return NULL;
}

static BOOL check_correct_order(const MouseEvent *me) {
    MouseEvent *pre = get_last_resend(me);
    if (!pre) return TRUE;
    /* NonEvent followed by Up, or Up followed by Up = bad order */
    if (pre->type == ME_NON_EVENT && me_is_up(me->type)) return FALSE;
    if (me_is_up(pre->type) && me_is_up(me->type)) return FALSE;
    return TRUE;
}

static LRESULT skip_resend_lr(const MouseEvent *me) {
    if (!scroll_is_injected(me)) return CHECK_NEXT;

    if (scroll_is_resend_click(me)) return call_next_hook();

    if (scroll_is_resend(me)) {
        if (g_resent_down_up) {
            g_resent_down_up = FALSE;
            if (check_correct_order(me)) {
                MouseEvent *lr = get_last_resend(me);
                if (lr) *lr = *me;
                return call_next_hook();
            } else {
                Sleep(1);
                scroll_resend_up(me);
                return HOOK_SUPPRESS;
            }
        }
        MouseEvent *lr = get_last_resend(me);
        if (lr) *lr = *me;
        return call_next_hook();
    }

    /* Other software-injected event */
    return call_next_hook();
}

static LRESULT skip_resend_single(const MouseEvent *me) {
    if (!scroll_is_injected(me)) return CHECK_NEXT;
    return call_next_hook();
}

static LRESULT check_escape(const MouseEvent *me) {
    (void)me;
    if (scroll_check_esc()) {
        cfg_init_state();
        return call_next_hook();
    }
    return CHECK_NEXT;
}

static LRESULT skip_first_up(const MouseEvent *me) {
    (void)me;
    if (g_last_event.type == ME_NON_EVENT)
        return call_next_hook();
    return CHECK_NEXT;
}

static LRESULT check_same_last(const MouseEvent *me) {
    if (me->type == g_last_event.type)
        return call_next_hook();
    g_last_event = *me;
    return CHECK_NEXT;
}

static LRESULT reset_last_flags_lr(const MouseEvent *me) {
    cfg_last_flags_reset_lr(me);
    return CHECK_NEXT;
}

static LRESULT check_exit_scroll_down(const MouseEvent *me) {
    if (cfg_is_released_scroll()) {
        cfg_exit_scroll();
        cfg_last_flags_set_suppressed(me);
        return HOOK_SUPPRESS;
    }
    return CHECK_NEXT;
}

static LRESULT pass_pressed_scroll(const MouseEvent *me) {
    if (cfg_is_pressed_scroll()) {
        cfg_last_flags_set_passed(me);
        return call_next_hook();
    }
    return CHECK_NEXT;
}

static LRESULT check_exit_scroll_up(const MouseEvent *me) {
    if (cfg_is_pressed_scroll()) {
        if (cfg_check_exit_scroll(me->info.time))
            cfg_exit_scroll();
        else
            cfg_set_released_scroll();
        return HOOK_SUPPRESS;
    }
    return CHECK_NEXT;
}

static LRESULT check_exit_scroll_up_lr(const MouseEvent *me) {
    if (cfg_is_pressed_scroll()) {
        if (!g_second_trigger_up) {
            /* Ignore first up */
        } else if (cfg_check_exit_scroll(me->info.time)) {
            cfg_exit_scroll();
        } else {
            cfg_set_released_scroll();
        }
        g_second_trigger_up = !g_second_trigger_up;
        return HOOK_SUPPRESS;
    }
    return CHECK_NEXT;
}

static LRESULT check_starting_scroll(const MouseEvent *me) {
    (void)me;
    if (cfg_is_starting_scroll()) {
        Sleep(1);
        if (!g_second_trigger_up) {
            /* Ignore first up (starting) */
        } else {
            cfg_exit_scroll();
        }
        g_second_trigger_up = !g_second_trigger_up;
        return HOOK_SUPPRESS;
    }
    return CHECK_NEXT;
}

static LRESULT offer_event_waiter(const MouseEvent *me) {
    if (waiter_offer(me))
        return HOOK_SUPPRESS;
    return CHECK_NEXT;
}

static LRESULT check_suppressed_down(const MouseEvent *me) {
    if (cfg_last_flags_get_reset_suppressed(me))
        return HOOK_SUPPRESS;
    return CHECK_NEXT;
}

static LRESULT check_resent_down(const MouseEvent *me) {
    if (cfg_last_flags_get_reset_resent(me)) {
        g_resent_down_up = TRUE;
        scroll_resend_up(me);
        return HOOK_SUPPRESS;
    }
    return CHECK_NEXT;
}

static LRESULT check_passed_down(const MouseEvent *me) {
    if (cfg_last_flags_get_reset_passed(me))
        return call_next_hook();
    return CHECK_NEXT;
}

static LRESULT check_trigger_wait_start(const MouseEvent *me) {
    if (cfg_is_lr_trigger() || cfg_is_trigger_event(me->type)) {
        waiter_start(me);
        return HOOK_SUPPRESS;
    }
    return CHECK_NEXT;
}

static LRESULT check_key_send_middle(const MouseEvent *me) {
    if (cfg_is_send_middle_click() &&
        (scroll_check_shift() || scroll_check_ctrl() || scroll_check_alt())) {
        scroll_resend_click(MC_MIDDLE, &me->info);
        cfg_last_flags_set_suppressed(me);
        return HOOK_SUPPRESS;
    }
    return CHECK_NEXT;
}

static LRESULT check_trigger_scroll_start(const MouseEvent *me) {
    if (cfg_is_trigger_event(me->type)) {
        cfg_start_scroll(&me->info);
        return HOOK_SUPPRESS;
    }
    return CHECK_NEXT;
}

static LRESULT pass_not_trigger(const MouseEvent *me) {
    if (!cfg_is_trigger_event(me->type))
        return call_next_hook();
    return CHECK_NEXT;
}

static LRESULT pass_not_drag_trigger(const MouseEvent *me) {
    if (!cfg_is_drag_trigger_event(me->type))
        return call_next_hook();
    return CHECK_NEXT;
}

static LRESULT end_not_trigger(const MouseEvent *me) {
    (void)me;
    return call_next_hook();
}

static LRESULT end_pass(const MouseEvent *me) {
    (void)me;
    return call_next_hook();
}

static LRESULT end_illegal(const MouseEvent *me) {
    (void)me;
    return HOOK_SUPPRESS;
}

/* ========== Drag handling ========== */

static void drag_start(const MSLLHOOKSTRUCT *info) {
    g_drag_move_x += abs(info->pt.x - g_drag_start_x);
    g_drag_move_y += abs(info->pt.y - g_drag_start_y);

    int thr = cfg_get_drag_threshold();
    if (g_drag_move_x > thr || g_drag_move_y > thr) {
        cfg_start_scroll(info);
        g_drag_pre_scroll = FALSE;
        if (cfg_is_cursor_change() && !cfg_is_vh_adjuster_mode())
            cursor_change_v();
        g_drag_fn = drag_default;
        g_dragged = TRUE;
    }
}

static LRESULT start_scroll_drag(const MouseEvent *me) {
    g_drag_pre_scroll = TRUE;
    g_drag_start_x = me->info.pt.x;
    g_drag_start_y = me->info.pt.y;
    g_drag_move_x = 0;
    g_drag_move_y = 0;
    g_drag_fn = drag_start;
    g_dragged = FALSE;
    return HOOK_SUPPRESS;
}

static LRESULT continue_scroll_drag(const MouseEvent *me) {
    (void)me;
    if (cfg_is_dragged_lock() && g_dragged) {
        cfg_set_released_scroll();
        return HOOK_SUPPRESS;
    }
    return CHECK_NEXT;
}

static LRESULT exit_and_resend_drag(const MouseEvent *me) {
    g_drag_fn = drag_default;
    g_drag_pre_scroll = FALSE;
    cfg_exit_scroll();

    if (!g_dragged) {
        MouseClickType mc;
        switch (me->type) {
        case ME_LEFT_UP:   mc = MC_LEFT;   break;
        case ME_RIGHT_UP:  mc = MC_RIGHT;  break;
        case ME_MIDDLE_UP: mc = MC_MIDDLE; break;
        case ME_X1_UP:     mc = MC_X1;     break;
        case ME_X2_UP:     mc = MC_X2;     break;
        default: return HOOK_SUPPRESS;
        }
        scroll_resend_click(mc, &me->info);
    }
    return HOOK_SUPPRESS;
}

/* ========== Checker chain runner ========== */

typedef LRESULT (*Checker)(const MouseEvent *me);

static LRESULT run_checkers(const Checker *cs, int count, const MouseEvent *me) {
    for (int i = 0; i < count; i++) {
        LRESULT r = cs[i](me);
        if (r != CHECK_NEXT) return r;
    }
    return call_next_hook();
}

/* ========== Handler chains ========== */

static LRESULT lr_down(const MouseEvent *me) {
    static const Checker cs[] = {
        skip_resend_lr,
        check_same_last,
        reset_last_flags_lr,
        check_exit_scroll_down,
        pass_pressed_scroll,
        offer_event_waiter,
        check_trigger_wait_start,
        end_not_trigger,
    };
    return run_checkers(cs, sizeof(cs)/sizeof(cs[0]), me);
}

static LRESULT lr_up(const MouseEvent *me) {
    static const Checker cs[] = {
        skip_resend_lr,
        check_escape,
        skip_first_up,
        check_same_last,
        check_passed_down,
        check_resent_down,
        check_exit_scroll_up_lr,
        check_starting_scroll,
        offer_event_waiter,
        check_suppressed_down,
        end_not_trigger,
    };
    return run_checkers(cs, sizeof(cs)/sizeof(cs[0]), me);
}

static LRESULT single_down(const MouseEvent *me) {
    static const Checker cs[] = {
        skip_resend_single,
        check_same_last,
        check_exit_scroll_down,
        pass_not_trigger,
        check_key_send_middle,
        check_trigger_scroll_start,
        end_illegal,
    };
    return run_checkers(cs, sizeof(cs)/sizeof(cs[0]), me);
}

static LRESULT single_up(const MouseEvent *me) {
    static const Checker cs[] = {
        skip_resend_single,
        check_escape,
        skip_first_up,
        check_same_last,
        check_suppressed_down,
        pass_not_trigger,
        check_exit_scroll_up,
        end_illegal,
    };
    return run_checkers(cs, sizeof(cs)/sizeof(cs[0]), me);
}

static LRESULT drag_down(const MouseEvent *me) {
    static const Checker cs[] = {
        skip_resend_single,
        check_same_last,
        check_exit_scroll_down,
        pass_not_drag_trigger,
        start_scroll_drag,
    };
    return run_checkers(cs, sizeof(cs)/sizeof(cs[0]), me);
}

static LRESULT drag_up(const MouseEvent *me) {
    static const Checker cs[] = {
        skip_resend_single,
        check_escape,
        skip_first_up,
        check_same_last,
        check_suppressed_down,
        pass_not_drag_trigger,
        continue_scroll_drag,
        exit_and_resend_drag,
    };
    return run_checkers(cs, sizeof(cs)/sizeof(cs[0]), me);
}

static LRESULT none_down(const MouseEvent *me) {
    static const Checker cs[] = {
        check_exit_scroll_down,
        end_pass,
    };
    return run_checkers(cs, sizeof(cs)/sizeof(cs[0]), me);
}

static LRESULT none_up(const MouseEvent *me) {
    static const Checker cs[] = {
        check_escape,
        check_suppressed_down,
        end_pass,
    };
    return run_checkers(cs, sizeof(cs)/sizeof(cs[0]), me);
}

/* ========== Swappable handler pointers ========== */

static LRESULT (*volatile g_proc_down_lr)(const MouseEvent *) = lr_down;
static LRESULT (*volatile g_proc_up_lr)(const MouseEvent *) = lr_up;
static LRESULT (*volatile g_proc_down_s)(const MouseEvent *) = none_down;
static LRESULT (*volatile g_proc_up_s)(const MouseEvent *) = none_up;

static void change_trigger(void) {
    if (cfg_is_double_trigger()) {
        InterlockedExchangePointer((volatile PVOID *)&g_proc_down_lr, (PVOID)lr_down);
        InterlockedExchangePointer((volatile PVOID *)&g_proc_up_lr, (PVOID)lr_up);
        InterlockedExchangePointer((volatile PVOID *)&g_proc_down_s, (PVOID)none_down);
        InterlockedExchangePointer((volatile PVOID *)&g_proc_up_s, (PVOID)none_up);
    } else if (cfg_is_single_trigger()) {
        InterlockedExchangePointer((volatile PVOID *)&g_proc_down_lr, (PVOID)none_down);
        InterlockedExchangePointer((volatile PVOID *)&g_proc_up_lr, (PVOID)none_up);
        InterlockedExchangePointer((volatile PVOID *)&g_proc_down_s, (PVOID)single_down);
        InterlockedExchangePointer((volatile PVOID *)&g_proc_up_s, (PVOID)single_up);
    } else if (cfg_is_drag_trigger()) {
        InterlockedExchangePointer((volatile PVOID *)&g_proc_down_lr, (PVOID)drag_down);
        InterlockedExchangePointer((volatile PVOID *)&g_proc_up_lr, (PVOID)drag_up);
        InterlockedExchangePointer((volatile PVOID *)&g_proc_down_s, (PVOID)drag_down);
        InterlockedExchangePointer((volatile PVOID *)&g_proc_up_s, (PVOID)drag_up);
    } else {
        InterlockedExchangePointer((volatile PVOID *)&g_proc_down_lr, (PVOID)none_down);
        InterlockedExchangePointer((volatile PVOID *)&g_proc_up_lr, (PVOID)none_up);
        InterlockedExchangePointer((volatile PVOID *)&g_proc_down_s, (PVOID)none_down);
        InterlockedExchangePointer((volatile PVOID *)&g_proc_up_s, (PVOID)none_up);
    }
}

/* ========== Public dispatch ========== */

LRESULT event_left_down(const MSLLHOOKSTRUCT *info) {
    MouseEvent me = { ME_LEFT_DOWN, *info };
    return g_proc_down_lr(&me);
}

LRESULT event_left_up(const MSLLHOOKSTRUCT *info) {
    MouseEvent me = { ME_LEFT_UP, *info };
    return g_proc_up_lr(&me);
}

LRESULT event_right_down(const MSLLHOOKSTRUCT *info) {
    MouseEvent me = { ME_RIGHT_DOWN, *info };
    return g_proc_down_lr(&me);
}

LRESULT event_right_up(const MSLLHOOKSTRUCT *info) {
    MouseEvent me = { ME_RIGHT_UP, *info };
    return g_proc_up_lr(&me);
}

LRESULT event_middle_down(const MSLLHOOKSTRUCT *info) {
    MouseEvent me = { ME_MIDDLE_DOWN, *info };
    return g_proc_down_s(&me);
}

LRESULT event_middle_up(const MSLLHOOKSTRUCT *info) {
    MouseEvent me = { ME_MIDDLE_UP, *info };
    return g_proc_up_s(&me);
}

LRESULT event_x_down(const MSLLHOOKSTRUCT *info) {
    MouseEventType type = me_is_xbutton1(info->mouseData) ? ME_X1_DOWN : ME_X2_DOWN;
    MouseEvent me = { type, *info };
    return g_proc_down_s(&me);
}

LRESULT event_x_up(const MSLLHOOKSTRUCT *info) {
    MouseEventType type = me_is_xbutton1(info->mouseData) ? ME_X1_UP : ME_X2_UP;
    MouseEvent me = { type, *info };
    return g_proc_up_s(&me);
}

LRESULT event_move(const MSLLHOOKSTRUCT *info) {
    if (cfg_is_scroll_mode() || g_drag_pre_scroll) {
        if (g_drag_fn) g_drag_fn(info);
        return HOOK_SUPPRESS;
    }

    MouseEvent me = { ME_MOVE, *info };
    if (waiter_offer(&me))
        return HOOK_SUPPRESS;

    return call_next_hook();
}

/* ========== Init ========== */

void event_init(void) {
    g_drag_fn = drag_default;
    cfg_set_change_trigger_cb(change_trigger);
    cfg_set_init_state_meh_cb(init_state);
}
