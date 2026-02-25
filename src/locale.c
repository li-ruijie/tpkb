/*
 * Copyright (c) 2016-2021 Yuki Ono
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#include "locale.h"
#include <windows.h>
#include <string.h>

const wchar_t *ADMIN_MESSAGE =
    L"W10Wheel is not running in admin mode, so it won't work in certain windows. "
    L"It's highly recommended to run it as administrator. Click 'OK' for more info.";

typedef struct {
    const wchar_t *en;
    const wchar_t *ja;
} LocaleEntry;

static const LocaleEntry JA_TABLE[] = {
    { L"Double Launch?", L"\x4E8C\x91CD\x8D77\x52D5\x3057\x3066\x3044\x307E\x305B\x3093\x304B\xFF1F" },
    { L"Failed mouse hook install", L"\x30DE\x30A6\x30B9\x30D5\x30C3\x30AF\x306E\x30A4\x30F3\x30B9\x30C8\x30FC\x30EB\x306B\x5931\x6557\x3057\x307E\x3057\x305F" },
    { L"Properties does not exist", L"\x8A2D\x5B9A\x30D5\x30A1\x30A4\x30EB\x304C\x5B58\x5728\x3057\x307E\x305B\x3093" },
    { L"Unknown Command", L"\x4E0D\x660E\x306A\x30B3\x30DE\x30F3\x30C9" },
    { L"Command Error", L"\x30B3\x30DE\x30F3\x30C9\x306E\x30A8\x30E9\x30FC" },
    { L"Error", L"\x30A8\x30E9\x30FC" },
    { L"Question", L"\x8CEA\x554F" },
    { L"Stopped", L"\x505C\x6B62\x4E2D" },
    { L"Runnable", L"\x5B9F\x884C\x4E2D" },
    { L"Properties Name", L"\x8A2D\x5B9A\x30D5\x30A1\x30A4\x30EB\x540D" },
    { L"Add Properties", L"\x8A2D\x5B9A\x30D5\x30A1\x30A4\x30EB\x8FFD\x52A0" },
    { L"Invalid Name", L"\x7121\x52B9\x306A\x540D\x524D" },
    { L"Invalid Number", L"\x7121\x52B9\x306A\x6570\x5024" },
    { L"Name Error", L"\x540D\x524D\x306E\x30A8\x30E9\x30FC" },
    { L"Delete properties", L"\x8A2D\x5B9A\x30D5\x30A1\x30A4\x30EB\x524A\x9664" },
    { L"Set Text", L"\x30C6\x30AD\x30B9\x30C8\x3092\x8A2D\x5B9A" },
    { L"Trigger", L"\x30C8\x30EA\x30AC\x30FC" },
    { L"LR (Left <<-->> Right)", L"\x5DE6\x53F3 (\x5DE6 <<-->> \x53F3)" },
    { L"Left (Left -->> Right)", L"\x5DE6 (\x5DE6 -->> \x53F3)" },
    { L"Right (Right -->> Left)", L"\x53F3 (\x53F3 -->> \x5DE6)" },
    { L"Middle", L"\x4E2D\x592E" },
    { L"X1", L"X1 (\x62E1\x5F35" L"1)" },
    { L"X2", L"X2 (\x62E1\x5F35" L"2)" },
    { L"LeftDrag", L"\x5DE6\x30C9\x30E9\x30C3\x30B0" },
    { L"RightDrag", L"\x53F3\x30C9\x30E9\x30C3\x30B0" },
    { L"MiddleDrag", L"\x4E2D\x592E\x30C9\x30E9\x30C3\x30B0" },
    { L"X1Drag", L"X1 \x30C9\x30E9\x30C3\x30B0" },
    { L"X2Drag", L"X2 \x30C9\x30E9\x30C3\x30B0" },
    { L"None", L"\x306A\x3057" },
    { L"Send MiddleClick", L"\x4E2D\x592E\x30AF\x30EA\x30C3\x30AF\x9001\x4FE1" },
    { L"Dragged Lock", L"\x30C9\x30E9\x30C3\x30B0\x5F8C\x56FA\x5B9A" },
    { L"Keyboard", L"\x30AD\x30FC\x30DC\x30FC\x30C9" },
    { L"ON / OFF", L"\x6709\x52B9 / \x7121\x52B9" },
    { L"VK_CONVERT (Henkan)", L"VK_CONVERT (\x5909\x63DB)" },
    { L"VK_NONCONVERT (Muhenkan)", L"VK_NONCONVERT (\x7121\x5909\x63DB)" },
    { L"VK_LWIN (Left Windows)", L"VK_LWIN (\x5DE6 Windows)" },
    { L"VK_RWIN (Right Windows)", L"VK_RWIN (\x53F3 Windows)" },
    { L"VK_LSHIFT (Left Shift)", L"VK_LSHIFT (\x5DE6 Shift)" },
    { L"VK_RSHIFT (Right Shift)", L"VK_RSHIFT (\x53F3 Shift)" },
    { L"VK_LCONTROL (Left Ctrl)", L"VK_LCONTROL (\x5DE6 Ctrl)" },
    { L"VK_RCONTROL (Right Ctrl)", L"VK_RCONTROL (\x53F3 Ctrl)" },
    { L"VK_LMENU (Left Alt)", L"VK_LMENU (\x5DE6 Alt)" },
    { L"VK_RMENU (Right Alt)", L"VK_RMENU (\x53F3 Alt)" },
    { L"Accel Table", L"\x52A0\x901F\x30C6\x30FC\x30D6\x30EB" },
    { L"Custom Table", L"\x30AB\x30B9\x30BF\x30E0\x30C6\x30FC\x30D6\x30EB" },
    { L"Priority", L"\x30D7\x30ED\x30BB\x30B9\x512A\x5148\x5EA6" },
    { L"High", L"\x9AD8" },
    { L"Above Normal", L"\x901A\x5E38\x4EE5\x4E0A" },
    { L"Normal", L"\x901A\x5E38" },
    { L"Set Number", L"\x30D1\x30E9\x30E1\x30FC\x30BF\x30FC\x3092\x8A2D\x5B9A" },
    { L"pollTimeout", L"\x540C\x6642\x62BC\x3057\x5224\x5B9A\x6642\x9593" },
    { L"scrollLocktime", L"\x30B9\x30AF\x30ED\x30FC\x30EB\x30E2\x30FC\x30C9\x56FA\x5B9A\x5224\x5B9A\x6642\x9593" },
    { L"verticalThreshold", L"\x5782\x76F4\x30B9\x30AF\x30ED\x30FC\x30EB\x95BE\x5024" },
    { L"horizontalThreshold", L"\x6C34\x5E73\x30B9\x30AF\x30ED\x30FC\x30EB\x95BE\x5024" },
    { L"Real Wheel Mode", L"\x64EC\x4F3C\x30DB\x30A4\x30FC\x30EB\x30E2\x30FC\x30C9" },
    { L"wheelDelta", L"\x30DB\x30A4\x30FC\x30EB\x56DE\x8EE2\x5024" },
    { L"vWheelMove", L"\x5782\x76F4\x30DB\x30A4\x30FC\x30EB\x79FB\x52D5\x5024" },
    { L"hWheelMove", L"\x6C34\x5E73\x30DB\x30A4\x30FC\x30EB\x79FB\x52D5\x5024" },
    { L"quickFirst", L"\x521D\x56DE\x306E\x53CD\x5FDC\x3092\x901F\x304F\x3059\x308B" },
    { L"quickTurn", L"\x6298\x308A\x8FD4\x3057\x306E\x53CD\x5FDC\x3092\x901F\x304F\x3059\x308B" },
    { L"VH Adjuster", L"\x5782\x76F4/\x6C34\x5E73\x30B9\x30AF\x30ED\x30FC\x30EB\x8ABF\x6574" },
    { L"Fixed", L"\x56FA\x5B9A" },
    { L"Switching", L"\x5207\x308A\x66FF\x3048" },
    { L"firstPreferVertical", L"\x521D\x56DE\x5782\x76F4\x30B9\x30AF\x30ED\x30FC\x30EB\x512A\x5148" },
    { L"firstMinThreshold", L"\x521D\x56DE\x5224\x5B9A\x95BE\x5024" },
    { L"switchingThreshold", L"\x5207\x308A\x66FF\x3048\x95BE\x5024" },
    { L"Properties", L"\x8A2D\x5B9A\x30D5\x30A1\x30A4\x30EB" },
    { L"Reload", L"\x518D\x8AAD\x307F\x8FBC\x307F" },
    { L"Save", L"\x4FDD\x5B58" },
    { L"Open Dir", L"\x30D5\x30A9\x30EB\x30C0\x3092\x958B\x304F" },
    { L"Add", L"\x8FFD\x52A0" },
    { L"Delete", L"\x524A\x9664" },
    { L"Default", L"\x30C7\x30D5\x30A9\x30EB\x30C8" },
    { L"Cursor Change", L"\x30AB\x30FC\x30BD\x30EB\x5909\x66F4" },
    { L"Horizontal Scroll", L"\x6C34\x5E73\x30B9\x30AF\x30ED\x30FC\x30EB" },
    { L"Reverse Scroll (Flip)", L"\x5782\x76F4\x30B9\x30AF\x30ED\x30FC\x30EB\x53CD\x8EE2" },
    { L"Swap Scroll (V.H)", L"\x5782\x76F4/\x6C34\x5E73\x30B9\x30AF\x30ED\x30FC\x30EB\x5165\x308C\x66FF\x3048" },
    { L"Pass Mode", L"\x5236\x5FA1\x505C\x6B62" },
    { L"Language", L"\x8A00\x8A9E" },
    { L"English", L"\x82F1\x8A9E" },
    { L"Japanese", L"\x65E5\x672C\x8A9E" },
    { L"Info", L"\x60C5\x5831" },
    { L"Name", L"\x540D\x524D" },
    { L"Version", L"\x30D0\x30FC\x30B8\x30E7\x30F3" },
    { L"Exit", L"\x7D42\x4E86" },
    { L"dragThreshold", L"\x30C9\x30E9\x30C3\x30B0\x95BE\x5024" },
};

#define JA_TABLE_SIZE (sizeof(JA_TABLE) / sizeof(JA_TABLE[0]))

const wchar_t *locale_detect_language(void) {
    wchar_t lang[10];
    if (GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lang, 10) > 0) {
        if (wcscmp(lang, L"ja") == 0)
            return L"ja";
    }
    return L"en";
}

const wchar_t *locale_conv(const wchar_t *lang, const wchar_t *msg) {
    if (wcscmp(lang, L"ja") != 0)
        return msg;

    for (int i = 0; i < (int)JA_TABLE_SIZE; i++) {
        if (wcscmp(JA_TABLE[i].en, msg) == 0)
            return JA_TABLE[i].ja;
    }
    return msg;
}
