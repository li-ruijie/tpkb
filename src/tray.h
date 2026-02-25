/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#ifndef W10WHEEL_TRAY_H
#define W10WHEEL_TRAY_H

#include <windows.h>

HWND tray_init(HINSTANCE hInst);
void tray_cleanup(void);
void tray_update_icon(void);

#endif
