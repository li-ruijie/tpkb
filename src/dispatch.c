/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "dispatch.h"
#include "types.h"
#include "config.h"
#include "hook.h"
#include "event.h"
#include "kevent.h"
#include "tray.h"

#ifndef _MSC_VER
#include <setjmp.h>

static jmp_buf g_hook_jmpbuf;
static volatile int g_hook_depth = 0;

static LONG CALLBACK dispatch_veh(EXCEPTION_POINTERS *ep) {
    (void)ep;
    if (g_hook_depth > 0)
        longjmp(g_hook_jmpbuf, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

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
    /* nCode < 0: lParam may be invalid; pass through without dereferencing */
    if (nCode < 0)
        return hook_call_next_mouse(nCode, wParam, lParam);

    tray_hook_alive();

    const MSLLHOOKSTRUCT *info = (const MSLLHOOKSTRUCT *)lParam;

    /* Save/restore statics for re-entrancy (SendInput can re-enter the hook) */
    int prev_nCode = sm_nCode;
    WPARAM prev_wParam = sm_wParam;
    LPARAM prev_lParam = sm_lParam;

    sm_nCode = nCode;
    sm_wParam = wParam;
    sm_lParam = lParam;
    event_set_call_next_hook(call_next_mouse);

    LRESULT result;
#ifndef _MSC_VER
    jmp_buf prev_jmpbuf;
    memcpy(prev_jmpbuf, g_hook_jmpbuf, sizeof(jmp_buf));
    g_hook_depth++;
    if (setjmp(g_hook_jmpbuf) != 0) {
        result = call_next_mouse();
        goto mouse_done;
    }
#endif
#ifdef _MSC_VER
    __try {
#endif
        if (cfg_is_pass_mode()) {
            result = call_next_mouse();
        } else {
            switch ((int)wParam) {
            case WM_MOUSEMOVE:    result = event_move(info);         break;
            case WM_LBUTTONDOWN:  result = event_left_down(info);    break;
            case WM_LBUTTONUP:    result = event_left_up(info);      break;
            case WM_RBUTTONDOWN:  result = event_right_down(info);   break;
            case WM_RBUTTONUP:    result = event_right_up(info);     break;
            case WM_MBUTTONDOWN:  result = event_middle_down(info);  break;
            case WM_MBUTTONUP:    result = event_middle_up(info);    break;
            case WM_XBUTTONDOWN:  result = event_x_down(info);      break;
            case WM_XBUTTONUP:    result = event_x_up(info);        break;
            case WM_MOUSEWHEEL:   result = call_next_mouse();        break;
            case WM_MOUSEHWHEEL:  result = call_next_mouse();        break;
            default:              result = call_next_mouse();        break;
            }
        }
#ifdef _MSC_VER
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        result = call_next_mouse();
    }
#endif
#ifndef _MSC_VER
mouse_done:
    g_hook_depth--;
    memcpy(g_hook_jmpbuf, prev_jmpbuf, sizeof(jmp_buf));
#endif

    sm_nCode = prev_nCode;
    sm_wParam = prev_wParam;
    sm_lParam = prev_lParam;
    return result;
}

static LRESULT CALLBACK keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    /* nCode < 0: lParam may be invalid; pass through without dereferencing */
    if (nCode < 0)
        return hook_call_next_keyboard(nCode, wParam, lParam);

    const KBDLLHOOKSTRUCT *info = (const KBDLLHOOKSTRUCT *)lParam;

    /* Save/restore statics for re-entrancy (SendInput can re-enter the hook) */
    int prev_nCode = sk_nCode;
    WPARAM prev_wParam = sk_wParam;
    LPARAM prev_lParam = sk_lParam;

    sk_nCode = nCode;
    sk_wParam = wParam;
    sk_lParam = lParam;
    kevent_set_call_next_hook(call_next_keyboard);

    LRESULT result;
#ifndef _MSC_VER
    jmp_buf prev_jmpbuf;
    memcpy(prev_jmpbuf, g_hook_jmpbuf, sizeof(jmp_buf));
    g_hook_depth++;
    if (setjmp(g_hook_jmpbuf) != 0) {
        result = call_next_keyboard();
        goto keyboard_done;
    }
#endif
#ifdef _MSC_VER
    __try {
#endif
        if (cfg_is_pass_mode()) {
            result = call_next_keyboard();
        } else {
            switch ((int)wParam) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN: result = kevent_key_down(info);  break;
            case WM_KEYUP:
            case WM_SYSKEYUP:   result = kevent_key_up(info);    break;
            default:             result = call_next_keyboard();   break;
            }
        }
#ifdef _MSC_VER
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        result = call_next_keyboard();
    }
#endif
#ifndef _MSC_VER
keyboard_done:
    g_hook_depth--;
    memcpy(g_hook_jmpbuf, prev_jmpbuf, sizeof(jmp_buf));
#endif

    sk_nCode = prev_nCode;
    sk_wParam = prev_wParam;
    sk_lParam = prev_lParam;
    return result;
}

void dispatch_init(void) {
#ifndef _MSC_VER
    AddVectoredExceptionHandler(1, dispatch_veh);
#endif
    hook_set_mouse_dispatcher(mouse_proc);
    hook_set_keyboard_dispatcher(keyboard_proc);
}
