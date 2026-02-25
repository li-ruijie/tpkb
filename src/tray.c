/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "tray.h"
#include "types.h"
#include "config.h"
#include "hook.h"
#include "ipc.h"
#include "settings.h"
#include "../res/resource.h"
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>

#define TIMER_HOOK_HEALTH 1

enum {
    IDM_PASS_MODE = 1,
    IDM_SETTINGS,
    IDM_EXIT,
};

static NOTIFYICONDATAW g_nid;
static HWND g_hwnd;
static HICON g_icon_run, g_icon_stop;
static volatile LONG g_hook_alive = FALSE;
static POINT g_last_pt;
static UINT g_wm_taskbar_created;

/* ========== Menu ========== */

static HMENU build_context_menu(void) {
    HMENU menu = CreatePopupMenu();

    AppendMenuW(menu, MF_GRAYED, 0,
                IsUserAnAdmin() ? L"Running as Admin" : L"Running as User");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, cfg_is_pass_mode() ? MF_CHECKED : MF_UNCHECKED,
                IDM_PASS_MODE, cfg_is_pass_mode() ? L"Stopped" : L"Runnable");
    AppendMenuW(menu, MF_STRING, IDM_SETTINGS, L"Settings...");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");

    return menu;
}

/* ========== Command handler ========== */

static void handle_command(int cmd_id) {
    switch (cmd_id) {
    case IDM_PASS_MODE: {
        BOOL b = !cfg_is_pass_mode();
        cfg_set_pass_mode(b);
        tray_update_icon();
        break;
    }
    case IDM_SETTINGS:
        settings_show();
        break;
    case IDM_EXIT:
        cfg_exit_action();
        break;
    }
}

/* ========== Window procedure ========== */

static LRESULT CALLBACK tray_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            HMENU menu = build_context_menu();
            TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(menu);
            PostMessageW(hwnd, WM_NULL, 0, 0);
        }
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_HOOK_HEALTH) {
            POINT pt;
            GetCursorPos(&pt);
            BOOL moved = (pt.x != g_last_pt.x || pt.y != g_last_pt.y);
            g_last_pt = pt;
            if (moved && !InterlockedExchange(&g_hook_alive, FALSE))  {
                hook_unhook_mouse();
                hook_set_mouse();
            }
        }
        return 0;

    case WM_APP + 1:
        ipc_proc_message((int)wParam);
        return 0;

    case WM_COMMAND:
        handle_command(LOWORD(wParam));
        return 0;

    case WM_ENDSESSION:
        if (wParam)
            cfg_store_properties();
        return 0;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }

    if (msg == g_wm_taskbar_created && g_wm_taskbar_created != 0) {
        Shell_NotifyIconW(NIM_ADD, &g_nid);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* ========== Public API ========== */

void tray_update_icon(void) {
    g_nid.hIcon = cfg_is_pass_mode() ? g_icon_stop : g_icon_run;
    _snwprintf(g_nid.szTip, 128, L"%s - %s",
               PROGRAM_NAME, cfg_is_pass_mode() ? L"Stopped" : L"Runnable");
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

HWND tray_init(HINSTANCE hInst) {
    g_wm_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");

    /* Load tray icons from resources */
    g_icon_run = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_TRAY_RUN));
    g_icon_stop = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_TRAY_STOP));

    /* Register window class */
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = tray_wnd_proc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"tpkbTray";
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP));
    RegisterClassExW(&wc);

    /* Create hidden window */
    g_hwnd = CreateWindowExW(0, L"tpkbTray", PROGRAM_NAME, 0,
                             0, 0, 0, 0, NULL, NULL, hInst, NULL);

    /* Create system tray icon */
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = g_icon_run;
    _snwprintf(g_nid.szTip, 128, L"%s - %s", PROGRAM_NAME, L"Runnable");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    return g_hwnd;
}

void tray_cleanup(void) {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    if (g_hwnd) DestroyWindow(g_hwnd);
}

void tray_hook_alive(void) {
    InterlockedExchange(&g_hook_alive, TRUE);
}

void tray_start_health_timer(void) {
    tray_update_health_timer();
}

void tray_update_health_timer(void) {
    if (g_hwnd) KillTimer(g_hwnd, TIMER_HOOK_HEALTH);
    int secs = cfg_get_hook_health_check();
    if (secs > 0 && g_hwnd)
        SetTimer(g_hwnd, TIMER_HOOK_HEALTH, (UINT)(secs * 1000), NULL);
}

HWND tray_get_hwnd(void) {
    return g_hwnd;
}
