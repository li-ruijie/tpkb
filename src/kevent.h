/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#ifndef W10WHEEL_KEVENT_H
#define W10WHEEL_KEVENT_H

#include "types.h"

void kevent_init(void);
void kevent_set_call_next_hook(LRESULT (*fn)(void));

LRESULT kevent_key_down(const KBDLLHOOKSTRUCT *info);
LRESULT kevent_key_up(const KBDLLHOOKSTRUCT *info);

#endif
