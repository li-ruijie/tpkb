/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "ipc.h"
#include "types.h"
#include "config.h"
#include "scroll.h"
#include <tlhelp32.h>

#define W10_MESSAGE_BASE  (264816059 & 0x0FFFFFFF)
#define W10_MESSAGE_EXIT       (W10_MESSAGE_BASE + 1)
#define W10_MESSAGE_PASSMODE   (W10_MESSAGE_BASE + 2)
#define W10_MESSAGE_RELOAD     (W10_MESSAGE_BASE + 3)
#define W10_MESSAGE_INITSTATE  (W10_MESSAGE_BASE + 4)

static BOOL is_valid_sender(void) {
    DWORD my_pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return FALSE;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    BOOL found = FALSE;

    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID != my_pid &&
                _wcsicmp(pe.szExeFile, L"W10Wheel.exe") == 0) {
                found = TRUE;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return found;
}

static void send_message(int msg) {
    POINT pt;
    GetCursorPos(&pt);
    scroll_send_input_direct(pt, 1, W10_MOUSEEVENTF_HWHEEL, 0, (DWORD)msg);
}

void ipc_send_exit(void) {
    send_message(W10_MESSAGE_EXIT);
}

void ipc_send_pass_mode(BOOL b) {
    int msg = W10_MESSAGE_PASSMODE | (b ? 0x10000000 : 0);
    send_message(msg);
}

void ipc_send_reload_prop(void) {
    send_message(W10_MESSAGE_RELOAD);
}

void ipc_send_init_state(void) {
    send_message(W10_MESSAGE_INITSTATE);
}

BOOL ipc_proc_message(int msg) {
    if (!is_valid_sender()) return FALSE;

    int flag = msg & 0x0FFFFFFF;
    BOOL bval = (msg & 0xF0000000) != 0;

    if (flag == W10_MESSAGE_EXIT) {
        cfg_exit_action();
        return TRUE;
    }
    if (flag == W10_MESSAGE_PASSMODE) {
        cfg_set_pass_mode(bval);
        return TRUE;
    }
    if (flag == W10_MESSAGE_RELOAD) {
        cfg_reload_properties();
        return TRUE;
    }
    if (flag == W10_MESSAGE_INITSTATE) {
        cfg_init_state();
        return TRUE;
    }
    return FALSE;
}
