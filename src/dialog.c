/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "dialog.h"
#include <stdio.h>
#include <stdlib.h>

void dialog_error(const wchar_t *msg, const wchar_t *title) {
    MessageBoxW(NULL, msg, title, MB_OK | MB_ICONERROR);
}

/* ========== Custom input dialog (replaces VB InputBox) ========== */

/*
 * We build the dialog template in memory. The layout is:
 * - A static label
 * - An edit control
 * - OK and Cancel buttons
 */

#define IDD_INPUT_LABEL  10
#define IDD_INPUT_EDIT   11
#define IDD_INPUT_OK     IDOK
#define IDD_INPUT_CANCEL IDCANCEL

static wchar_t g_input_result[256];
static const wchar_t *g_input_prompt;

static INT_PTR CALLBACK input_dlg_proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        SetDlgItemTextW(hDlg, IDD_INPUT_LABEL, g_input_prompt);
        SetDlgItemTextW(hDlg, IDD_INPUT_EDIT, (const wchar_t *)lParam);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            GetDlgItemTextW(hDlg, IDD_INPUT_EDIT, g_input_result, 256);
            EndDialog(hDlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/* Build dialog template in memory */
static INT_PTR show_input_dialog(const wchar_t *title, const wchar_t *prompt,
                                 const wchar_t *default_val) {
    /* Alignment helper */
    #define ALIGN_DWORD(p) (((BYTE *)(p)) + ((4 - ((ULONG_PTR)(p) & 3)) & 3))

    BYTE buf[2048];
    memset(buf, 0, sizeof(buf));
    BYTE *p = buf;

    /* DLGTEMPLATE */
    DLGTEMPLATE *dlg = (DLGTEMPLATE *)p;
    dlg->style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_SETFONT;
    dlg->cdit = 4; /* 4 controls */
    dlg->x = 0; dlg->y = 0;
    dlg->cx = 200; dlg->cy = 70;
    p += sizeof(DLGTEMPLATE);

    /* Menu (none), Class (none), Title */
    *(WORD *)p = 0; p += 2; /* no menu */
    *(WORD *)p = 0; p += 2; /* default class */
    int tlen = (int)wcslen(title) + 1;
    memcpy(p, title, tlen * 2); p += tlen * 2;

    /* Font (8pt MS Shell Dlg) */
    *(WORD *)p = 8; p += 2;
    wcscpy((wchar_t *)p, L"MS Shell Dlg"); p += 26;

    /* --- Controls --- */

    /* Static label */
    p = ALIGN_DWORD(p);
    DLGITEMTEMPLATE *item = (DLGITEMTEMPLATE *)p;
    item->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
    item->x = 7; item->y = 7; item->cx = 186; item->cy = 12;
    item->id = IDD_INPUT_LABEL;
    p += sizeof(DLGITEMTEMPLATE);
    *(WORD *)p = 0xFFFF; p += 2;
    *(WORD *)p = 0x0082; p += 2; /* Static */
    *(WORD *)p = 0; p += 2; /* empty text (set in WM_INITDIALOG) */
    *(WORD *)p = 0; p += 2;

    /* Edit control */
    p = ALIGN_DWORD(p);
    item = (DLGITEMTEMPLATE *)p;
    item->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
    item->x = 7; item->y = 22; item->cx = 186; item->cy = 14;
    item->id = IDD_INPUT_EDIT;
    p += sizeof(DLGITEMTEMPLATE);
    *(WORD *)p = 0xFFFF; p += 2;
    *(WORD *)p = 0x0081; p += 2; /* Edit */
    *(WORD *)p = 0; p += 2;
    *(WORD *)p = 0; p += 2;

    /* OK button */
    p = ALIGN_DWORD(p);
    item = (DLGITEMTEMPLATE *)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP;
    item->x = 50; item->y = 48; item->cx = 45; item->cy = 14;
    item->id = IDOK;
    p += sizeof(DLGITEMTEMPLATE);
    *(WORD *)p = 0xFFFF; p += 2;
    *(WORD *)p = 0x0080; p += 2; /* Button */
    wcscpy((wchar_t *)p, L"OK"); p += 6;
    *(WORD *)p = 0; p += 2;

    /* Cancel button */
    p = ALIGN_DWORD(p);
    item = (DLGITEMTEMPLATE *)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP;
    item->x = 105; item->y = 48; item->cx = 45; item->cy = 14;
    item->id = IDCANCEL;
    p += sizeof(DLGITEMTEMPLATE);
    *(WORD *)p = 0xFFFF; p += 2;
    *(WORD *)p = 0x0080; p += 2; /* Button */
    wcscpy((wchar_t *)p, L"Cancel"); p += 14;
    *(WORD *)p = 0; p += 2;

    g_input_prompt = prompt;
    g_input_result[0] = L'\0';

    return DialogBoxIndirectParamW(GetModuleHandleW(NULL),
                                   (DLGTEMPLATE *)buf, NULL,
                                   input_dlg_proc, (LPARAM)default_val);

    #undef ALIGN_DWORD
}

BOOL dialog_number_input(const wchar_t *name, const wchar_t *title,
                         int low, int up, int cur, int *result) {
    wchar_t prompt[128], defval[32];
    _snwprintf(prompt, 128, L"%s (%d - %d)", name, low, up);
    _snwprintf(defval, 32, L"%d", cur);

    if (show_input_dialog(title, prompt, defval) != IDOK)
        return FALSE;

    int val = _wtoi(g_input_result);
    if (val < low || val > up) return FALSE;
    *result = val;
    return TRUE;
}

BOOL dialog_text_input(const wchar_t *msg, const wchar_t *title,
                       wchar_t *buf, int bufsize) {
    if (show_input_dialog(title, msg, L"") != IDOK)
        return FALSE;
    if (g_input_result[0] == L'\0') return FALSE;
    wcsncpy(buf, g_input_result, bufsize - 1);
    buf[bufsize - 1] = L'\0';
    return TRUE;
}
