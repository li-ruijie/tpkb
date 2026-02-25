/*
 * Copyright (c) 2016-2021 Yuki Ono
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#include "dispatch.h"
#include "types.h"
#include "config.h"
#include "hook.h"
#include "event.h"
#include "kevent.h"
#include "ipc.h"

static BOOL proc_command(const MSLLHOOKSTRUCT *info) {
    if ((info->mouseData >> 16) != 1) return FALSE;
    int msg = (int)(DWORD)(ULONG_PTR)info->dwExtraInfo;
    return ipc_proc_message(msg);
}

/*
 * Hook callbacks are always invoked sequentially on the main thread, so
 * module-level statics are safe to use as "captured" parameters.
 */

static int sm_nCode;
static WPARAM sm_wParam;
static LPARAM sm_lParam;

static LRESULT call_next_mouse(void) {
    return hook_call_next_mouse(sm_nCode, sm_wParam, sm_lParam);
}

static int sk_nCode;
static WPARAM sk_wParam;
static LPARAM sk_lParam;

static LRESULT call_next_keyboard(void) {
    return hook_call_next_keyboard(sk_nCode, sk_wParam, sk_lParam);
}

static LRESULT CALLBACK mouse_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    const MSLLHOOKSTRUCT *info = (const MSLLHOOKSTRUCT *)lParam;

    sm_nCode = nCode;
    sm_wParam = wParam;
    sm_lParam = lParam;
    event_set_call_next_hook(call_next_mouse);

    if (nCode < 0)
        return call_next_mouse();

    if (cfg_is_pass_mode()) {
        if ((int)wParam == WM_MOUSEHWHEEL && proc_command(info))
            return 1;
        return call_next_mouse();
    }

    switch ((int)wParam) {
    case WM_MOUSEMOVE:    return event_move(info);
    case WM_LBUTTONDOWN:  return event_left_down(info);
    case WM_LBUTTONUP:    return event_left_up(info);
    case WM_RBUTTONDOWN:  return event_right_down(info);
    case WM_RBUTTONUP:    return event_right_up(info);
    case WM_MBUTTONDOWN:  return event_middle_down(info);
    case WM_MBUTTONUP:    return event_middle_up(info);
    case WM_XBUTTONDOWN:  return event_x_down(info);
    case WM_XBUTTONUP:    return event_x_up(info);
    case WM_MOUSEWHEEL:   return call_next_mouse();
    case WM_MOUSEHWHEEL:
        if (proc_command(info)) return 1;
        return call_next_mouse();
    default: return call_next_mouse();
    }
}

static LRESULT CALLBACK keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    const KBDLLHOOKSTRUCT *info = (const KBDLLHOOKSTRUCT *)lParam;

    sk_nCode = nCode;
    sk_wParam = wParam;
    sk_lParam = lParam;
    kevent_set_call_next_hook(call_next_keyboard);

    if (nCode < 0 || cfg_is_pass_mode())
        return call_next_keyboard();

    switch ((int)wParam) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: return kevent_key_down(info);
    case WM_KEYUP:
    case WM_SYSKEYUP:   return kevent_key_up(info);
    default: return call_next_keyboard();
    }
}

void dispatch_init(void) {
    hook_set_mouse_dispatcher(mouse_proc);
    hook_set_keyboard_dispatcher(keyboard_proc);
}
