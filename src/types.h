/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#ifndef TPKB_TYPES_H
#define TPKB_TYPES_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ========== Application constants ========== */

#define PROGRAM_NAME      L"tpkb"
#define PROGRAM_VERSION   L"3.0.1"

/* ========== Tray callback message ========== */

#define WM_TRAYICON  (WM_USER + 1)

/* ========== Mouse event flags ========== */

#define TPKB_MOUSEEVENTF_LEFTDOWN  0x0002
#define TPKB_MOUSEEVENTF_LEFTUP    0x0004
#define TPKB_MOUSEEVENTF_RIGHTDOWN 0x0008
#define TPKB_MOUSEEVENTF_RIGHTUP   0x0010
#define TPKB_MOUSEEVENTF_MIDDLEDOWN 0x0020
#define TPKB_MOUSEEVENTF_MIDDLEUP  0x0040
#define TPKB_MOUSEEVENTF_XDOWN     0x0080
#define TPKB_MOUSEEVENTF_XUP       0x0100
#define TPKB_MOUSEEVENTF_WHEEL     0x0800
#define TPKB_MOUSEEVENTF_HWHEEL    0x1000
#define TPKB_XBUTTON1 0x0001
#define TPKB_XBUTTON2 0x0002

/* ========== Cursor IDs ========== */

#define TPKB_OCR_NORMAL  32512
#define TPKB_OCR_IBEAM   32513
#define TPKB_OCR_HAND    32649
#define TPKB_OCR_SIZENS  32645
#define TPKB_OCR_SIZEWE  32644

/* ========== Trigger enum ========== */

typedef enum {
    TRIGGER_LR,
    TRIGGER_LEFT,
    TRIGGER_RIGHT,
    TRIGGER_MIDDLE,
    TRIGGER_X1,
    TRIGGER_X2,
    TRIGGER_LEFT_DRAG,
    TRIGGER_RIGHT_DRAG,
    TRIGGER_MIDDLE_DRAG,
    TRIGGER_X1_DRAG,
    TRIGGER_X2_DRAG,
    TRIGGER_NONE
} Trigger;

static inline BOOL trigger_is_single(Trigger t) {
    return t == TRIGGER_MIDDLE || t == TRIGGER_X1 || t == TRIGGER_X2;
}

static inline BOOL trigger_is_double(Trigger t) {
    return t == TRIGGER_LR || t == TRIGGER_LEFT || t == TRIGGER_RIGHT;
}

static inline BOOL trigger_is_drag(Trigger t) {
    return t >= TRIGGER_LEFT_DRAG && t <= TRIGGER_X2_DRAG;
}

/* ========== Mouse event type ========== */

typedef enum {
    ME_LEFT_DOWN, ME_LEFT_UP,
    ME_RIGHT_DOWN, ME_RIGHT_UP,
    ME_MIDDLE_DOWN, ME_MIDDLE_UP,
    ME_X1_DOWN, ME_X1_UP,
    ME_X2_DOWN, ME_X2_UP,
    ME_MOVE,
    ME_NON_EVENT
} MouseEventType;

typedef struct {
    MouseEventType type;
    MSLLHOOKSTRUCT info;
} MouseEvent;

static inline BOOL me_is_up(MouseEventType t) {
    return t == ME_LEFT_UP || t == ME_RIGHT_UP || t == ME_MIDDLE_UP ||
           t == ME_X1_UP || t == ME_X2_UP;
}

static inline BOOL me_is_left(MouseEventType t) {
    return t == ME_LEFT_DOWN || t == ME_LEFT_UP;
}

static inline BOOL me_is_right(MouseEventType t) {
    return t == ME_RIGHT_DOWN || t == ME_RIGHT_UP;
}

static inline BOOL me_is_middle(MouseEventType t) {
    return t == ME_MIDDLE_DOWN || t == ME_MIDDLE_UP;
}

static inline BOOL me_is_x1(MouseEventType t) {
    return t == ME_X1_DOWN || t == ME_X1_UP;
}

static inline BOOL me_is_x2(MouseEventType t) {
    return t == ME_X2_DOWN || t == ME_X2_UP;
}

/* Get the trigger type for a mouse event */
static inline Trigger me_get_trigger(MouseEventType t) {
    if (me_is_left(t))   return TRIGGER_LEFT;
    if (me_is_right(t))  return TRIGGER_RIGHT;
    if (me_is_middle(t)) return TRIGGER_MIDDLE;
    if (me_is_x1(t))     return TRIGGER_X1;
    if (me_is_x2(t))     return TRIGGER_X2;
    return TRIGGER_NONE;
}

/* Get the drag trigger type for a mouse event */
static inline Trigger me_get_drag_trigger(MouseEventType t) {
    if (me_is_left(t))   return TRIGGER_LEFT_DRAG;
    if (me_is_right(t))  return TRIGGER_RIGHT_DRAG;
    if (me_is_middle(t)) return TRIGGER_MIDDLE_DRAG;
    if (me_is_x1(t))     return TRIGGER_X1_DRAG;
    if (me_is_x2(t))     return TRIGGER_X2_DRAG;
    return TRIGGER_NONE;
}

static inline BOOL me_is_xbutton1(DWORD mouseData) {
    return (mouseData >> 16) == TPKB_XBUTTON1;
}

/* ========== Mouse click type (for resend) ========== */

typedef enum {
    MC_LEFT, MC_RIGHT, MC_MIDDLE, MC_X1, MC_X2
} MouseClickType;

typedef struct {
    MouseClickType type;
    MSLLHOOKSTRUCT info;
} MouseClick;

/* ========== Keyboard event type ========== */

typedef enum {
    KE_KEY_DOWN,
    KE_KEY_UP,
    KE_NON_EVENT
} KeyboardEventType;

typedef struct {
    KeyboardEventType type;
    KBDLLHOOKSTRUCT info;
} KeyboardEvent;

static inline int ke_vk_code(const KeyboardEvent *ke) {
    return (int)ke->info.vkCode;
}

/* ========== Acceleration preset ========== */

typedef enum {
    ACCEL_PRESET_M5, ACCEL_PRESET_M6, ACCEL_PRESET_M7, ACCEL_PRESET_M8, ACCEL_PRESET_M9
} AccelPreset;

/* ========== Process priority ========== */

typedef enum {
    PRIO_NORMAL, PRIO_ABOVE_NORMAL, PRIO_HIGH
} Priority;

/* ========== VH adjuster ========== */

typedef enum {
    VH_FIXED, VH_SWITCHING
} VHMethod;

typedef enum {
    VHD_VERTICAL, VHD_HORIZONTAL, VHD_NONE
} VHDirection;

/* ========== Move direction (real wheel mode) ========== */

typedef enum {
    DIR_PLUS, DIR_MINUS, DIR_ZERO
} MoveDirection;

/* ========== Acceleration tables (Kensington MouseWorks) ========== */

#define ACCEL_TABLE_SIZE 12

static const int DEFAULT_ACCEL_THRESHOLD[ACCEL_TABLE_SIZE] = {
    1, 2, 3, 5, 7, 10, 14, 20, 30, 43, 63, 91
};

static const double ACCEL_M5[ACCEL_TABLE_SIZE] = {
    1.0, 1.3, 1.7, 2.0, 2.4, 2.7, 3.1, 3.4, 3.8, 4.1, 4.5, 4.8
};
static const double ACCEL_M6[ACCEL_TABLE_SIZE] = {
    1.2, 1.6, 2.0, 2.4, 2.8, 3.3, 3.7, 4.1, 4.5, 4.9, 5.4, 5.8
};
static const double ACCEL_M7[ACCEL_TABLE_SIZE] = {
    1.4, 1.8, 2.3, 2.8, 3.3, 3.8, 4.3, 4.8, 5.3, 5.8, 6.3, 6.7
};
static const double ACCEL_M8[ACCEL_TABLE_SIZE] = {
    1.6, 2.1, 2.7, 3.2, 3.8, 4.4, 4.9, 5.5, 6.0, 6.6, 7.2, 7.7
};
static const double ACCEL_M9[ACCEL_TABLE_SIZE] = {
    1.8, 2.4, 3.0, 3.6, 4.3, 4.9, 5.5, 6.2, 6.8, 7.4, 8.1, 8.7
};

static inline const double *accel_preset_array(AccelPreset p) {
    switch (p) {
    case ACCEL_PRESET_M5: return ACCEL_M5;
    case ACCEL_PRESET_M6: return ACCEL_M6;
    case ACCEL_PRESET_M7: return ACCEL_M7;
    case ACCEL_PRESET_M8: return ACCEL_M8;
    case ACCEL_PRESET_M9: return ACCEL_M9;
    }
    return ACCEL_M5;
}

/* ========== VK code table (keyboard triggers) ========== */

typedef struct {
    const wchar_t *name;
    int code;
} VKEntry;

#define VK_ENTRY_COUNT 24

static const VKEntry VK_TABLE[VK_ENTRY_COUNT] = {
    { L"None",           0x00 },
    { L"VK_TAB",         0x09 },
    { L"VK_PAUSE",       0x13 },
    { L"VK_CAPITAL",     0x14 },
    { L"VK_CONVERT",     0x1C },
    { L"VK_NONCONVERT",  0x1D },
    { L"VK_PRIOR",       0x21 },
    { L"VK_NEXT",        0x22 },
    { L"VK_END",         0x23 },
    { L"VK_HOME",        0x24 },
    { L"VK_SNAPSHOT",    0x2C },
    { L"VK_INSERT",      0x2D },
    { L"VK_DELETE",      0x2E },
    { L"VK_LWIN",        0x5B },
    { L"VK_RWIN",        0x5C },
    { L"VK_APPS",        0x5D },
    { L"VK_NUMLOCK",     0x90 },
    { L"VK_SCROLL",      0x91 },
    { L"VK_LSHIFT",      0xA0 },
    { L"VK_RSHIFT",      0xA1 },
    { L"VK_LCONTROL",    0xA2 },
    { L"VK_RCONTROL",    0xA3 },
    { L"VK_LMENU",       0xA4 },
    { L"VK_RMENU",       0xA5 },
};

static inline int vk_code_from_name(const wchar_t *name) {
    for (int i = 0; i < VK_ENTRY_COUNT; i++)
        if (wcscmp(VK_TABLE[i].name, name) == 0) return VK_TABLE[i].code;
    return 0;
}

static inline const wchar_t *vk_name_from_code(int code) {
    for (int i = 0; i < VK_ENTRY_COUNT; i++)
        if (VK_TABLE[i].code == code) return VK_TABLE[i].name;
    return L"None";
}

/* ========== Trigger string table ========== */

typedef struct {
    const wchar_t *name;
    Trigger trigger;
} TriggerEntry;

#define TRIGGER_ENTRY_COUNT 12

static const TriggerEntry TRIGGER_TABLE[TRIGGER_ENTRY_COUNT] = {
    { L"LR",         TRIGGER_LR },
    { L"Left",       TRIGGER_LEFT },
    { L"Right",      TRIGGER_RIGHT },
    { L"Middle",     TRIGGER_MIDDLE },
    { L"X1",         TRIGGER_X1 },
    { L"X2",         TRIGGER_X2 },
    { L"LeftDrag",   TRIGGER_LEFT_DRAG },
    { L"RightDrag",  TRIGGER_RIGHT_DRAG },
    { L"MiddleDrag", TRIGGER_MIDDLE_DRAG },
    { L"X1Drag",     TRIGGER_X1_DRAG },
    { L"X2Drag",     TRIGGER_X2_DRAG },
    { L"None",       TRIGGER_NONE },
};

static inline Trigger trigger_from_name(const wchar_t *name) {
    for (int i = 0; i < TRIGGER_ENTRY_COUNT; i++)
        if (wcscmp(TRIGGER_TABLE[i].name, name) == 0) return TRIGGER_TABLE[i].trigger;
    /* Accept F#-style union case names (e.g., "MiddleTrigger" -> "Middle") */
    size_t len = wcslen(name);
    if (len > 7 && wcscmp(name + len - 7, L"Trigger") == 0) {
        wchar_t short_name[32];
        wcsncpy(short_name, name, len - 7);
        short_name[len - 7] = L'\0';
        for (int i = 0; i < TRIGGER_ENTRY_COUNT; i++)
            if (wcscmp(TRIGGER_TABLE[i].name, short_name) == 0) return TRIGGER_TABLE[i].trigger;
    }
    return TRIGGER_NONE;
}

static inline const wchar_t *trigger_to_name(Trigger t) {
    for (int i = 0; i < TRIGGER_ENTRY_COUNT; i++)
        if (TRIGGER_TABLE[i].trigger == t) return TRIGGER_TABLE[i].name;
    return L"None";
}

/* ========== Priority string table ========== */

static inline Priority priority_from_name(const wchar_t *name) {
    if (wcscmp(name, L"High") == 0) return PRIO_HIGH;
    if (wcscmp(name, L"AboveNormal") == 0 || wcscmp(name, L"Above Normal") == 0) return PRIO_ABOVE_NORMAL;
    return PRIO_NORMAL;
}

static inline const wchar_t *priority_to_name(Priority p) {
    switch (p) {
    case PRIO_HIGH: return L"High";
    case PRIO_ABOVE_NORMAL: return L"AboveNormal";
    default: return L"Normal";
    }
}

static inline VHMethod vh_method_from_name(const wchar_t *name) {
    if (wcscmp(name, L"Fixed") == 0) return VH_FIXED;
    return VH_SWITCHING;
}

static inline const wchar_t *vh_method_to_name(VHMethod m) {
    return m == VH_FIXED ? L"Fixed" : L"Switching";
}

static inline const wchar_t *accel_preset_to_name(AccelPreset p) {
    switch (p) {
    case ACCEL_PRESET_M5: return L"M5";
    case ACCEL_PRESET_M6: return L"M6";
    case ACCEL_PRESET_M7: return L"M7";
    case ACCEL_PRESET_M8: return L"M8";
    case ACCEL_PRESET_M9: return L"M9";
    }
    return L"M5";
}

static inline AccelPreset accel_preset_from_name(const wchar_t *name) {
    if (wcscmp(name, L"M6") == 0) return ACCEL_PRESET_M6;
    if (wcscmp(name, L"M7") == 0) return ACCEL_PRESET_M7;
    if (wcscmp(name, L"M8") == 0) return ACCEL_PRESET_M8;
    if (wcscmp(name, L"M9") == 0) return ACCEL_PRESET_M9;
    return ACCEL_PRESET_M5;
}

/* ========== LastFlags (event tracking) ========== */

typedef struct {
    volatile BOOL ld_resent, rd_resent;
    volatile BOOL ld_passed, rd_passed;
    volatile BOOL ld_suppressed, rd_suppressed, sd_suppressed;
    volatile BOOL kd_suppressed[256];
} LastFlags;

#endif /* TPKB_TYPES_H */
