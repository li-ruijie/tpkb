/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "settings.h"
#include "types.h"
#include "config.h"
#include "hook.h"
#include "tray.h"
#include "dialog.h"
#include "../res/resource.h"
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>

/* Message sent to pages to refresh from config (e.g. after profile switch) */
#define WM_SETTINGS_REFRESH (WM_APP + 1)

static HWND g_page_hwnd[7];
static BOOL g_initializing; /* suppress change notifications during init */

/* ========== Refresh all pages ========== */

static void refresh_all_pages(void) {
    for (int i = 0; i < 7; i++)
        if (g_page_hwnd[i])
            SendMessageW(g_page_hwnd[i], WM_SETTINGS_REFRESH, 0, 0);
}

/* ========== Helpers ========== */

static void mark_changed(HWND hDlg) {
    if (!g_initializing)
        PropSheet_Changed(GetParent(hDlg), hDlg);
}

static void set_spin_range(HWND hDlg, int spin_id, int lo, int hi, int val) {
    SendDlgItemMessageW(hDlg, spin_id, UDM_SETRANGE32, lo, hi);
    SendDlgItemMessageW(hDlg, spin_id, UDM_SETPOS32, 0, val);
}

static BOOL get_validated_spin(HWND hDlg, int spin_id, int lo, int hi,
                               const wchar_t *name, int *out) {
    BOOL err = FALSE;
    int val = (int)SendDlgItemMessageW(hDlg, spin_id, UDM_GETPOS32, 0, (LPARAM)&err);
    if (err || val < lo || val > hi) {
        wchar_t msg[128];
        _snwprintf(msg, 128, L"%s: %d \x2013 %d", name, lo, hi);
        dialog_error(msg, L"Invalid Number");
        return FALSE;
    }
    *out = val;
    return TRUE;
}

/* Create Help (?) and Reset buttons at bottom of a page */
static void add_page_buttons(HWND hDlg) {
    HFONT hFont = (HFONT)SendMessageW(hDlg, WM_GETFONT, 0, 0);
    HINSTANCE hInst = GetModuleHandleW(NULL);

    /* Map dialog units to pixels: 14 DLU height, positioned at y=184 DLU */
    RECT rc = { 7, 184, 7 + 20, 184 + 14 };
    MapDialogRect(hDlg, &rc);
    int y = rc.top, h = rc.bottom - rc.top;

    RECT rc2 = { 0, 0, 20, 0 };
    MapDialogRect(hDlg, &rc2);
    int btn_w_help = rc2.right; /* 20 DLU */
    rc2.right = 40;
    MapDialogRect(hDlg, &rc2);
    int btn_w_reset = rc2.right; /* 40 DLU */

    RECT rc3 = { 7, 0, 0, 0 };
    MapDialogRect(hDlg, &rc3);
    int x = rc3.left;

    HWND hw = CreateWindowExW(0, L"BUTTON", L"?",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        x, y, btn_w_help, h, hDlg, (HMENU)(INT_PTR)IDC_TAB_HELP, hInst, NULL);
    if (hFont) SendMessageW(hw, WM_SETFONT, (WPARAM)hFont, 0);

    hw = CreateWindowExW(0, L"BUTTON", L"Reset",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        x + btn_w_help + 4, y, btn_w_reset, h, hDlg, (HMENU)(INT_PTR)IDC_TAB_RESET, hInst, NULL);
    if (hFont) SendMessageW(hw, WM_SETFONT, (WPARAM)hFont, 0);
}

/* ========== Help text ========== */

static const wchar_t *HELP_GENERAL =
    L"Trigger: Which mouse button(s) activate scroll mode.\r\n"
    L"  LR = hold Left then Right (or vice versa) to scroll.\r\n"
    L"  Drag variants = hold button and drag to scroll.\r\n"
    L"  None = use keyboard trigger only.\r\n\r\n"
    L"Send MiddleClick: Send a middle-click when the trigger\r\n"
    L"  button is released without scrolling (single/double triggers).\r\n\r\n"
    L"Dragged Lock: Keep scroll mode active after releasing\r\n"
    L"  the trigger button (drag triggers only). Click to exit.\r\n\r\n"
    L"Hotkey: Enable a keyboard key as scroll mode trigger.\r\n"
    L"  VK Code: which key to use (e.g., VK_SCROLL = Scroll Lock).\r\n\r\n"
    L"Priority: Process priority. Higher = more responsive scrolling\r\n"
    L"  but uses more CPU.\r\n\r\n"
    L"Health check interval: Seconds between hook health checks\r\n"
    L"  (0 = off). Monitors cursor movement to detect when Windows\r\n"
    L"  silently drops the mouse hook, and reinstalls it\r\n"
    L"  automatically. Does not affect power management or sleep.";

static const wchar_t *HELP_SCROLL =
    L"Cursor Change: Change the mouse cursor while in scroll mode\r\n"
    L"  to indicate scroll direction.\r\n\r\n"
    L"Horizontal Scroll: Allow horizontal scrolling by moving\r\n"
    L"  the mouse left/right while in scroll mode.\r\n\r\n"
    L"Reverse Scroll: Invert the vertical scroll direction\r\n"
    L"  (natural/reverse scrolling).\r\n\r\n"
    L"Swap Scroll (V/H): Swap vertical and horizontal scroll axes.\r\n\r\n"
    L"Button press timeout (50\x2013" L"500 ms): Time window to detect\r\n"
    L"  simultaneous button presses for LR/Left/Right triggers.\r\n\r\n"
    L"Scroll lock time (150\x2013" L"500 ms): Minimum time before scroll\r\n"
    L"  mode can be exited. Prevents accidental exits.\r\n\r\n"
    L"Vertical threshold (0\x2013" L"500): Minimum mouse movement in\r\n"
    L"  pixels before vertical scrolling starts.\r\n\r\n"
    L"Horizontal threshold (0\x2013" L"500): Same for horizontal scrolling.\r\n\r\n"
    L"Drag threshold (0\x2013" L"500): Mouse movement threshold\r\n"
    L"  to distinguish drag-scroll from a click (drag triggers).";

static const wchar_t *HELP_ACCEL =
    L"Enable acceleration: Turn on scroll acceleration.\r\n"
    L"  Faster mouse movement = larger scroll steps.\r\n\r\n"
    L"Preset (M5\x2013M9): Kensington MouseWorks-style multiplier\r\n"
    L"  tables. M5 is gentlest, M9 is most aggressive.\r\n\r\n"
    L"Use custom table: Override presets with your own values.\r\n"
    L"  Thresholds: comma-separated positive integers (mouse\r\n"
    L"  speed breakpoints in pixels).\r\n"
    L"  E.g.: 1,2,3,5,7,10\r\n"
    L"  Multipliers: comma-separated positive decimals\r\n"
    L"  (scroll multiplier at each speed; 1.0 = normal).\r\n"
    L"  E.g.: 1.0,1.5,2.0,2.5,3.0,3.5\r\n\r\n"
    L"  Both arrays must have the same number of entries.\r\n"
    L"  Maximum 64 entries per array.";

static const wchar_t *HELP_REALWHEEL =
    L"Enable real wheel mode: Simulate actual mouse wheel events\r\n"
    L"  instead of using SendInput. Some apps respond better to\r\n"
    L"  real wheel messages.\r\n\r\n"
    L"Wheel delta (10\x2013" L"500): Size of each scroll step.\r\n"
    L"  Standard mouse wheel = 120. Smaller = smoother scrolling.\r\n\r\n"
    L"Vertical speed (10\x2013" L"500): Vertical scroll speed\r\n"
    L"  (higher = faster).\r\n\r\n"
    L"Horizontal speed (10\x2013" L"500): Horizontal scroll speed.\r\n\r\n"
    L"Quick first scroll: Make the first scroll event fire\r\n"
    L"  immediately instead of waiting for a threshold.\r\n"
    L"  Feels more responsive.\r\n\r\n"
    L"Quick direction change: Respond instantly when scroll\r\n"
    L"  direction changes. Useful for quickly switching between\r\n"
    L"  scrolling up and down.";

static const wchar_t *HELP_VHADJ =
    L"Enable VH adjuster: Constrain scrolling to vertical-only\r\n"
    L"  or horizontal-only based on initial movement direction.\r\n"
    L"  Prevents diagonal wobble. Requires Horizontal Scroll on.\r\n\r\n"
    L"Fixed: Lock to the first detected direction (V or H)\r\n"
    L"  for the entire scroll session.\r\n\r\n"
    L"Switching: Allow direction to switch if mouse movement\r\n"
    L"  changes axis. More flexible but may feel less precise.\r\n\r\n"
    L"Prefer vertical first: When initial movement is ambiguous,\r\n"
    L"  prefer vertical scrolling over horizontal.\r\n\r\n"
    L"Min. threshold (1\x2013" L"10): Minimum movement in pixels\r\n"
    L"  before locking direction. Higher = more deliberate.\r\n\r\n"
    L"Switching threshold (10\x2013" L"500): Movement required to switch\r\n"
    L"  direction (Switching mode only).";

static void show_help(HWND hDlg, const wchar_t *text) {
    MessageBoxW(hDlg, text, L"Help", MB_OK | MB_ICONINFORMATION);
}

/* ========== Page: General ========== */

static void general_init(HWND hDlg) {
    g_initializing = TRUE;

    /* Trigger combo */
    HWND hTrig = GetDlgItem(hDlg, IDC_TRIGGER_COMBO);
    SendMessageW(hTrig, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < TRIGGER_ENTRY_COUNT; i++)
        SendMessageW(hTrig, CB_ADDSTRING, 0, (LPARAM)TRIGGER_TABLE[i].name);
    SendMessageW(hTrig, CB_SETCURSEL, (WPARAM)cfg_get_trigger(), 0);

    CheckDlgButton(hDlg, IDC_SEND_MIDDLE_CLICK, cfg_is_send_middle_click() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_DRAGGED_LOCK, cfg_is_dragged_lock() ? BST_CHECKED : BST_UNCHECKED);

    /* Keyboard */
    CheckDlgButton(hDlg, IDC_KEYBOARD_ENABLE, cfg_is_keyboard_hook() ? BST_CHECKED : BST_UNCHECKED);

    HWND hVK = GetDlgItem(hDlg, IDC_VK_COMBO);
    SendMessageW(hVK, CB_RESETCONTENT, 0, 0);
    int cur_vk = cfg_get_target_vk_code();
    int vk_sel = 0;
    for (int i = 0; i < VK_ENTRY_COUNT; i++) {
        SendMessageW(hVK, CB_ADDSTRING, 0, (LPARAM)VK_TABLE[i].name);
        if (VK_TABLE[i].code == cur_vk) vk_sel = i;
    }
    SendMessageW(hVK, CB_SETCURSEL, vk_sel, 0);

    /* Priority */
    HWND hPrio = GetDlgItem(hDlg, IDC_PRIORITY_COMBO);
    SendMessageW(hPrio, CB_RESETCONTENT, 0, 0);
    SendMessageW(hPrio, CB_ADDSTRING, 0, (LPARAM)L"High");
    SendMessageW(hPrio, CB_ADDSTRING, 0, (LPARAM)L"Above Normal");
    SendMessageW(hPrio, CB_ADDSTRING, 0, (LPARAM)L"Normal");
    Priority prio = cfg_get_priority();
    int prio_sel = (prio == PRIO_HIGH) ? 0 : (prio == PRIO_ABOVE_NORMAL) ? 1 : 2;
    SendMessageW(hPrio, CB_SETCURSEL, prio_sel, 0);

    /* hookHealthCheck */
    set_spin_range(hDlg, IDC_HOOK_HEALTH_SPIN, 0, 300, cfg_get_hook_health_check());

    g_initializing = FALSE;
}

static BOOL general_apply(HWND hDlg) {
    /* Validate spin */
    int health;
    if (!get_validated_spin(hDlg, IDC_HOOK_HEALTH_SPIN, 0, 300, L"Health check interval", &health))
        return FALSE;

    /* Trigger */
    int sel = (int)SendDlgItemMessageW(hDlg, IDC_TRIGGER_COMBO, CB_GETCURSEL, 0, 0);
    if (sel >= 0 && sel < TRIGGER_ENTRY_COUNT)
        cfg_set_trigger((Trigger)sel);

    cfg_set_boolean(L"sendMiddleClick", IsDlgButtonChecked(hDlg, IDC_SEND_MIDDLE_CLICK) == BST_CHECKED);
    cfg_set_boolean(L"draggedLock", IsDlgButtonChecked(hDlg, IDC_DRAGGED_LOCK) == BST_CHECKED);

    /* Keyboard */
    BOOL kb = IsDlgButtonChecked(hDlg, IDC_KEYBOARD_ENABLE) == BST_CHECKED;
    cfg_set_boolean(L"keyboardHook", kb);
    hook_set_or_unset_keyboard(kb);

    int vk_sel = (int)SendDlgItemMessageW(hDlg, IDC_VK_COMBO, CB_GETCURSEL, 0, 0);
    if (vk_sel >= 0 && vk_sel < VK_ENTRY_COUNT)
        cfg_set_vk_code_name(VK_TABLE[vk_sel].name);

    /* Priority */
    int prio_sel = (int)SendDlgItemMessageW(hDlg, IDC_PRIORITY_COMBO, CB_GETCURSEL, 0, 0);
    static const wchar_t *prio_names[] = { L"High", L"AboveNormal", L"Normal" };
    if (prio_sel >= 0 && prio_sel < 3)
        cfg_set_priority_name(prio_names[prio_sel]);

    /* hookHealthCheck */
    cfg_set_number(L"hookHealthCheck", health);
    tray_update_health_timer();

    return TRUE;
}

static void general_reset(HWND hDlg) {
    g_initializing = TRUE;
    SendDlgItemMessageW(hDlg, IDC_TRIGGER_COMBO, CB_SETCURSEL, TRIGGER_LR, 0);
    CheckDlgButton(hDlg, IDC_SEND_MIDDLE_CLICK, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_DRAGGED_LOCK, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_KEYBOARD_ENABLE, BST_UNCHECKED);
    SendDlgItemMessageW(hDlg, IDC_VK_COMBO, CB_SETCURSEL, 0, 0);
    SendDlgItemMessageW(hDlg, IDC_PRIORITY_COMBO, CB_SETCURSEL, 1, 0); /* Above Normal */
    SendDlgItemMessageW(hDlg, IDC_HOOK_HEALTH_SPIN, UDM_SETPOS32, 0, 0);
    g_initializing = FALSE;
    mark_changed(hDlg);
}

static INT_PTR CALLBACK general_proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        g_page_hwnd[0] = hDlg;
        general_init(hDlg);
        add_page_buttons(hDlg);

        return TRUE;

    case WM_SETTINGS_REFRESH:
        general_init(hDlg);

        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_TAB_HELP) { show_help(hDlg, HELP_GENERAL); return TRUE; }
        if (LOWORD(wParam) == IDC_TAB_RESET) { general_reset(hDlg); return TRUE; }
        switch (HIWORD(wParam)) {
        case BN_CLICKED: case CBN_SELCHANGE: case EN_CHANGE:
            mark_changed(hDlg);
        }
        break;

    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lParam;
        if (nm->code == PSN_APPLY) {
            if (!general_apply(hDlg)) {
                SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
                return TRUE;
            }
            SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        if (nm->code == UDN_DELTAPOS)
            mark_changed(hDlg);
        break;
    }
    }
    return FALSE;
}

/* ========== Page: Scroll ========== */

static void scroll_init(HWND hDlg) {
    g_initializing = TRUE;

    CheckDlgButton(hDlg, IDC_CURSOR_CHANGE, cfg_is_cursor_change() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_HORIZONTAL_SCROLL, cfg_is_horizontal_scroll() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_REVERSE_SCROLL, cfg_is_reverse_scroll() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SWAP_SCROLL, cfg_is_swap_scroll() ? BST_CHECKED : BST_UNCHECKED);

    set_spin_range(hDlg, IDC_POLL_TIMEOUT_SPIN, 50, 500, cfg_get_number(L"pollTimeout"));
    set_spin_range(hDlg, IDC_SCROLL_LOCK_SPIN, 150, 500, cfg_get_number(L"scrollLocktime"));
    set_spin_range(hDlg, IDC_VERT_THR_SPIN, 0, 500, cfg_get_number(L"verticalThreshold"));
    set_spin_range(hDlg, IDC_HORIZ_THR_SPIN, 0, 500, cfg_get_number(L"horizontalThreshold"));
    set_spin_range(hDlg, IDC_DRAG_THR_SPIN, 0, 500, cfg_get_number(L"dragThreshold"));

    g_initializing = FALSE;
}

static BOOL scroll_apply(HWND hDlg) {
    /* Validate all spins first */
    int poll, lock, vert, horiz, drag;
    if (!get_validated_spin(hDlg, IDC_POLL_TIMEOUT_SPIN, 50, 500, L"Button press timeout", &poll)) return FALSE;
    if (!get_validated_spin(hDlg, IDC_SCROLL_LOCK_SPIN, 150, 500, L"Scroll lock time", &lock)) return FALSE;
    if (!get_validated_spin(hDlg, IDC_VERT_THR_SPIN, 0, 500, L"Vertical threshold", &vert)) return FALSE;
    if (!get_validated_spin(hDlg, IDC_HORIZ_THR_SPIN, 0, 500, L"Horizontal threshold", &horiz)) return FALSE;
    if (!get_validated_spin(hDlg, IDC_DRAG_THR_SPIN, 0, 500, L"Drag threshold", &drag)) return FALSE;

    cfg_set_boolean(L"cursorChange", IsDlgButtonChecked(hDlg, IDC_CURSOR_CHANGE) == BST_CHECKED);
    cfg_set_boolean(L"horizontalScroll", IsDlgButtonChecked(hDlg, IDC_HORIZONTAL_SCROLL) == BST_CHECKED);
    cfg_set_boolean(L"reverseScroll", IsDlgButtonChecked(hDlg, IDC_REVERSE_SCROLL) == BST_CHECKED);
    cfg_set_boolean(L"swapScroll", IsDlgButtonChecked(hDlg, IDC_SWAP_SCROLL) == BST_CHECKED);

    cfg_set_number(L"pollTimeout", poll);
    cfg_set_number(L"scrollLocktime", lock);
    cfg_set_number(L"verticalThreshold", vert);
    cfg_set_number(L"horizontalThreshold", horiz);
    cfg_set_number(L"dragThreshold", drag);

    return TRUE;
}

static void scroll_reset(HWND hDlg) {
    g_initializing = TRUE;
    CheckDlgButton(hDlg, IDC_CURSOR_CHANGE, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_HORIZONTAL_SCROLL, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_REVERSE_SCROLL, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_SWAP_SCROLL, BST_UNCHECKED);
    SendDlgItemMessageW(hDlg, IDC_POLL_TIMEOUT_SPIN, UDM_SETPOS32, 0, 200);
    SendDlgItemMessageW(hDlg, IDC_SCROLL_LOCK_SPIN, UDM_SETPOS32, 0, 200);
    SendDlgItemMessageW(hDlg, IDC_VERT_THR_SPIN, UDM_SETPOS32, 0, 0);
    SendDlgItemMessageW(hDlg, IDC_HORIZ_THR_SPIN, UDM_SETPOS32, 0, 75);
    SendDlgItemMessageW(hDlg, IDC_DRAG_THR_SPIN, UDM_SETPOS32, 0, 0);
    g_initializing = FALSE;
    mark_changed(hDlg);
}

static INT_PTR CALLBACK scroll_proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        g_page_hwnd[1] = hDlg;
        scroll_init(hDlg);
        add_page_buttons(hDlg);

        return TRUE;

    case WM_SETTINGS_REFRESH:
        scroll_init(hDlg);

        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_TAB_HELP) { show_help(hDlg, HELP_SCROLL); return TRUE; }
        if (LOWORD(wParam) == IDC_TAB_RESET) { scroll_reset(hDlg); return TRUE; }
        switch (HIWORD(wParam)) {
        case BN_CLICKED: case CBN_SELCHANGE: case EN_CHANGE:
            mark_changed(hDlg);
        }
        break;

    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lParam;
        if (nm->code == PSN_APPLY) {
            if (!scroll_apply(hDlg)) {
                SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
                return TRUE;
            }
            SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        if (nm->code == UDN_DELTAPOS)
            mark_changed(hDlg);
        break;
    }
    }
    return FALSE;
}

/* ========== Accel preset prefill ========== */

static void accel_prefill_custom(HWND hDlg, int preset_idx) {
    const double *mul = accel_preset_array((AccelPreset)preset_idx);
    wchar_t thr_buf[256], mul_buf[256];
    int tpos = 0, mpos = 0;
    for (int i = 0; i < ACCEL_TABLE_SIZE; i++) {
        if (i > 0) { thr_buf[tpos++] = L','; mul_buf[mpos++] = L','; }
        tpos += _snwprintf(thr_buf + tpos, 256 - tpos, L"%d", DEFAULT_ACCEL_THRESHOLD[i]);
        mpos += _snwprintf(mul_buf + mpos, 256 - mpos, L"%.1f", mul[i]);
    }
    thr_buf[tpos] = L'\0';
    mul_buf[mpos] = L'\0';
    SetDlgItemTextW(hDlg, IDC_CUSTOM_THRESHOLD, thr_buf);
    SetDlgItemTextW(hDlg, IDC_CUSTOM_MULTIPLIER, mul_buf);
}

/* ========== Page: Acceleration ========== */

static void accel_init(HWND hDlg) {
    g_initializing = TRUE;

    CheckDlgButton(hDlg, IDC_ACCEL_ENABLE, cfg_is_accel_table() ? BST_CHECKED : BST_UNCHECKED);

    /* Preset radios */
    AccelPreset preset = cfg_get_accel_preset();
    int radio_ids[] = { IDC_ACCEL_M5, IDC_ACCEL_M6, IDC_ACCEL_M7, IDC_ACCEL_M8, IDC_ACCEL_M9 };
    for (int i = 0; i < 5; i++)
        CheckDlgButton(hDlg, radio_ids[i], (i == (int)preset) ? BST_CHECKED : BST_UNCHECKED);

    /* Custom accel */
    BOOL custom = cfg_is_custom_accel();
    CheckDlgButton(hDlg, IDC_CUSTOM_ENABLE, custom ? BST_CHECKED : BST_UNCHECKED);

    if (custom) {
        wchar_t thr_buf[1024], mul_buf[1024];
        cfg_get_custom_accel_strings(thr_buf, 1024, mul_buf, 1024);
        SetDlgItemTextW(hDlg, IDC_CUSTOM_THRESHOLD, thr_buf);
        SetDlgItemTextW(hDlg, IDC_CUSTOM_MULTIPLIER, mul_buf);
    } else {
        accel_prefill_custom(hDlg, (int)preset);
    }

    g_initializing = FALSE;
}

static BOOL accel_apply(HWND hDlg) {
    cfg_set_boolean(L"accelTable", IsDlgButtonChecked(hDlg, IDC_ACCEL_ENABLE) == BST_CHECKED);

    /* Preset */
    int radio_ids[] = { IDC_ACCEL_M5, IDC_ACCEL_M6, IDC_ACCEL_M7, IDC_ACCEL_M8, IDC_ACCEL_M9 };
    static const wchar_t *preset_names[] = { L"M5", L"M6", L"M7", L"M8", L"M9" };
    for (int i = 0; i < 5; i++) {
        if (IsDlgButtonChecked(hDlg, radio_ids[i]) == BST_CHECKED) {
            cfg_set_accel_multiplier_name(preset_names[i]);
            break;
        }
    }

    /* Custom */
    BOOL custom = IsDlgButtonChecked(hDlg, IDC_CUSTOM_ENABLE) == BST_CHECKED;
    cfg_set_boolean(L"customAccelTable", custom);

    if (custom) {
        wchar_t thr_buf[1024], mul_buf[1024];
        GetDlgItemTextW(hDlg, IDC_CUSTOM_THRESHOLD, thr_buf, 1024);
        GetDlgItemTextW(hDlg, IDC_CUSTOM_MULTIPLIER, mul_buf, 1024);

        /* Require both fields when custom is enabled */
        if (!thr_buf[0] || !mul_buf[0]) {
            dialog_error(L"Invalid Number", L"Error");
            return FALSE;
        }
        if (!cfg_set_custom_accel_strings(thr_buf, mul_buf)) {
            dialog_error(L"Invalid Number", L"Error");
            return FALSE;
        }
    }

    return TRUE;
}

static void accel_reset(HWND hDlg) {
    g_initializing = TRUE;
    CheckDlgButton(hDlg, IDC_ACCEL_ENABLE, BST_CHECKED);
    CheckDlgButton(hDlg, IDC_ACCEL_M5, BST_CHECKED);
    CheckDlgButton(hDlg, IDC_ACCEL_M6, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_ACCEL_M7, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_ACCEL_M8, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_ACCEL_M9, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CUSTOM_ENABLE, BST_UNCHECKED);
    SetDlgItemTextW(hDlg, IDC_CUSTOM_THRESHOLD, L"");
    SetDlgItemTextW(hDlg, IDC_CUSTOM_MULTIPLIER, L"");
    g_initializing = FALSE;
    mark_changed(hDlg);
}

static INT_PTR CALLBACK accel_proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        g_page_hwnd[2] = hDlg;
        accel_init(hDlg);
        add_page_buttons(hDlg);

        return TRUE;

    case WM_SETTINGS_REFRESH:
        accel_init(hDlg);

        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_TAB_HELP) { show_help(hDlg, HELP_ACCEL); return TRUE; }
        if (LOWORD(wParam) == IDC_TAB_RESET) { accel_reset(hDlg); return TRUE; }
        if (HIWORD(wParam) == BN_CLICKED) {
            int id = LOWORD(wParam);
            if (id >= IDC_ACCEL_M5 && id <= IDC_ACCEL_M9)
                accel_prefill_custom(hDlg, id - IDC_ACCEL_M5);
            mark_changed(hDlg);
        } else switch (HIWORD(wParam)) {
        case CBN_SELCHANGE: case EN_CHANGE:
            mark_changed(hDlg);
        }
        break;

    case WM_NOTIFY:
        if (((NMHDR *)lParam)->code == PSN_APPLY) {
            if (!accel_apply(hDlg)) {
                SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
                return TRUE;
            }
            SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/* ========== Page: Real Wheel ========== */

static void realwheel_init(HWND hDlg) {
    g_initializing = TRUE;

    CheckDlgButton(hDlg, IDC_REALWHEEL_ENABLE, cfg_is_real_wheel_mode() ? BST_CHECKED : BST_UNCHECKED);

    set_spin_range(hDlg, IDC_WHEEL_DELTA_SPIN, 10, 500, cfg_get_number(L"wheelDelta"));
    set_spin_range(hDlg, IDC_VWHEEL_MOVE_SPIN, 10, 500, cfg_get_number(L"vWheelMove"));
    set_spin_range(hDlg, IDC_HWHEEL_MOVE_SPIN, 10, 500, cfg_get_number(L"hWheelMove"));

    CheckDlgButton(hDlg, IDC_QUICK_FIRST, cfg_is_quick_first() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_QUICK_TURN, cfg_is_quick_turn() ? BST_CHECKED : BST_UNCHECKED);

    g_initializing = FALSE;
}

static BOOL realwheel_apply(HWND hDlg) {
    int delta, vmove, hmove;
    if (!get_validated_spin(hDlg, IDC_WHEEL_DELTA_SPIN, 10, 500, L"Wheel delta", &delta)) return FALSE;
    if (!get_validated_spin(hDlg, IDC_VWHEEL_MOVE_SPIN, 10, 500, L"Vertical speed", &vmove)) return FALSE;
    if (!get_validated_spin(hDlg, IDC_HWHEEL_MOVE_SPIN, 10, 500, L"Horizontal speed", &hmove)) return FALSE;

    cfg_set_boolean(L"realWheelMode", IsDlgButtonChecked(hDlg, IDC_REALWHEEL_ENABLE) == BST_CHECKED);

    cfg_set_number(L"wheelDelta", delta);
    cfg_set_number(L"vWheelMove", vmove);
    cfg_set_number(L"hWheelMove", hmove);

    cfg_set_boolean(L"quickFirst", IsDlgButtonChecked(hDlg, IDC_QUICK_FIRST) == BST_CHECKED);
    cfg_set_boolean(L"quickTurn", IsDlgButtonChecked(hDlg, IDC_QUICK_TURN) == BST_CHECKED);

    return TRUE;
}

static void realwheel_reset(HWND hDlg) {
    g_initializing = TRUE;
    CheckDlgButton(hDlg, IDC_REALWHEEL_ENABLE, BST_UNCHECKED);
    SendDlgItemMessageW(hDlg, IDC_WHEEL_DELTA_SPIN, UDM_SETPOS32, 0, 120);
    SendDlgItemMessageW(hDlg, IDC_VWHEEL_MOVE_SPIN, UDM_SETPOS32, 0, 60);
    SendDlgItemMessageW(hDlg, IDC_HWHEEL_MOVE_SPIN, UDM_SETPOS32, 0, 60);
    CheckDlgButton(hDlg, IDC_QUICK_FIRST, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_QUICK_TURN, BST_UNCHECKED);
    g_initializing = FALSE;
    mark_changed(hDlg);
}

static INT_PTR CALLBACK realwheel_proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        g_page_hwnd[3] = hDlg;
        realwheel_init(hDlg);
        add_page_buttons(hDlg);

        return TRUE;

    case WM_SETTINGS_REFRESH:
        realwheel_init(hDlg);

        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_TAB_HELP) { show_help(hDlg, HELP_REALWHEEL); return TRUE; }
        if (LOWORD(wParam) == IDC_TAB_RESET) { realwheel_reset(hDlg); return TRUE; }
        switch (HIWORD(wParam)) {
        case BN_CLICKED: case CBN_SELCHANGE: case EN_CHANGE:
            mark_changed(hDlg);
        }
        break;

    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lParam;
        if (nm->code == PSN_APPLY) {
            if (!realwheel_apply(hDlg)) {
                SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
                return TRUE;
            }
            SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        if (nm->code == UDN_DELTAPOS)
            mark_changed(hDlg);
        break;
    }
    }
    return FALSE;
}

/* ========== Page: VH Adjuster ========== */

static void vhadj_init(HWND hDlg) {
    g_initializing = TRUE;

    CheckDlgButton(hDlg, IDC_VH_ENABLE, cfg_get_boolean(L"vhAdjusterMode") ? BST_CHECKED : BST_UNCHECKED);

    BOOL sw = cfg_is_vh_adjuster_switching();
    CheckDlgButton(hDlg, IDC_VH_FIXED, sw ? BST_UNCHECKED : BST_CHECKED);
    CheckDlgButton(hDlg, IDC_VH_SWITCHING, sw ? BST_CHECKED : BST_UNCHECKED);

    CheckDlgButton(hDlg, IDC_VH_FIRST_VERT, cfg_is_first_prefer_vertical() ? BST_CHECKED : BST_UNCHECKED);

    set_spin_range(hDlg, IDC_FIRST_MIN_SPIN, 1, 10, cfg_get_number(L"firstMinThreshold"));
    set_spin_range(hDlg, IDC_SWITCHING_SPIN, 10, 500, cfg_get_number(L"switchingThreshold"));

    g_initializing = FALSE;
}

static BOOL vhadj_apply(HWND hDlg) {
    int fmin, swthr;
    if (!get_validated_spin(hDlg, IDC_FIRST_MIN_SPIN, 1, 10, L"Min. threshold", &fmin)) return FALSE;
    if (!get_validated_spin(hDlg, IDC_SWITCHING_SPIN, 10, 500, L"Switching threshold", &swthr)) return FALSE;

    cfg_set_boolean(L"vhAdjusterMode", IsDlgButtonChecked(hDlg, IDC_VH_ENABLE) == BST_CHECKED);

    if (IsDlgButtonChecked(hDlg, IDC_VH_FIXED) == BST_CHECKED)
        cfg_set_vh_method_name(L"Fixed");
    else
        cfg_set_vh_method_name(L"Switching");

    cfg_set_boolean(L"firstPreferVertical", IsDlgButtonChecked(hDlg, IDC_VH_FIRST_VERT) == BST_CHECKED);

    cfg_set_number(L"firstMinThreshold", fmin);
    cfg_set_number(L"switchingThreshold", swthr);

    return TRUE;
}

static void vhadj_reset(HWND hDlg) {
    g_initializing = TRUE;
    CheckDlgButton(hDlg, IDC_VH_ENABLE, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_VH_FIXED, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_VH_SWITCHING, BST_CHECKED);
    CheckDlgButton(hDlg, IDC_VH_FIRST_VERT, BST_CHECKED);
    SendDlgItemMessageW(hDlg, IDC_FIRST_MIN_SPIN, UDM_SETPOS32, 0, 5);
    SendDlgItemMessageW(hDlg, IDC_SWITCHING_SPIN, UDM_SETPOS32, 0, 50);
    g_initializing = FALSE;
    mark_changed(hDlg);
}

static INT_PTR CALLBACK vhadj_proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        g_page_hwnd[4] = hDlg;
        vhadj_init(hDlg);
        add_page_buttons(hDlg);

        return TRUE;

    case WM_SETTINGS_REFRESH:
        vhadj_init(hDlg);

        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_TAB_HELP) { show_help(hDlg, HELP_VHADJ); return TRUE; }
        if (LOWORD(wParam) == IDC_TAB_RESET) { vhadj_reset(hDlg); return TRUE; }
        switch (HIWORD(wParam)) {
        case BN_CLICKED: case CBN_SELCHANGE: case EN_CHANGE:
            mark_changed(hDlg);
        }
        break;

    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lParam;
        if (nm->code == PSN_APPLY) {
            if (!vhadj_apply(hDlg)) {
                SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
                return TRUE;
            }
            SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        if (nm->code == UDN_DELTAPOS)
            mark_changed(hDlg);
        break;
    }
    }
    return FALSE;
}

/* ========== Page: Keyboard ========== */

static const wchar_t *HELP_KEYBOARD =
    L"Character repeat delay (0\u20133): How long to hold a key\r\n"
    L"  before auto-repeat starts. 0=shortest, 3=longest.\r\n\r\n"
    L"Character repeat speed (0\u201331): Auto-repeat rate.\r\n"
    L"  31=fastest (~30 char/sec), 0=slowest (~2.5 char/sec).\r\n\r\n"
    L"Enable Filter Keys: Override keyboard repeat via the\r\n"
    L"  accessibility Filter Keys feature. When off, Windows\r\n"
    L"  reverts to standard keyboard repeat behaviour.\r\n\r\n"
    L"Lock: Reapply all keyboard settings on startup if the\r\n"
    L"  system state has changed (e.g. after reboot).\r\n\r\n"
    L"Acceptance delay: Time in ms a key must be held before\r\n"
    L"  it registers. Set to 0 to accept keys immediately.\r\n\r\n"
    L"Repeat delay: Time in ms a key must be held before\r\n"
    L"  auto-repeat begins (Filter Keys).\r\n\r\n"
    L"Repeat rate: Interval in ms between repeated keystrokes.\r\n"
    L"  Lower = faster. E.g. 16 ms \x2248 60 keys/sec.\r\n\r\n"
    L"Bounce time: Time in ms to ignore duplicate presses of\r\n"
    L"  the same key after release. Set to 0 to disable.\r\n"
    L"  Cannot be used together with the other timing fields.";

static void fk_delete_last_registry_values(void) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Control Panel\\Accessibility\\Keyboard Response",
            0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, L"Last BounceKey Setting");
        RegDeleteValueW(hKey, L"Last Valid Delay");
        RegDeleteValueW(hKey, L"Last Valid Repeat");
        RegDeleteValueW(hKey, L"Last Valid Wait");
        RegCloseKey(hKey);
    }
}

static void keyboard_init(HWND hDlg) {
    g_initializing = TRUE;

    /* Read current keyboard repeat settings */
    int kb_delay = 1;
    SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &kb_delay, 0);
    set_spin_range(hDlg, IDC_KB_DELAY_SPIN, 0, 3, kb_delay);

    DWORD kb_speed = 31;
    SystemParametersInfoW(SPI_GETKEYBOARDSPEED, 0, &kb_speed, 0);
    set_spin_range(hDlg, IDC_KB_SPEED_SPIN, 0, 31, (int)kb_speed);

    /* Read current filter keys state */
    FILTERKEYS fk = {0};
    fk.cbSize = sizeof(FILTERKEYS);
    SystemParametersInfoW(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &fk, 0);

    BOOL enabled = (fk.dwFlags & FKF_FILTERKEYSON) != 0;
    CheckDlgButton(hDlg, IDC_FK_ENABLE, enabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_FK_LOCK, cfg_get_boolean(L"fkLock") ? BST_CHECKED : BST_UNCHECKED);

    if (enabled) {
        set_spin_range(hDlg, IDC_FK_ACCEPT_SPIN, 0, 10000, (int)fk.iWaitMSec);
        set_spin_range(hDlg, IDC_FK_DELAY_SPIN, 0, 10000, (int)fk.iDelayMSec);
        set_spin_range(hDlg, IDC_FK_REPEAT_SPIN, 0, 10000, (int)fk.iRepeatMSec);
        set_spin_range(hDlg, IDC_FK_BOUNCE_SPIN, 0, 10000, (int)fk.iBounceMSec);
    } else {
        set_spin_range(hDlg, IDC_FK_ACCEPT_SPIN, 0, 10000, cfg_get_number(L"fkAcceptanceDelay"));
        set_spin_range(hDlg, IDC_FK_DELAY_SPIN, 0, 10000, cfg_get_number(L"fkRepeatDelay"));
        set_spin_range(hDlg, IDC_FK_REPEAT_SPIN, 0, 10000, cfg_get_number(L"fkRepeatRate"));
        set_spin_range(hDlg, IDC_FK_BOUNCE_SPIN, 0, 10000, cfg_get_number(L"fkBounceTime"));
    }

    g_initializing = FALSE;
}

static BOOL keyboard_apply(HWND hDlg) {
    /* Keyboard repeat */
    int kb_delay, kb_speed;
    if (!get_validated_spin(hDlg, IDC_KB_DELAY_SPIN, 0, 3, L"Repeat delay", &kb_delay)) return FALSE;
    if (!get_validated_spin(hDlg, IDC_KB_SPEED_SPIN, 0, 31, L"Repeat speed", &kb_speed)) return FALSE;

    cfg_set_number(L"kbRepeatDelay", kb_delay);
    cfg_set_number(L"kbRepeatSpeed", kb_speed);

    SystemParametersInfoW(SPI_SETKEYBOARDDELAY, (UINT)kb_delay, NULL,
                          SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    SystemParametersInfoW(SPI_SETKEYBOARDSPEED, (UINT)kb_speed, NULL,
                          SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

    /* Filter Keys */
    int accept, delay, repeat, bounce;
    if (!get_validated_spin(hDlg, IDC_FK_ACCEPT_SPIN, 0, 10000, L"Acceptance delay", &accept)) return FALSE;
    if (!get_validated_spin(hDlg, IDC_FK_DELAY_SPIN, 0, 10000, L"Repeat delay", &delay)) return FALSE;
    if (!get_validated_spin(hDlg, IDC_FK_REPEAT_SPIN, 0, 10000, L"Repeat rate", &repeat)) return FALSE;
    if (!get_validated_spin(hDlg, IDC_FK_BOUNCE_SPIN, 0, 10000, L"Bounce time", &bounce)) return FALSE;

    BOOL enabled = IsDlgButtonChecked(hDlg, IDC_FK_ENABLE) == BST_CHECKED;
    cfg_set_boolean(L"filterKeys", enabled);
    cfg_set_boolean(L"fkLock", IsDlgButtonChecked(hDlg, IDC_FK_LOCK) == BST_CHECKED);
    cfg_set_number(L"fkAcceptanceDelay", accept);
    cfg_set_number(L"fkRepeatDelay", delay);
    cfg_set_number(L"fkRepeatRate", repeat);
    cfg_set_number(L"fkBounceTime", bounce);

    FILTERKEYS fk = {0};
    fk.cbSize = sizeof(FILTERKEYS);

    if (enabled) {
        fk.dwFlags = 59; /* FKF_FILTERKEYSON | AVAILABLE | CONFIRMHOTKEY | HOTKEYSOUND | INDICATOR */
        fk.iWaitMSec = (DWORD)accept;
        fk.iDelayMSec = (DWORD)delay;
        fk.iRepeatMSec = (DWORD)repeat;
        fk.iBounceMSec = (DWORD)bounce;
    } else {
        fk.dwFlags = 126; /* AVAILABLE | HOTKEYACTIVE | CONFIRMHOTKEY | HOTKEYSOUND | INDICATOR | CLICKON */
        fk.iWaitMSec = 1000;
        fk.iDelayMSec = 1000;
        fk.iRepeatMSec = 500;
        fk.iBounceMSec = 0;
    }
    SystemParametersInfoW(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &fk,
                          SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

    if (!enabled)
        fk_delete_last_registry_values();

    return TRUE;
}

static void keyboard_reset(HWND hDlg) {
    g_initializing = TRUE;
    SendDlgItemMessageW(hDlg, IDC_KB_DELAY_SPIN, UDM_SETPOS32, 0, 1);
    SendDlgItemMessageW(hDlg, IDC_KB_SPEED_SPIN, UDM_SETPOS32, 0, 31);
    CheckDlgButton(hDlg, IDC_FK_ENABLE, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_FK_LOCK, BST_UNCHECKED);
    SendDlgItemMessageW(hDlg, IDC_FK_ACCEPT_SPIN, UDM_SETPOS32, 0, 1000);
    SendDlgItemMessageW(hDlg, IDC_FK_DELAY_SPIN, UDM_SETPOS32, 0, 1000);
    SendDlgItemMessageW(hDlg, IDC_FK_REPEAT_SPIN, UDM_SETPOS32, 0, 500);
    SendDlgItemMessageW(hDlg, IDC_FK_BOUNCE_SPIN, UDM_SETPOS32, 0, 0);
    g_initializing = FALSE;
    mark_changed(hDlg);
}

static INT_PTR CALLBACK keyboard_proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        g_page_hwnd[5] = hDlg;
        keyboard_init(hDlg);
        add_page_buttons(hDlg);

        return TRUE;

    case WM_SETTINGS_REFRESH:
        keyboard_init(hDlg);

        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_TAB_HELP) { show_help(hDlg, HELP_KEYBOARD); return TRUE; }
        if (LOWORD(wParam) == IDC_TAB_RESET) { keyboard_reset(hDlg); return TRUE; }
        switch (HIWORD(wParam)) {
        case BN_CLICKED: case CBN_SELCHANGE: case EN_CHANGE:
            mark_changed(hDlg);
        }
        break;

    case WM_NOTIFY: {
        NMHDR *nm = (NMHDR *)lParam;
        if (nm->code == PSN_APPLY) {
            if (!keyboard_apply(hDlg)) {
                SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
                return TRUE;
            }
            SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        if (nm->code == UDN_DELTAPOS)
            mark_changed(hDlg);
        break;
    }
    }
    return FALSE;
}

void settings_apply_filter_keys(void) {
    BOOL lock = cfg_get_boolean(L"fkLock");

    /* Keyboard repeat — only enforce if lock is on */
    if (lock) {
        int cfg_kb_delay = cfg_get_number(L"kbRepeatDelay");
        int cfg_kb_speed = cfg_get_number(L"kbRepeatSpeed");
        int sys_kb_delay = 1;
        DWORD sys_kb_speed = 31;
        SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &sys_kb_delay, 0);
        SystemParametersInfoW(SPI_GETKEYBOARDSPEED, 0, &sys_kb_speed, 0);

        if (sys_kb_delay != cfg_kb_delay)
            SystemParametersInfoW(SPI_SETKEYBOARDDELAY, (UINT)cfg_kb_delay, NULL,
                                  SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
        if ((int)sys_kb_speed != cfg_kb_speed)
            SystemParametersInfoW(SPI_SETKEYBOARDSPEED, (UINT)cfg_kb_speed, NULL,
                                  SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    }

    /* Filter Keys */
    FILTERKEYS fk = {0};
    fk.cbSize = sizeof(FILTERKEYS);
    SystemParametersInfoW(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &fk, 0);

    BOOL sys_enabled = (fk.dwFlags & FKF_FILTERKEYSON) != 0;
    BOOL cfg_enabled = cfg_get_boolean(L"filterKeys");

    if (sys_enabled) {
        if (lock) {
            /* System enabled, lock on → enforce config values */
            int cfg_accept = cfg_get_number(L"fkAcceptanceDelay");
            int cfg_delay = cfg_get_number(L"fkRepeatDelay");
            int cfg_repeat = cfg_get_number(L"fkRepeatRate");
            int cfg_bounce = cfg_get_number(L"fkBounceTime");
            if ((int)fk.iWaitMSec != cfg_accept ||
                (int)fk.iDelayMSec != cfg_delay ||
                (int)fk.iRepeatMSec != cfg_repeat ||
                (int)fk.iBounceMSec != cfg_bounce) {
                FILTERKEYS nfk = {0};
                nfk.cbSize = sizeof(FILTERKEYS);
                nfk.dwFlags = 59;
                nfk.iWaitMSec = (DWORD)cfg_accept;
                nfk.iDelayMSec = (DWORD)cfg_delay;
                nfk.iRepeatMSec = (DWORD)cfg_repeat;
                nfk.iBounceMSec = (DWORD)cfg_bounce;
                SystemParametersInfoW(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &nfk,
                                      SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
            }
        } else {
            /* System enabled, lock off → sync system values to config */
            cfg_set_boolean(L"filterKeys", TRUE);
            cfg_set_number(L"fkAcceptanceDelay", (int)fk.iWaitMSec);
            cfg_set_number(L"fkRepeatDelay", (int)fk.iDelayMSec);
            cfg_set_number(L"fkRepeatRate", (int)fk.iRepeatMSec);
            cfg_set_number(L"fkBounceTime", (int)fk.iBounceMSec);
        }
    } else if (cfg_enabled) {
        if (lock) {
            /* System disabled, config enabled, lock on → enable with config values */
            int cfg_accept = cfg_get_number(L"fkAcceptanceDelay");
            int cfg_delay = cfg_get_number(L"fkRepeatDelay");
            int cfg_repeat = cfg_get_number(L"fkRepeatRate");
            int cfg_bounce = cfg_get_number(L"fkBounceTime");
            FILTERKEYS nfk = {0};
            nfk.cbSize = sizeof(FILTERKEYS);
            nfk.dwFlags = 59;
            nfk.iWaitMSec = (DWORD)cfg_accept;
            nfk.iDelayMSec = (DWORD)cfg_delay;
            nfk.iRepeatMSec = (DWORD)cfg_repeat;
            nfk.iBounceMSec = (DWORD)cfg_bounce;
            SystemParametersInfoW(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &nfk,
                                  SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
        } else {
            /* System disabled, config enabled, lock off → enable with registry values */
            FILTERKEYS nfk = {0};
            nfk.cbSize = sizeof(FILTERKEYS);
            nfk.dwFlags = 59;
            nfk.iWaitMSec = fk.iWaitMSec;
            nfk.iDelayMSec = fk.iDelayMSec;
            nfk.iRepeatMSec = fk.iRepeatMSec;
            nfk.iBounceMSec = fk.iBounceMSec;
            SystemParametersInfoW(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &nfk,
                                  SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
            cfg_set_number(L"fkAcceptanceDelay", (int)fk.iWaitMSec);
            cfg_set_number(L"fkRepeatDelay", (int)fk.iDelayMSec);
            cfg_set_number(L"fkRepeatRate", (int)fk.iRepeatMSec);
            cfg_set_number(L"fkBounceTime", (int)fk.iBounceMSec);
        }
    }
    /* System disabled, config disabled → do nothing */
}

/* ========== Page: Profiles ========== */

static BOOL is_valid_profile_name(const wchar_t *name) {
    if (name[0] == L'-' && name[1] == L'-') return FALSE;
    if (wcschr(name, L'\\')) return FALSE;
    if (wcschr(name, L'/')) return FALSE;
    if (wcsstr(name, L"..")) return FALSE;
    return TRUE;
}

static void profiles_refresh(HWND hDlg) {
    HWND hList = GetDlgItem(hDlg, IDC_PROFILE_LIST);
    SendMessageW(hList, LB_RESETCONTENT, 0, 0);

    const wchar_t *sel = cfg_get_selected_properties();

    SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)L"Default");
    int sel_idx = 0;

    wchar_t names[50][256];
    int count = cfg_get_prop_files(names, 50);
    for (int i = 0; i < count; i++) {
        int idx = (int)SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)names[i]);
        if (wcscmp(sel, names[i]) == 0) sel_idx = idx;
    }
    if (wcscmp(sel, L"Default") == 0) sel_idx = 0;

    SendMessageW(hList, LB_SETCURSEL, sel_idx, 0);
}

static void profile_switch(HWND hDlg) {
    HWND hList = GetDlgItem(hDlg, IDC_PROFILE_LIST);
    int sel = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
    if (sel < 0) return;

    wchar_t name[256];
    SendMessageW(hList, LB_GETTEXT, sel, (LPARAM)name);
    cfg_set_selected_properties(name);
    cfg_reload_properties();
    tray_update_health_timer();
    refresh_all_pages();
}

static INT_PTR CALLBACK profiles_proc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        g_page_hwnd[6] = hDlg;
        profiles_refresh(hDlg);

        return TRUE;

    case WM_SETTINGS_REFRESH:
        profiles_refresh(hDlg);

        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_PROFILE_LIST:
            if (HIWORD(wParam) == LBN_DBLCLK)
                profile_switch(hDlg);
            return TRUE;

        case IDC_PROFILE_RELOAD:
            cfg_reload_properties();
            tray_update_health_timer();
            refresh_all_pages();
            return TRUE;

        case IDC_PROFILE_SAVE:
            cfg_store_properties();
            return TRUE;

        case IDC_PROFILE_OPENDIR:
            ShellExecuteW(NULL, L"open", cfg_get_user_dir(), NULL, NULL, SW_SHOW);
            return TRUE;

        case IDC_PROFILE_ADD: {
            wchar_t name[256];
            if (dialog_text_input(L"Properties Name", L"Add Properties", name, 256)) {
                if (!is_valid_profile_name(name)) {
                    dialog_error(L"Invalid Name", L"Name Error");
                } else {
                    cfg_properties_copy(cfg_get_selected_properties(), name);
                    cfg_set_selected_properties(name);
                    cfg_reload_properties();
                    tray_update_health_timer();
                    refresh_all_pages();
                }
            }
            return TRUE;
        }

        case IDC_PROFILE_DELETE: {
            const wchar_t *sel = cfg_get_selected_properties();
            if (wcscmp(sel, L"Default") != 0) {
                cfg_properties_delete(sel);
                cfg_set_selected_properties(L"Default");
                cfg_reload_properties();
                tray_update_health_timer();
                refresh_all_pages();
            }
            return TRUE;
        }
        }
        break;

    case WM_NOTIFY:
        if (((NMHDR *)lParam)->code == PSN_APPLY) {
            cfg_store_properties();
            SetWindowLongPtrW(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/* ========== PropertySheet ========== */

static BOOL g_settings_open = FALSE;

void settings_show(void) {
    if (g_settings_open) return;
    g_settings_open = TRUE;

    HINSTANCE hinst = GetModuleHandleW(NULL);
    memset(g_page_hwnd, 0, sizeof(g_page_hwnd));
    g_initializing = FALSE;

    struct {
        int dlg_id;
        const wchar_t *title;
        DLGPROC proc;
    } pages[] = {
        { IDD_PAGE_GENERAL,    L"General",      general_proc },
        { IDD_PAGE_SCROLL,     L"Scroll",       scroll_proc },
        { IDD_PAGE_ACCEL,      L"Acceleration", accel_proc },
        { IDD_PAGE_REALWHEEL,  L"Real Wheel",   realwheel_proc },
        { IDD_PAGE_VHADJ,      L"VH Adjuster",  vhadj_proc },
        { IDD_PAGE_KEYBOARD,   L"Keyboard",     keyboard_proc },
        { IDD_PAGE_PROFILES,   L"Profiles",     profiles_proc },
    };

    /* Translate tab titles */
    const wchar_t *titles[7];
    for (int i = 0; i < 7; i++)
        titles[i] = pages[i].title;

    PROPSHEETPAGEW psp[7];
    memset(psp, 0, sizeof(psp));
    for (int i = 0; i < 7; i++) {
        psp[i].dwSize = sizeof(PROPSHEETPAGEW);
        psp[i].dwFlags = PSP_USETITLE;
        psp[i].hInstance = hinst;
        psp[i].pszTemplate = MAKEINTRESOURCEW(pages[i].dlg_id);
        psp[i].pszTitle = titles[i];
        psp[i].pfnDlgProc = pages[i].proc;
    }

    PROPSHEETHEADERW psh;
    memset(&psh, 0, sizeof(psh));
    psh.dwSize = sizeof(PROPSHEETHEADERW);
    psh.dwFlags = PSH_PROPSHEETPAGE | PSH_USEHICON;
    psh.hwndParent = NULL;
    psh.hIcon = LoadIconW(hinst, MAKEINTRESOURCEW(IDI_APP));
    psh.hInstance = hinst;
    psh.pszCaption = L"tpkb Settings";
    psh.nPages = 7;
    psh.ppsp = psp;

    PropertySheetW(&psh);

    memset(g_page_hwnd, 0, sizeof(g_page_hwnd));
    g_settings_open = FALSE;
}
