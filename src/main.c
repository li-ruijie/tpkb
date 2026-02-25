/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "types.h"
#include "config.h"
#include "scroll.h"
#include "event.h"
#include "kevent.h"
#include "waiter.h"
#include "dispatch.h"
#include "hook.h"
#include "tray.h"
#include "settings.h"
#include "dialog.h"
#include "cursor.h"
#include "rawinput.h"
#include "ipc.h"
#include "util.h"
#include <wchar.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <tlhelp32.h>

static const wchar_t *ADMIN_MESSAGE =
    L"tpkb is not running as administrator. "
    L"It may not work in some windows. "
    L"Running as administrator is recommended.";

/* ========== Admin privilege check ========== */

#define ADMIN_BTN_RUNAS   1
#define ADMIN_BTN_CONTINUE 2
#define ADMIN_BTN_EXIT    3

static HRESULT CALLBACK admin_td_callback(HWND hwnd, UINT msg,
        WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData) {
    (void)wParam; (void)lParam; (void)lpRefData;
    if (msg == TDN_CREATED)
        SendMessageW(hwnd, TDM_SET_BUTTON_ELEVATION_REQUIRED_STATE, ADMIN_BTN_RUNAS, TRUE);
    return S_OK;
}

static void relaunch_as_admin(void) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    ShellExecuteW(NULL, L"runas", path, NULL, NULL, SW_SHOWNORMAL);
}

static void check_admin(void) {
    if (IsUserAnAdmin()) return;

    TASKDIALOG_BUTTON buttons[] = {
        { ADMIN_BTN_RUNAS, L"Run as Admin" },
        { ADMIN_BTN_CONTINUE, L"Continue" },
        { ADMIN_BTN_EXIT, L"Exit" },
    };

    TASKDIALOGCONFIG config = {0};
    config.cbSize = sizeof(config);
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle = L"tpkb";
    config.pszMainIcon = TD_WARNING_ICON;
    config.pszContent = ADMIN_MESSAGE;
    config.cButtons = 3;
    config.pButtons = buttons;
    config.nDefaultButton = ADMIN_BTN_RUNAS;
    config.pfCallback = admin_td_callback;

    int pressed = 0;
    HRESULT hr = TaskDialogIndirect(&config, &pressed, NULL, NULL);

    if (FAILED(hr)) {
        int mb = MessageBoxW(NULL, ADMIN_MESSAGE, L"tpkb",
                             MB_YESNOCANCEL | MB_ICONWARNING);
        if (mb == IDYES) pressed = ADMIN_BTN_RUNAS;
        else if (mb == IDNO) pressed = ADMIN_BTN_CONTINUE;
        else pressed = ADMIN_BTN_EXIT;
    }

    if (pressed == ADMIN_BTN_RUNAS) {
        util_unlock();
        relaunch_as_admin();
        ExitProcess(0);
    } else if (pressed == ADMIN_BTN_CONTINUE) {
        return;
    } else {
        ExitProcess(0);
    }
}

/* ========== Double launch check ========== */

static BOOL is_other_instance_elevated(void) {
    DWORD my_pid = GetCurrentProcessId();
    wchar_t my_path[MAX_PATH];
    GetModuleFileNameW(NULL, my_path, MAX_PATH);

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return FALSE;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    BOOL elevated = FALSE;

    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID != my_pid) {
                HANDLE hproc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                if (hproc) {
                    wchar_t proc_path[MAX_PATH];
                    DWORD sz = MAX_PATH;
                    BOOL match = QueryFullProcessImageNameW(hproc, 0, proc_path, &sz) &&
                                 _wcsicmp(proc_path, my_path) == 0;
                    if (match) {
                        HANDLE htoken;
                        if (OpenProcessToken(hproc, TOKEN_QUERY, &htoken)) {
                            TOKEN_ELEVATION elev = {0};
                            DWORD len;
                            if (GetTokenInformation(htoken, TokenElevation, &elev, sizeof(elev), &len))
                                elevated = elev.TokenIsElevated;
                            CloseHandle(htoken);
                        }
                    }
                    CloseHandle(hproc);
                }
                if (elevated) break;
            }
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return elevated;
}

static BOOL check_double_launch(void) {
    if (util_try_lock()) return TRUE;

    TASKDIALOG_BUTTON buttons[] = {
        { 1, L"Restart" },
        { 2, L"Exit" },
    };

    TASKDIALOGCONFIG config = {0};
    config.cbSize = sizeof(config);
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle = L"tpkb";
    config.pszMainIcon = TD_INFORMATION_ICON;
    config.pszContent = L"tpkb is already running. Restart?";
    config.cButtons = 2;
    config.pButtons = buttons;
    config.nDefaultButton = 1;

    int pressed = 0;
    TaskDialogIndirect(&config, &pressed, NULL, NULL);

    if (pressed != 1)
        ExitProcess(0);

    BOOL was_admin = is_other_instance_elevated();
    ipc_send_exit();

    /* Wait for old instance to release the mutex */
    HANDLE h = OpenMutexW(SYNCHRONIZE, FALSE, L"Local\\tpkb_SingleInstance");
    if (h) {
        WaitForSingleObject(h, 5000);
        CloseHandle(h);
    }

    if (util_try_lock()) {
        if (was_admin && !IsUserAnAdmin()) {
            util_unlock();
            relaunch_as_admin();
            ExitProcess(0);
        }
        return TRUE;
    }

    MessageBoxW(NULL, L"Error",
                L"Error", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

static void proc_exit(void) {
    ipc_server_stop();
    tray_cleanup();
    hook_unhook();
    waiter_cleanup();
    scroll_cleanup();
    cfg_store_properties();
    util_unlock();
    util_cleanup();
}

static void proc_argv(int argc, wchar_t **argv) {
    if (argc < 2) return;

    if (wcscmp(argv[1], L"--sendExit") == 0) {
        ipc_send_exit();
    } else if (wcscmp(argv[1], L"--sendPassMode") == 0) {
        BOOL val = TRUE;
        if (argc > 2) val = (_wcsicmp(argv[2], L"true") == 0);
        ipc_send_pass_mode(val);
    } else if (wcscmp(argv[1], L"--sendReloadProp") == 0) {
        ipc_send_reload_prop();
    } else if (wcscmp(argv[1], L"--sendInitState") == 0) {
        ipc_send_init_state();
    } else if (wcsncmp(argv[1], L"--", 2) == 0) {
        wchar_t msg[512];
        _snwprintf(msg, 512, L"Unknown Command: %s", argv[1]);
        dialog_error(msg, L"Command Error");
        ExitProcess(1);
        return; /* not reached */
    } else {
        /* Profile name */
        if (cfg_properties_exists(argv[1]))
            cfg_set_selected_properties(argv[1]);
        return;
    }

    /* --send commands: exit immediately (pipe write is synchronous) */
    ExitProcess(0);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    /* Initialize subsystems */
    cfg_init();
    cursor_init();
    rawinput_init();

    /* Process command-line args */
    int argc;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        proc_argv(argc, argv);
        LocalFree(argv);
    }

    /* Load properties file (for language setting) */
    cfg_load_properties_file_only();

    /* Check for double launch (offers restart option) */
    check_double_launch();

    check_admin();

    /* Initialize all module callbacks */
    dispatch_init();
    scroll_init();
    waiter_init();
    event_init();
    kevent_init();

    /* Load full properties */
    cfg_load_properties(FALSE);
    settings_apply_filter_keys();

    /* Create system tray */
    HWND hwnd = tray_init(hInstance);
    (void)hwnd;

    /* Start IPC server */
    ipc_server_start();

    /* Install mouse hook */
    if (!hook_set_mouse()) {
        wchar_t err[256], msg[512];
        util_get_last_error_message(err, 256);
        _snwprintf(msg, 512, L"%s: %s",
                   L"Failed mouse hook install", err);
        dialog_error(msg, L"Error");
        return 1;
    }
    tray_start_health_timer();

    /* Set keyboard hook if configured */
    if (cfg_is_keyboard_hook())
        hook_set_or_unset_keyboard(TRUE);

    /* Main message loop */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    proc_exit();
    return 0;
}
