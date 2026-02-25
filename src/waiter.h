/*
 * Copyright (c) 2016-2021 Yuki Ono
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#ifndef W10WHEEL_WAITER_H
#define W10WHEEL_WAITER_H

#include "types.h"

void waiter_init(void);
BOOL waiter_offer(const MouseEvent *me);
void waiter_start(const MouseEvent *down);

#endif
