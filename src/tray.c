/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#include "tray.h"
#include "types.h"
#include "config.h"
#include "hook.h"
#include "dialog.h"
#include "locale.h"
#include <shellapi.h>
#include <shlobj.h>

/* ========== Menu command IDs ========== */

enum {
    /* Trigger submenu */
    IDM_TRIGGER_LR = 1001,
    IDM_TRIGGER_LEFT,
    IDM_TRIGGER_RIGHT,
    IDM_TRIGGER_MIDDLE,
    IDM_TRIGGER_X1,
    IDM_TRIGGER_X2,
    IDM_TRIGGER_LEFTDRAG,
    IDM_TRIGGER_RIGHTDRAG,
    IDM_TRIGGER_MIDDLEDRAG,
    IDM_TRIGGER_X1DRAG,
    IDM_TRIGGER_X2DRAG,
    IDM_TRIGGER_NONE,
    IDM_SEND_MIDDLE_CLICK,
    IDM_DRAGGED_LOCK,

    /* Keyboard submenu */
    IDM_KEYBOARD_TOGGLE = 1100,
    IDM_VK_FIRST,
    IDM_VK_LAST = IDM_VK_FIRST + VK_ENTRY_COUNT - 1,

    /* Accel table submenu */
    IDM_ACCEL_TOGGLE = 1200,
    IDM_ACCEL_M5,
    IDM_ACCEL_M6,
    IDM_ACCEL_M7,
    IDM_ACCEL_M8,
    IDM_ACCEL_M9,
    IDM_ACCEL_CUSTOM,

    /* Priority submenu */
    IDM_PRIO_HIGH = 1300,
    IDM_PRIO_ABOVE_NORMAL,
    IDM_PRIO_NORMAL,

    /* Number settings */
    IDM_NUM_POLL_TIMEOUT = 1400,
    IDM_NUM_SCROLL_LOCKTIME,
    IDM_NUM_VERT_THRESHOLD,
    IDM_NUM_HORIZ_THRESHOLD,
    IDM_NUM_DRAG_THRESHOLD,

    /* Real wheel submenu */
    IDM_REAL_WHEEL_TOGGLE = 1500,
    IDM_NUM_WHEEL_DELTA,
    IDM_NUM_V_WHEEL_MOVE,
    IDM_NUM_H_WHEEL_MOVE,
    IDM_QUICK_FIRST,
    IDM_QUICK_TURN,

    /* VH adjuster submenu */
    IDM_VH_TOGGLE = 1600,
    IDM_VH_FIXED,
    IDM_VH_SWITCHING,
    IDM_VH_FIRST_PREFER_VERT,
    IDM_NUM_FIRST_MIN_THR,
    IDM_NUM_SWITCHING_THR,

    /* Properties submenu */
    IDM_PROP_RELOAD = 1700,
    IDM_PROP_SAVE,
    IDM_PROP_OPEN_DIR,
    IDM_PROP_ADD,
    IDM_PROP_DELETE,
    IDM_PROP_DEFAULT,
    IDM_PROP_FIRST = 1750, /* Dynamic profile entries */
    IDM_PROP_LAST = 1799,

    /* Boolean toggles */
    IDM_CURSOR_CHANGE = 1800,
    IDM_HORIZONTAL_SCROLL,
    IDM_REVERSE_SCROLL,
    IDM_SWAP_SCROLL,
    IDM_PASS_MODE,

    /* Language submenu */
    IDM_LANG_EN = 1900,
    IDM_LANG_JA,

    /* Info/exit */
    IDM_INFO = 1950,
    IDM_EXIT = 1999,
};

static NOTIFYICONDATAW g_nid;
static HWND g_hwnd;
static HICON g_icon_run, g_icon_stop;
static wchar_t g_prop_names[50][256];
static int g_prop_count;

static const wchar_t *cl(const wchar_t *msg) {
    return cfg_conv_lang(msg);
}

/* ========== Menu construction ========== */

static void add_trigger_menu(HMENU parent) {
    HMENU sub = CreatePopupMenu();

    struct { int id; const wchar_t *label; Trigger t; } items[] = {
        { IDM_TRIGGER_LR, L"LR (Left <<-->> Right)", TRIGGER_LR },
        { IDM_TRIGGER_LEFT, L"Left (Left -->> Right)", TRIGGER_LEFT },
        { IDM_TRIGGER_RIGHT, L"Right (Right -->> Left)", TRIGGER_RIGHT },
        { IDM_TRIGGER_MIDDLE, L"Middle", TRIGGER_MIDDLE },
        { IDM_TRIGGER_X1, L"X1", TRIGGER_X1 },
        { IDM_TRIGGER_X2, L"X2", TRIGGER_X2 },
        { IDM_TRIGGER_LEFTDRAG, L"LeftDrag", TRIGGER_LEFT_DRAG },
        { IDM_TRIGGER_RIGHTDRAG, L"RightDrag", TRIGGER_RIGHT_DRAG },
        { IDM_TRIGGER_MIDDLEDRAG, L"MiddleDrag", TRIGGER_MIDDLE_DRAG },
        { IDM_TRIGGER_X1DRAG, L"X1Drag", TRIGGER_X1_DRAG },
        { IDM_TRIGGER_X2DRAG, L"X2Drag", TRIGGER_X2_DRAG },
        { IDM_TRIGGER_NONE, L"None", TRIGGER_NONE },
    };
    Trigger cur = cfg_get_trigger();
    for (int i = 0; i < 12; i++) {
        UINT flags = MF_STRING;
        if (items[i].t == cur) flags |= MF_CHECKED;
        AppendMenuW(sub, flags, items[i].id, cl(items[i].label));
    }
    AppendMenuW(sub, MF_SEPARATOR, 0, NULL);
    AppendMenuW(sub, cfg_is_send_middle_click() ? MF_CHECKED : MF_UNCHECKED,
                IDM_SEND_MIDDLE_CLICK, cl(L"Send MiddleClick"));
    AppendMenuW(sub, cfg_is_dragged_lock() ? MF_CHECKED : MF_UNCHECKED,
                IDM_DRAGGED_LOCK, cl(L"Dragged Lock"));

    AppendMenuW(parent, MF_POPUP, (UINT_PTR)sub, cl(L"Trigger"));
}

static void add_keyboard_menu(HMENU parent) {
    HMENU sub = CreatePopupMenu();
    AppendMenuW(sub, cfg_is_keyboard_hook() ? MF_CHECKED : MF_UNCHECKED,
                IDM_KEYBOARD_TOGGLE, cl(L"ON / OFF"));
    AppendMenuW(sub, MF_SEPARATOR, 0, NULL);

    int cur_vk = cfg_get_target_vk_code();
    for (int i = 0; i < VK_ENTRY_COUNT; i++) {
        UINT flags = MF_STRING;
        if (VK_TABLE[i].code == cur_vk) flags |= MF_CHECKED;
        /* Build display name */
        wchar_t label[128];
        if (VK_TABLE[i].code == 0x1C)
            _snwprintf(label, 128, L"%s", cl(L"VK_CONVERT (Henkan)"));
        else if (VK_TABLE[i].code == 0x1D)
            _snwprintf(label, 128, L"%s", cl(L"VK_NONCONVERT (Muhenkan)"));
        else if (VK_TABLE[i].code == 0x5B)
            _snwprintf(label, 128, L"%s", cl(L"VK_LWIN (Left Windows)"));
        else if (VK_TABLE[i].code == 0x5C)
            _snwprintf(label, 128, L"%s", cl(L"VK_RWIN (Right Windows)"));
        else if (VK_TABLE[i].code == 0xA0)
            _snwprintf(label, 128, L"%s", cl(L"VK_LSHIFT (Left Shift)"));
        else if (VK_TABLE[i].code == 0xA1)
            _snwprintf(label, 128, L"%s", cl(L"VK_RSHIFT (Right Shift)"));
        else if (VK_TABLE[i].code == 0xA2)
            _snwprintf(label, 128, L"%s", cl(L"VK_LCONTROL (Left Ctrl)"));
        else if (VK_TABLE[i].code == 0xA3)
            _snwprintf(label, 128, L"%s", cl(L"VK_RCONTROL (Right Ctrl)"));
        else if (VK_TABLE[i].code == 0xA4)
            _snwprintf(label, 128, L"%s", cl(L"VK_LMENU (Left Alt)"));
        else if (VK_TABLE[i].code == 0xA5)
            _snwprintf(label, 128, L"%s", cl(L"VK_RMENU (Right Alt)"));
        else
            _snwprintf(label, 128, L"%s", VK_TABLE[i].name);

        AppendMenuW(sub, flags, IDM_VK_FIRST + i, label);
    }
    AppendMenuW(parent, MF_POPUP, (UINT_PTR)sub, cl(L"Keyboard"));
}

static void add_accel_menu(HMENU parent) {
    HMENU sub = CreatePopupMenu();
    AppendMenuW(sub, cfg_is_accel_table() ? MF_CHECKED : MF_UNCHECKED,
                IDM_ACCEL_TOGGLE, cl(L"ON / OFF"));
    AppendMenuW(sub, MF_SEPARATOR, 0, NULL);

    AccelPreset cur = cfg_get_accel_preset();
    struct { int id; AccelPreset p; const wchar_t *name; } items[] = {
        { IDM_ACCEL_M5, ACCEL_PRESET_M5, L"M5" },
        { IDM_ACCEL_M6, ACCEL_PRESET_M6, L"M6" },
        { IDM_ACCEL_M7, ACCEL_PRESET_M7, L"M7" },
        { IDM_ACCEL_M8, ACCEL_PRESET_M8, L"M8" },
        { IDM_ACCEL_M9, ACCEL_PRESET_M9, L"M9" },
    };
    for (int i = 0; i < 5; i++) {
        UINT flags = MF_STRING;
        if (!cfg_is_custom_accel() && items[i].p == cur) flags |= MF_CHECKED;
        AppendMenuW(sub, flags, items[i].id, items[i].name);
    }
    AppendMenuW(sub, MF_SEPARATOR, 0, NULL);
    AppendMenuW(sub, cfg_is_custom_accel() ? MF_CHECKED : MF_UNCHECKED,
                IDM_ACCEL_CUSTOM, cl(L"Custom Table"));

    AppendMenuW(parent, MF_POPUP, (UINT_PTR)sub, cl(L"Accel Table"));
}

static void add_number_item(HMENU menu, int id, const wchar_t *name) {
    wchar_t label[128];
    _snwprintf(label, 128, L"%s (%d)", cl(name), cfg_get_number(name));
    AppendMenuW(menu, MF_STRING, id, label);
}

static void add_priority_menu(HMENU parent) {
    HMENU sub = CreatePopupMenu();
    Priority cur = cfg_get_priority();
    struct { int id; Priority p; const wchar_t *name; } items[] = {
        { IDM_PRIO_HIGH, PRIO_HIGH, L"High" },
        { IDM_PRIO_ABOVE_NORMAL, PRIO_ABOVE_NORMAL, L"Above Normal" },
        { IDM_PRIO_NORMAL, PRIO_NORMAL, L"Normal" },
    };
    for (int i = 0; i < 3; i++) {
        UINT flags = MF_STRING;
        if (items[i].p == cur) flags |= MF_CHECKED;
        AppendMenuW(sub, flags, items[i].id, cl(items[i].name));
    }
    AppendMenuW(parent, MF_POPUP, (UINT_PTR)sub, cl(L"Priority"));
}

static void add_real_wheel_menu(HMENU parent) {
    HMENU sub = CreatePopupMenu();
    AppendMenuW(sub, cfg_is_real_wheel_mode() ? MF_CHECKED : MF_UNCHECKED,
                IDM_REAL_WHEEL_TOGGLE, cl(L"ON / OFF"));
    AppendMenuW(sub, MF_SEPARATOR, 0, NULL);
    add_number_item(sub, IDM_NUM_WHEEL_DELTA, L"wheelDelta");
    add_number_item(sub, IDM_NUM_V_WHEEL_MOVE, L"vWheelMove");
    add_number_item(sub, IDM_NUM_H_WHEEL_MOVE, L"hWheelMove");
    AppendMenuW(sub, MF_SEPARATOR, 0, NULL);
    AppendMenuW(sub, cfg_is_quick_first() ? MF_CHECKED : MF_UNCHECKED,
                IDM_QUICK_FIRST, cl(L"quickFirst"));
    AppendMenuW(sub, cfg_is_quick_turn() ? MF_CHECKED : MF_UNCHECKED,
                IDM_QUICK_TURN, cl(L"quickTurn"));
    AppendMenuW(parent, MF_POPUP, (UINT_PTR)sub, cl(L"Real Wheel Mode"));
}

static void add_vh_menu(HMENU parent) {
    HMENU sub = CreatePopupMenu();
    AppendMenuW(sub, cfg_is_vh_adjuster_mode() ? MF_CHECKED : MF_UNCHECKED,
                IDM_VH_TOGGLE, cl(L"ON / OFF"));
    AppendMenuW(sub, MF_SEPARATOR, 0, NULL);
    BOOL sw = cfg_is_vh_adjuster_switching();
    AppendMenuW(sub, sw ? MF_UNCHECKED : MF_CHECKED, IDM_VH_FIXED, cl(L"Fixed"));
    AppendMenuW(sub, sw ? MF_CHECKED : MF_UNCHECKED, IDM_VH_SWITCHING, cl(L"Switching"));
    AppendMenuW(sub, MF_SEPARATOR, 0, NULL);
    AppendMenuW(sub, cfg_is_first_prefer_vertical() ? MF_CHECKED : MF_UNCHECKED,
                IDM_VH_FIRST_PREFER_VERT, cl(L"firstPreferVertical"));
    add_number_item(sub, IDM_NUM_FIRST_MIN_THR, L"firstMinThreshold");
    add_number_item(sub, IDM_NUM_SWITCHING_THR, L"switchingThreshold");
    AppendMenuW(parent, MF_POPUP, (UINT_PTR)sub, cl(L"VH Adjuster"));
}

static void add_props_menu(HMENU parent) {
    HMENU sub = CreatePopupMenu();
    AppendMenuW(sub, MF_STRING, IDM_PROP_RELOAD, cl(L"Reload"));
    AppendMenuW(sub, MF_STRING, IDM_PROP_SAVE, cl(L"Save"));
    AppendMenuW(sub, MF_STRING, IDM_PROP_OPEN_DIR, cl(L"Open Dir"));
    AppendMenuW(sub, MF_SEPARATOR, 0, NULL);
    AppendMenuW(sub, MF_STRING, IDM_PROP_ADD, cl(L"Add"));
    AppendMenuW(sub, MF_STRING, IDM_PROP_DELETE, cl(L"Delete"));
    AppendMenuW(sub, MF_SEPARATOR, 0, NULL);

    /* Profile list */
    const wchar_t *sel = cfg_get_selected_properties();
    AppendMenuW(sub, wcscmp(sel, L"Default") == 0 ? MF_CHECKED : MF_UNCHECKED,
                IDM_PROP_DEFAULT, cl(L"Default"));

    g_prop_count = cfg_get_prop_files(g_prop_names, 50);
    for (int i = 0; i < g_prop_count && i < (IDM_PROP_LAST - IDM_PROP_FIRST); i++) {
        UINT flags = MF_STRING;
        if (wcscmp(sel, g_prop_names[i]) == 0) flags |= MF_CHECKED;
        AppendMenuW(sub, flags, IDM_PROP_FIRST + i, g_prop_names[i]);
    }
    AppendMenuW(parent, MF_POPUP, (UINT_PTR)sub, cl(L"Properties"));
}

static void add_lang_menu(HMENU parent) {
    HMENU sub = CreatePopupMenu();
    const wchar_t *cur = cfg_get_language();
    AppendMenuW(sub, wcscmp(cur, L"en") == 0 ? MF_CHECKED : MF_UNCHECKED,
                IDM_LANG_EN, cl(L"English"));
    AppendMenuW(sub, wcscmp(cur, L"ja") == 0 ? MF_CHECKED : MF_UNCHECKED,
                IDM_LANG_JA, cl(L"Japanese"));
    AppendMenuW(parent, MF_POPUP, (UINT_PTR)sub, cl(L"Language"));
}

static HMENU build_context_menu(void) {
    HMENU menu = CreatePopupMenu();

    add_trigger_menu(menu);
    add_keyboard_menu(menu);
    add_accel_menu(menu);
    add_priority_menu(menu);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

    /* Number settings */
    HMENU nums = CreatePopupMenu();
    add_number_item(nums, IDM_NUM_POLL_TIMEOUT, L"pollTimeout");
    add_number_item(nums, IDM_NUM_SCROLL_LOCKTIME, L"scrollLocktime");
    add_number_item(nums, IDM_NUM_VERT_THRESHOLD, L"verticalThreshold");
    add_number_item(nums, IDM_NUM_HORIZ_THRESHOLD, L"horizontalThreshold");
    add_number_item(nums, IDM_NUM_DRAG_THRESHOLD, L"dragThreshold");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)nums, cl(L"Set Number"));

    add_real_wheel_menu(menu);
    add_vh_menu(menu);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

    add_props_menu(menu);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

    /* Boolean toggles */
    AppendMenuW(menu, cfg_is_cursor_change() ? MF_CHECKED : MF_UNCHECKED,
                IDM_CURSOR_CHANGE, cl(L"Cursor Change"));
    AppendMenuW(menu, cfg_is_horizontal_scroll() ? MF_CHECKED : MF_UNCHECKED,
                IDM_HORIZONTAL_SCROLL, cl(L"Horizontal Scroll"));
    AppendMenuW(menu, cfg_is_reverse_scroll() ? MF_CHECKED : MF_UNCHECKED,
                IDM_REVERSE_SCROLL, cl(L"Reverse Scroll (Flip)"));
    AppendMenuW(menu, cfg_is_swap_scroll() ? MF_CHECKED : MF_UNCHECKED,
                IDM_SWAP_SCROLL, cl(L"Swap Scroll (V.H)"));
    AppendMenuW(menu, cfg_is_pass_mode() ? MF_CHECKED : MF_UNCHECKED,
                IDM_PASS_MODE, cl(L"Pass Mode"));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

    add_lang_menu(menu);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

    /* Info */
    wchar_t info_label[128];
    _snwprintf(info_label, 128, L"%s: %s %s", cl(L"Name"), PROGRAM_NAME, PROGRAM_VERSION);
    AppendMenuW(menu, MF_STRING | MF_GRAYED, IDM_INFO, info_label);
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(menu, MF_STRING, IDM_EXIT, cl(L"Exit"));

    return menu;
}

/* ========== Number setting handler ========== */

typedef struct { int id; const wchar_t *name; int low; int up; } NumSetting;

static const NumSetting NUM_SETTINGS[] = {
    { IDM_NUM_POLL_TIMEOUT, L"pollTimeout", 50, 500 },
    { IDM_NUM_SCROLL_LOCKTIME, L"scrollLocktime", 150, 500 },
    { IDM_NUM_VERT_THRESHOLD, L"verticalThreshold", 0, 500 },
    { IDM_NUM_HORIZ_THRESHOLD, L"horizontalThreshold", 0, 500 },
    { IDM_NUM_DRAG_THRESHOLD, L"dragThreshold", 0, 500 },
    { IDM_NUM_WHEEL_DELTA, L"wheelDelta", 10, 500 },
    { IDM_NUM_V_WHEEL_MOVE, L"vWheelMove", 10, 500 },
    { IDM_NUM_H_WHEEL_MOVE, L"hWheelMove", 10, 500 },
    { IDM_NUM_FIRST_MIN_THR, L"firstMinThreshold", 1, 10 },
    { IDM_NUM_SWITCHING_THR, L"switchingThreshold", 10, 500 },
};
#define NUM_SETTINGS_COUNT (sizeof(NUM_SETTINGS) / sizeof(NUM_SETTINGS[0]))

static void handle_number_setting(int cmd_id) {
    for (int i = 0; i < (int)NUM_SETTINGS_COUNT; i++) {
        if (NUM_SETTINGS[i].id == cmd_id) {
            int result;
            if (dialog_number_input(cl(NUM_SETTINGS[i].name), cl(L"Set Number"),
                                    NUM_SETTINGS[i].low, NUM_SETTINGS[i].up,
                                    cfg_get_number(NUM_SETTINGS[i].name), &result)) {
                cfg_set_number(NUM_SETTINGS[i].name, result);
            }
            return;
        }
    }
}

/* ========== Command handler ========== */

static void handle_command(int cmd_id) {
    /* Trigger */
    if (cmd_id >= IDM_TRIGGER_LR && cmd_id <= IDM_TRIGGER_NONE) {
        Trigger t = (Trigger)(cmd_id - IDM_TRIGGER_LR);
        cfg_set_trigger(t);
        return;
    }

    /* VK code */
    if (cmd_id >= IDM_VK_FIRST && cmd_id <= IDM_VK_LAST) {
        int idx = cmd_id - IDM_VK_FIRST;
        cfg_set_vk_code_name(VK_TABLE[idx].name);
        return;
    }

    /* Properties profile */
    if (cmd_id >= IDM_PROP_FIRST && cmd_id <= IDM_PROP_LAST) {
        int idx = cmd_id - IDM_PROP_FIRST;
        if (idx < g_prop_count) {
            cfg_set_selected_properties(g_prop_names[idx]);
            cfg_reload_properties();
        }
        return;
    }

    /* Number settings */
    for (int i = 0; i < (int)NUM_SETTINGS_COUNT; i++) {
        if (NUM_SETTINGS[i].id == cmd_id) {
            handle_number_setting(cmd_id);
            return;
        }
    }

    switch (cmd_id) {
    case IDM_SEND_MIDDLE_CLICK:
        cfg_set_boolean(L"sendMiddleClick", !cfg_is_send_middle_click()); break;
    case IDM_DRAGGED_LOCK:
        cfg_set_boolean(L"draggedLock", !cfg_is_dragged_lock()); break;

    case IDM_KEYBOARD_TOGGLE: {
        BOOL b = !cfg_is_keyboard_hook();
        cfg_set_boolean(L"keyboardHook", b);
        hook_set_or_unset_keyboard(b);
        break;
    }

    case IDM_ACCEL_TOGGLE:
        cfg_set_boolean(L"accelTable", !cfg_is_accel_table()); break;
    case IDM_ACCEL_M5: cfg_set_accel_multiplier_name(L"M5"); cfg_set_boolean(L"customAccelTable", FALSE); break;
    case IDM_ACCEL_M6: cfg_set_accel_multiplier_name(L"M6"); cfg_set_boolean(L"customAccelTable", FALSE); break;
    case IDM_ACCEL_M7: cfg_set_accel_multiplier_name(L"M7"); cfg_set_boolean(L"customAccelTable", FALSE); break;
    case IDM_ACCEL_M8: cfg_set_accel_multiplier_name(L"M8"); cfg_set_boolean(L"customAccelTable", FALSE); break;
    case IDM_ACCEL_M9: cfg_set_accel_multiplier_name(L"M9"); cfg_set_boolean(L"customAccelTable", FALSE); break;
    case IDM_ACCEL_CUSTOM:
        cfg_set_boolean(L"customAccelTable", !cfg_is_custom_accel()); break;

    case IDM_PRIO_HIGH: cfg_set_priority_name(L"High"); break;
    case IDM_PRIO_ABOVE_NORMAL: cfg_set_priority_name(L"AboveNormal"); break;
    case IDM_PRIO_NORMAL: cfg_set_priority_name(L"Normal"); break;

    case IDM_REAL_WHEEL_TOGGLE:
        cfg_set_boolean(L"realWheelMode", !cfg_is_real_wheel_mode()); break;
    case IDM_QUICK_FIRST:
        cfg_set_boolean(L"quickFirst", !cfg_is_quick_first()); break;
    case IDM_QUICK_TURN:
        cfg_set_boolean(L"quickTurn", !cfg_is_quick_turn()); break;

    case IDM_VH_TOGGLE:
        cfg_set_boolean(L"vhAdjusterMode", !cfg_is_vh_adjuster_mode()); break;
    case IDM_VH_FIXED: cfg_set_vh_method_name(L"Fixed"); break;
    case IDM_VH_SWITCHING: cfg_set_vh_method_name(L"Switching"); break;
    case IDM_VH_FIRST_PREFER_VERT:
        cfg_set_boolean(L"firstPreferVertical", !cfg_is_first_prefer_vertical()); break;

    case IDM_PROP_RELOAD: cfg_reload_properties(); break;
    case IDM_PROP_SAVE: cfg_store_properties(); break;
    case IDM_PROP_OPEN_DIR:
        ShellExecuteW(NULL, L"open", cfg_get_user_dir(), NULL, NULL, SW_SHOW); break;
    case IDM_PROP_ADD: {
        wchar_t name[256];
        if (dialog_text_input(cl(L"Properties Name"), cl(L"Add Properties"), name, 256)) {
            if (name[0] == L'-' && name[1] == L'-') {
                dialog_error(cl(L"Invalid Name"), cl(L"Name Error"));
            } else {
                cfg_properties_copy(cfg_get_selected_properties(), name);
                cfg_set_selected_properties(name);
                cfg_reload_properties();
            }
        }
        break;
    }
    case IDM_PROP_DELETE: {
        const wchar_t *sel = cfg_get_selected_properties();
        if (wcscmp(sel, L"Default") != 0) {
            cfg_properties_delete(sel);
            cfg_set_selected_properties(L"Default");
            cfg_reload_properties();
        }
        break;
    }
    case IDM_PROP_DEFAULT:
        cfg_set_selected_properties(L"Default");
        cfg_reload_properties();
        break;

    case IDM_CURSOR_CHANGE:
        cfg_set_boolean(L"cursorChange", !cfg_is_cursor_change()); break;
    case IDM_HORIZONTAL_SCROLL:
        cfg_set_boolean(L"horizontalScroll", !cfg_is_horizontal_scroll()); break;
    case IDM_REVERSE_SCROLL:
        cfg_set_boolean(L"reverseScroll", !cfg_is_reverse_scroll()); break;
    case IDM_SWAP_SCROLL:
        cfg_set_boolean(L"swapScroll", !cfg_is_swap_scroll()); break;
    case IDM_PASS_MODE: {
        BOOL b = !cfg_is_pass_mode();
        cfg_set_pass_mode(b);
        tray_update_icon();
        break;
    }

    case IDM_LANG_EN: cfg_set_language(L"en"); break;
    case IDM_LANG_JA: cfg_set_language(L"ja"); break;

    case IDM_EXIT: cfg_exit_action(); break;
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

    case WM_COMMAND:
        handle_command(LOWORD(wParam));
        return 0;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* ========== Public API ========== */

void tray_update_icon(void) {
    g_nid.hIcon = cfg_is_pass_mode() ? g_icon_stop : g_icon_run;
    _snwprintf(g_nid.szTip, 128, L"%s - %s",
               PROGRAM_NAME, cl(cfg_is_pass_mode() ? L"Stopped" : L"Runnable"));
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

HWND tray_init(HINSTANCE hInst) {
    /* Load tray icons from resources */
    g_icon_run = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_TRAY_RUN));
    g_icon_stop = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_TRAY_STOP));

    /* Register window class */
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = tray_wnd_proc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"W10WheelTray";
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP));
    RegisterClassExW(&wc);

    /* Create hidden window */
    g_hwnd = CreateWindowExW(0, L"W10WheelTray", PROGRAM_NAME, 0,
                             0, 0, 0, 0, NULL, NULL, hInst, NULL);

    /* Create system tray icon */
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = g_icon_run;
    _snwprintf(g_nid.szTip, 128, L"%s - %s", PROGRAM_NAME, cl(L"Runnable"));
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    return g_hwnd;
}

void tray_cleanup(void) {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    if (g_hwnd) DestroyWindow(g_hwnd);
}
