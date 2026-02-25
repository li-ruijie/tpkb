/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#ifndef W10WHEEL_HOOK_H
#define W10WHEEL_HOOK_H

#include <windows.h>

typedef LRESULT (CALLBACK *LowLevelMouseProc)(int, WPARAM, LPARAM);
typedef LRESULT (CALLBACK *LowLevelKeyboardProc)(int, WPARAM, LPARAM);

void hook_set_mouse_dispatcher(LowLevelMouseProc proc);
void hook_set_keyboard_dispatcher(LowLevelKeyboardProc proc);

BOOL hook_set_mouse(void);
BOOL hook_set_keyboard(void);
void hook_unhook_mouse(void);
void hook_unhook_keyboard(void);
void hook_set_or_unset_keyboard(BOOL enable);
void hook_unhook(void);

LRESULT hook_call_next_mouse(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT hook_call_next_keyboard(int nCode, WPARAM wParam, LPARAM lParam);

#endif
