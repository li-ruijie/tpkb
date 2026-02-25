/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "hook.h"
#include "types.h"
#include "dialog.h"
#include "util.h"

static volatile HHOOK g_mouse_hhk = NULL;
static volatile HHOOK g_keyboard_hhk = NULL;

static LowLevelMouseProc g_mouse_dispatcher = NULL;
static LowLevelKeyboardProc g_keyboard_dispatcher = NULL;

void hook_set_mouse_dispatcher(LowLevelMouseProc proc) {
    g_mouse_dispatcher = proc;
}

void hook_set_keyboard_dispatcher(LowLevelKeyboardProc proc) {
    g_keyboard_dispatcher = proc;
}

LRESULT hook_call_next_mouse(int nCode, WPARAM wParam, LPARAM lParam) {
    HHOOK hhk = (HHOOK)InterlockedCompareExchangePointer((volatile PVOID *)&g_mouse_hhk, NULL, NULL);
    return CallNextHookEx(hhk, nCode, wParam, lParam);
}

LRESULT hook_call_next_keyboard(int nCode, WPARAM wParam, LPARAM lParam) {
    HHOOK hhk = (HHOOK)InterlockedCompareExchangePointer((volatile PVOID *)&g_keyboard_hhk, NULL, NULL);
    return CallNextHookEx(hhk, nCode, wParam, lParam);
}

BOOL hook_set_mouse(void) {
    HINSTANCE hmod = GetModuleHandleW(NULL);
    HHOOK hhk = SetWindowsHookExW(WH_MOUSE_LL, g_mouse_dispatcher, hmod, 0);
    InterlockedExchangePointer((volatile PVOID *)&g_mouse_hhk, hhk);
    return hhk != NULL;
}

BOOL hook_set_keyboard(void) {
    HINSTANCE hmod = GetModuleHandleW(NULL);
    HHOOK hhk = SetWindowsHookExW(WH_KEYBOARD_LL, g_keyboard_dispatcher, hmod, 0);
    InterlockedExchangePointer((volatile PVOID *)&g_keyboard_hhk, hhk);
    return hhk != NULL;
}

void hook_unhook_mouse(void) {
    HHOOK hhk = (HHOOK)InterlockedExchangePointer((volatile PVOID *)&g_mouse_hhk, NULL);
    if (hhk) UnhookWindowsHookEx(hhk);
}

void hook_unhook_keyboard(void) {
    HHOOK hhk = (HHOOK)InterlockedExchangePointer((volatile PVOID *)&g_keyboard_hhk, NULL);
    if (hhk) UnhookWindowsHookEx(hhk);
}

void hook_set_or_unset_keyboard(BOOL enable) {
    hook_unhook_keyboard();
    if (enable) {
        if (!hook_set_keyboard()) {
            wchar_t err[256];
            util_get_last_error_message(err, 256);
            dialog_error(L"Failed keyboard hook install", err);
        }
    }
}

void hook_unhook(void) {
    hook_unhook_mouse();
    hook_unhook_keyboard();
}
