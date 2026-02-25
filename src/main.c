/*
 * Copyright (c) 2016-2021 Yuki Ono
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
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
#include "dialog.h"
#include "cursor.h"
#include "rawinput.h"
#include "ipc.h"
#include "util.h"
#include <wchar.h>
#include <shellapi.h>

static void proc_exit(void) {
    hook_unhook();
    cfg_store_properties();
    util_unlock();
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

    /* --send commands: wait and exit */
    Sleep(1000);
    ExitProcess(0);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

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

    /* Check for double launch */
    if (!util_try_lock()) {
        MessageBoxW(NULL, cfg_conv_lang(L"Double Launch?"),
                    cfg_conv_lang(L"Error"), MB_OK | MB_ICONERROR);
        return 1;
    }

    /* Initialize all module callbacks */
    dispatch_init();
    scroll_init();
    waiter_init();
    event_init();
    kevent_init();

    /* Load full properties */
    cfg_load_properties(FALSE);

    /* Create system tray */
    HWND hwnd = tray_init(hInstance);
    (void)hwnd;

    /* Install mouse hook */
    if (!hook_set_mouse()) {
        wchar_t err[256], msg[512];
        util_get_last_error_message(err, 256);
        _snwprintf(msg, 512, L"%s: %s",
                   cfg_conv_lang(L"Failed mouse hook install"), err);
        dialog_error(msg, cfg_conv_lang(L"Error"));
        return 1;
    }

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
