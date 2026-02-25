/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#ifndef W10WHEEL_DIALOG_H
#define W10WHEEL_DIALOG_H

#include <windows.h>

void dialog_error(const wchar_t *msg, const wchar_t *title);
BOOL dialog_number_input(const wchar_t *name, const wchar_t *title,
                         int low, int up, int cur, int *result);
BOOL dialog_text_input(const wchar_t *msg, const wchar_t *title,
                       wchar_t *buf, int bufsize);

#endif
