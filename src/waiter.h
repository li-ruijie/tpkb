/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#ifndef W10WHEEL_WAITER_H
#define W10WHEEL_WAITER_H

#include "types.h"

void waiter_init(void);
void waiter_cleanup(void);
BOOL waiter_offer(const MouseEvent *me);
BOOL waiter_start(const MouseEvent *down);

#endif
