/*
 * Copyright (c) 2016-2021 Yuki Ono
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#ifndef W10WHEEL_UTIL_H
#define W10WHEEL_UTIL_H

#include <windows.h>
#include "types.h"

/* Single instance lock */
BOOL util_try_lock(void);
void util_unlock(void);

/* Process priority */
void util_set_priority(Priority p);

/* Win32 error message */
void util_get_last_error_message(wchar_t *buf, int bufsize);

#endif
