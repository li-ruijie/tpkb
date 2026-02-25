/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "ipc.h"
#include "types.h"
#include "config.h"
#include "tray.h"
#include "util.h"
#include <stdio.h>
#include <process.h>

static wchar_t g_pipe_name[64];

static void init_pipe_name(void) {
    DWORD session_id = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session_id);
    _snwprintf(g_pipe_name, 64, L"\\\\.\\pipe\\LOCAL\\tpkb_%lu", session_id);
}

#define TPKB_MESSAGE_BASE  (264816059 & 0x0FFFFFFF)
#define TPKB_MESSAGE_EXIT       (TPKB_MESSAGE_BASE + 1)
#define TPKB_MESSAGE_PASSMODE   (TPKB_MESSAGE_BASE + 2)
#define TPKB_MESSAGE_RELOAD     (TPKB_MESSAGE_BASE + 3)
#define TPKB_MESSAGE_INITSTATE  (TPKB_MESSAGE_BASE + 4)

/* ========== Server (running instance) ========== */

static HANDLE g_pipe = INVALID_HANDLE_VALUE;
static HANDLE g_server_thread = NULL;
static PSECURITY_ATTRIBUTES g_pipe_sa = NULL;

void ipc_proc_message(int msg) {
    int flag = msg & 0x0FFFFFFF;
    BOOL bval = (msg & 0xF0000000) != 0;

    if (flag == TPKB_MESSAGE_EXIT)
        cfg_exit_action();
    else if (flag == TPKB_MESSAGE_PASSMODE) {
        cfg_set_pass_mode(bval);
        tray_update_icon();
    }
    else if (flag == TPKB_MESSAGE_RELOAD)
        cfg_reload_properties();
    else if (flag == TPKB_MESSAGE_INITSTATE)
        cfg_init_state();
}

static volatile BOOL g_server_running = FALSE;

static unsigned __stdcall server_proc(void *arg) {
    (void)arg;
    HANDLE ol_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    while (g_server_running) {
        g_pipe = CreateNamedPipeW(
            g_pipe_name,
            PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1, 0, sizeof(int), 0, g_pipe_sa);

        if (g_pipe == INVALID_HANDLE_VALUE)
            break;

        /* Overlapped ConnectNamedPipe */
        OVERLAPPED ol = {0};
        ol.hEvent = ol_event;
        if (!ConnectNamedPipe(g_pipe, &ol)) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                WaitForSingleObject(ol_event, INFINITE);
                if (!g_server_running) {
                    CancelIo(g_pipe);
                    CloseHandle(g_pipe);
                    g_pipe = INVALID_HANDLE_VALUE;
                    break;
                }
            } else if (err != ERROR_PIPE_CONNECTED) {
                CloseHandle(g_pipe);
                g_pipe = INVALID_HANDLE_VALUE;
                continue;
            }
        }

        /* Overlapped ReadFile with 250ms timeout */
        int msg = 0;
        OVERLAPPED rd_ol = {0};
        rd_ol.hEvent = ol_event;
        ResetEvent(ol_event);
        if (ReadFile(g_pipe, &msg, sizeof(msg), NULL, &rd_ol) ||
            (GetLastError() == ERROR_IO_PENDING &&
             WaitForSingleObject(ol_event, 250) == WAIT_OBJECT_0)) {
            DWORD bytes_read;
            if (GetOverlappedResult(g_pipe, &rd_ol, &bytes_read, FALSE) &&
                bytes_read == sizeof(msg)) {
                PostMessageW(tray_get_hwnd(), WM_APP + 1, (WPARAM)msg, 0);
            }
        } else {
            CancelIo(g_pipe);
            DWORD dummy;
            GetOverlappedResult(g_pipe, &rd_ol, &dummy, TRUE);
        }

        DisconnectNamedPipe(g_pipe);
        CloseHandle(g_pipe);
        g_pipe = INVALID_HANDLE_VALUE;
    }
    CloseHandle(ol_event);
    return 0;
}

void ipc_server_start(void) {
    init_pipe_name();
    g_pipe_sa = util_get_owner_sa();
    g_server_running = TRUE;
    g_server_thread = (HANDLE)_beginthreadex(NULL, 0, server_proc, NULL, 0, NULL);
}

void ipc_server_stop(void) {
    g_server_running = FALSE;
    /* Dummy connect to unblock ConnectNamedPipe safely */
    HANDLE dummy = CreateFileW(g_pipe_name, GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, 0, NULL);
    if (dummy != INVALID_HANDLE_VALUE)
        CloseHandle(dummy);
    if (g_server_thread) {
        WaitForSingleObject(g_server_thread, 2000);
        CloseHandle(g_server_thread);
        g_server_thread = NULL;
    }
}

/* ========== Client (--send* commands) ========== */

static BOOL send_message(int msg) {
    init_pipe_name();
    HANDLE pipe = CreateFileW(g_pipe_name, GENERIC_WRITE, 0, NULL,
                              OPEN_EXISTING, 0, NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        if (GetLastError() != ERROR_PIPE_BUSY)
            return FALSE;
        if (!WaitNamedPipeW(g_pipe_name, 2000))
            return FALSE;
        pipe = CreateFileW(g_pipe_name, GENERIC_WRITE, 0, NULL,
                           OPEN_EXISTING, 0, NULL);
        if (pipe == INVALID_HANDLE_VALUE)
            return FALSE;
    }

    DWORD written;
    BOOL ok = WriteFile(pipe, &msg, sizeof(msg), &written, NULL);
    CloseHandle(pipe);
    return ok && written == sizeof(msg);
}

void ipc_send_exit(void) {
    send_message(TPKB_MESSAGE_EXIT);
}

void ipc_send_pass_mode(BOOL b) {
    int msg = TPKB_MESSAGE_PASSMODE | (b ? 0x10000000 : 0);
    send_message(msg);
}

void ipc_send_reload_prop(void) {
    send_message(TPKB_MESSAGE_RELOAD);
}

void ipc_send_init_state(void) {
    send_message(TPKB_MESSAGE_INITSTATE);
}
