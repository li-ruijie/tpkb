/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#ifndef W10WHEEL_LOCALE_H
#define W10WHEEL_LOCALE_H

#include <wchar.h>

const wchar_t *locale_detect_language(void);
const wchar_t *locale_conv(const wchar_t *lang, const wchar_t *msg);

extern const wchar_t *ADMIN_MESSAGE;

#endif
