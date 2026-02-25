/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#ifndef W10WHEEL_RAWINPUT_H
#define W10WHEEL_RAWINPUT_H

#include <windows.h>

typedef void (*SendWheelRawFn)(int x, int y);

void rawinput_init(void);
void rawinput_set_send_wheel_raw(SendWheelRawFn fn);
void rawinput_register(void);
void rawinput_unregister(void);

#endif
