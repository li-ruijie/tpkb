/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "rawinput.h"
#include "types.h"

static SendWheelRawFn g_send_wheel_raw = NULL;
static HWND g_msg_window = NULL;

void rawinput_set_send_wheel_raw(SendWheelRawFn fn) {
    g_send_wheel_raw = fn;
}

static void proc_raw_input(LPARAM lParam) {
    RAWINPUT ri;
    UINT size = sizeof(ri);
    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1)
        return;

    if (ri.header.dwType == RIM_TYPEMOUSE && ri.data.mouse.usFlags == MOUSE_MOVE_RELATIVE) {
        if (g_send_wheel_raw)
            g_send_wheel_raw(ri.data.mouse.lLastX, ri.data.mouse.lLastY);
    }
}

static LRESULT CALLBACK msg_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INPUT) {
        proc_raw_input(lParam);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void rawinput_init(void) {
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = msg_wnd_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"tpkbRawInput";
    RegisterClassExW(&wc);

    g_msg_window = CreateWindowExW(0, L"tpkbRawInput", L"", 0,
                                   0, 0, 0, 0, HWND_MESSAGE, NULL,
                                   wc.hInstance, NULL);
}

static BOOL register_raw_device(DWORD flags, HWND hwnd) {
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01; /* HID_USAGE_PAGE_GENERIC */
    rid.usUsage = 0x02;     /* HID_USAGE_GENERIC_MOUSE */
    rid.dwFlags = flags;
    rid.hwndTarget = hwnd;
    return RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

void rawinput_register(void) {
    register_raw_device(RIDEV_INPUTSINK, g_msg_window);
}

void rawinput_unregister(void) {
    register_raw_device(RIDEV_REMOVE, NULL);
}
