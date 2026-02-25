/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#ifndef W10WHEEL_TRAY_H
#define W10WHEEL_TRAY_H

#include <windows.h>

HWND tray_init(HINSTANCE hInst);
void tray_cleanup(void);
void tray_update_icon(void);
void tray_hook_alive(void);
void tray_start_health_timer(void);
void tray_update_health_timer(void);
HWND tray_get_hwnd(void);

#endif
