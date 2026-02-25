/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "kevent.h"
#include "config.h"

#define CHECK_NEXT    (-1)
#define HOOK_SUPPRESS 1

static LRESULT (*g_call_next_hook)(void) = NULL;

void kevent_set_call_next_hook(LRESULT (*fn)(void)) {
    g_call_next_hook = fn;
}

static LRESULT call_next_hook(void) {
    return g_call_next_hook ? g_call_next_hook() : 0;
}

static KeyboardEvent g_last_event = { KE_NON_EVENT };

static void init_state(void) {
    g_last_event.type = KE_NON_EVENT;
}

/* ========== Checker functions ========== */

static LRESULT skip_first_up(const KeyboardEvent *ke) {
    (void)ke;
    if (g_last_event.type == KE_NON_EVENT) return call_next_hook();
    return CHECK_NEXT;
}

static LRESULT check_same_last(const KeyboardEvent *ke) {
    if (ke->type == g_last_event.type &&
        ke_vk_code(ke) == ke_vk_code(&g_last_event) &&
        cfg_is_scroll_mode())
        return HOOK_SUPPRESS;
    g_last_event = *ke;
    return CHECK_NEXT;
}

static LRESULT check_trigger_scroll_start(const KeyboardEvent *ke) {
    if (cfg_is_trigger_key(ke)) {
        cfg_start_scroll_k(&ke->info);
        return HOOK_SUPPRESS;
    }
    return CHECK_NEXT;
}

static LRESULT check_exit_scroll_down(const KeyboardEvent *ke) {
    if (cfg_is_released_scroll()) {
        cfg_exit_scroll();
        cfg_last_flags_set_suppressed_k(ke);
        return HOOK_SUPPRESS;
    }
    return CHECK_NEXT;
}

static LRESULT check_exit_scroll_up(const KeyboardEvent *ke) {
    if (cfg_is_pressed_scroll()) {
        if (cfg_check_exit_scroll(ke->info.time))
            cfg_exit_scroll();
        else
            cfg_set_released_scroll();
        return HOOK_SUPPRESS;
    }
    return CHECK_NEXT;
}

static LRESULT check_suppressed_down(const KeyboardEvent *ke) {
    if (cfg_last_flags_get_reset_suppressed_k(ke))
        return HOOK_SUPPRESS;
    return CHECK_NEXT;
}

static LRESULT end_pass(const KeyboardEvent *ke) {
    (void)ke;
    return call_next_hook();
}

static LRESULT end_illegal(const KeyboardEvent *ke) {
    (void)ke;
    return HOOK_SUPPRESS;
}

/* ========== Checker chain runner ========== */

typedef LRESULT (*KChecker)(const KeyboardEvent *ke);

static LRESULT run_checkers(const KChecker *cs, int count, const KeyboardEvent *ke) {
    for (int i = 0; i < count; i++) {
        LRESULT r = cs[i](ke);
        if (r != CHECK_NEXT) return r;
    }
    return call_next_hook();
}

/* ========== Handler chains ========== */

static LRESULT single_down(const KeyboardEvent *ke) {
    static const KChecker cs[] = {
        check_same_last,
        check_exit_scroll_down,
        check_trigger_scroll_start,
        end_illegal,
    };
    return run_checkers(cs, sizeof(cs)/sizeof(cs[0]), ke);
}

static LRESULT single_up(const KeyboardEvent *ke) {
    static const KChecker cs[] = {
        skip_first_up,
        check_same_last,
        check_suppressed_down,
        check_exit_scroll_up,
        end_illegal,
    };
    return run_checkers(cs, sizeof(cs)/sizeof(cs[0]), ke);
}

static LRESULT none_down(const KeyboardEvent *ke) {
    static const KChecker cs[] = {
        check_exit_scroll_down,
        end_pass,
    };
    return run_checkers(cs, sizeof(cs)/sizeof(cs[0]), ke);
}

static LRESULT none_up(const KeyboardEvent *ke) {
    static const KChecker cs[] = {
        check_suppressed_down,
        end_pass,
    };
    return run_checkers(cs, sizeof(cs)/sizeof(cs[0]), ke);
}

/* ========== Public dispatch ========== */

LRESULT kevent_key_down(const KBDLLHOOKSTRUCT *info) {
    KeyboardEvent ke = { KE_KEY_DOWN, *info };
    if (cfg_is_trigger_key(&ke)) return single_down(&ke);
    return none_down(&ke);
}

LRESULT kevent_key_up(const KBDLLHOOKSTRUCT *info) {
    KeyboardEvent ke = { KE_KEY_UP, *info };
    if (cfg_is_trigger_key(&ke)) return single_up(&ke);
    return none_up(&ke);
}

/* ========== Init ========== */

void kevent_init(void) {
    cfg_set_init_state_keh_cb(init_state);
}
