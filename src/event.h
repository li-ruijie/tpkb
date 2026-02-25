/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#ifndef W10WHEEL_EVENT_H
#define W10WHEEL_EVENT_H

#include "types.h"

void event_init(void);

void event_set_call_next_hook(LRESULT (*fn)(void));

LRESULT event_left_down(const MSLLHOOKSTRUCT *info);
LRESULT event_left_up(const MSLLHOOKSTRUCT *info);
LRESULT event_right_down(const MSLLHOOKSTRUCT *info);
LRESULT event_right_up(const MSLLHOOKSTRUCT *info);
LRESULT event_middle_down(const MSLLHOOKSTRUCT *info);
LRESULT event_middle_up(const MSLLHOOKSTRUCT *info);
LRESULT event_x_down(const MSLLHOOKSTRUCT *info);
LRESULT event_x_up(const MSLLHOOKSTRUCT *info);
LRESULT event_move(const MSLLHOOKSTRUCT *info);

#endif
