/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#include "util.h"
#include <stdio.h>

/* ========== Single instance lock ========== */

static HANDLE g_lock_file = INVALID_HANDLE_VALUE;

BOOL util_try_lock(void) {
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, MAX_PATH, PROGRAM_NAME L".lock");

    g_lock_file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
                              0 /* no sharing */, NULL, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    return g_lock_file != INVALID_HANDLE_VALUE;
}

void util_unlock(void) {
    if (g_lock_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_lock_file);
        g_lock_file = INVALID_HANDLE_VALUE;
    }
}

/* ========== Process priority ========== */

void util_set_priority(Priority p) {
    DWORD cls;
    switch (p) {
    case PRIO_HIGH:         cls = HIGH_PRIORITY_CLASS; break;
    case PRIO_ABOVE_NORMAL: cls = ABOVE_NORMAL_PRIORITY_CLASS; break;
    default:                cls = NORMAL_PRIORITY_CLASS; break;
    }
    SetPriorityClass(GetCurrentProcess(), cls);
}

/* ========== Win32 error message ========== */

void util_get_last_error_message(wchar_t *buf, int bufsize) {
    DWORD err = GetLastError();
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, err, 0, buf, bufsize, NULL);
}
