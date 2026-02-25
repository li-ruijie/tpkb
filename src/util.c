/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <aclapi.h>
#include <sddl.h>

/* ========== Owner-only security attributes ========== */

static SECURITY_ATTRIBUTES g_owner_sa;
static SECURITY_DESCRIPTOR g_owner_sd;
static PACL g_owner_acl = NULL;
static PACL g_owner_sacl = NULL;
static BOOL g_owner_sa_valid = FALSE;

PSECURITY_ATTRIBUTES util_get_owner_sa(void) {
    if (g_owner_sa_valid)
        return &g_owner_sa;

    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return NULL;

    DWORD needed = 0;
    GetTokenInformation(hToken, TokenUser, NULL, 0, &needed);
    TOKEN_USER *tu = (TOKEN_USER *)malloc(needed);
    if (!tu) { CloseHandle(hToken); return NULL; }

    if (!GetTokenInformation(hToken, TokenUser, tu, needed, &needed)) {
        free(tu);
        CloseHandle(hToken);
        return NULL;
    }
    CloseHandle(hToken);

    EXPLICIT_ACCESS_W ea;
    memset(&ea, 0, sizeof(ea));
    ea.grfAccessPermissions = GENERIC_ALL;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.ptstrName = (LPWSTR)tu->User.Sid;

    DWORD rc = SetEntriesInAclW(1, &ea, NULL, &g_owner_acl);
    free(tu);
    if (rc != ERROR_SUCCESS)
        return NULL;

    InitializeSecurityDescriptor(&g_owner_sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&g_owner_sd, TRUE, g_owner_acl, FALSE);

    /* Medium Mandatory Label: allows non-elevated instances to access
       objects created by elevated instances (cross-elevation IPC). */
    PSID medium_sid = NULL;
    if (ConvertStringSidToSidW(L"S-1-16-8192", &medium_sid)) {
        DWORD sid_len = GetLengthSid(medium_sid);
        DWORD sacl_size = sizeof(ACL) + sizeof(SYSTEM_MANDATORY_LABEL_ACE)
                          - sizeof(DWORD) + sid_len;
        g_owner_sacl = (PACL)LocalAlloc(LPTR, sacl_size);
        if (g_owner_sacl) {
            InitializeAcl(g_owner_sacl, sacl_size, ACL_REVISION);
            AddMandatoryAce(g_owner_sacl, ACL_REVISION, 0,
                            SYSTEM_MANDATORY_LABEL_NO_WRITE_UP, medium_sid);
            SetSecurityDescriptorSacl(&g_owner_sd, TRUE, g_owner_sacl, FALSE);
        }
        LocalFree(medium_sid);
    }

    g_owner_sa.nLength = sizeof(g_owner_sa);
    g_owner_sa.lpSecurityDescriptor = &g_owner_sd;
    g_owner_sa.bInheritHandle = FALSE;
    g_owner_sa_valid = TRUE;

    return &g_owner_sa;
}

/* ========== Single instance lock ========== */

static HANDLE g_mutex = NULL;

BOOL util_try_lock(void) {
    g_mutex = CreateMutexW(util_get_owner_sa(), FALSE, L"Local\\tpkb_SingleInstance");
    if (!g_mutex) return FALSE;
    DWORD result = WaitForSingleObject(g_mutex, 0);
    if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED)
        return TRUE;
    CloseHandle(g_mutex);
    g_mutex = NULL;
    return FALSE;
}

void util_unlock(void) {
    if (g_mutex) {
        ReleaseMutex(g_mutex);
        CloseHandle(g_mutex);
        g_mutex = NULL;
    }
}

void util_cleanup(void) {
    if (g_owner_sacl) {
        LocalFree(g_owner_sacl);
        g_owner_sacl = NULL;
    }
    if (g_owner_acl) {
        LocalFree(g_owner_acl);
        g_owner_acl = NULL;
        g_owner_sa_valid = FALSE;
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
