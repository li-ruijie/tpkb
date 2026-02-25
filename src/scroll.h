/*
 * Copyright (c) 2016-2021 Yuki Ono
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#ifndef W10WHEEL_SCROLL_H
#define W10WHEEL_SCROLL_H

#include "types.h"

void scroll_init(void);

/* Event detection */
BOOL scroll_is_injected(const MouseEvent *me);
BOOL scroll_is_resend(const MouseEvent *me);
BOOL scroll_is_resend_click(const MouseEvent *me);

/* Input injection */
void scroll_send_input(POINT pt, int data, int flags, DWORD time, DWORD extra);
UINT scroll_send_input_direct(POINT pt, int data, int flags, DWORD time, DWORD extra);

/* Click resend */
void scroll_resend_click(MouseClickType type, const MSLLHOOKSTRUCT *info);
void scroll_resend_down(const MouseEvent *me);
void scroll_resend_up(const MouseEvent *me);

/* Modifier key detection */
BOOL scroll_check_shift(void);
BOOL scroll_check_ctrl(void);
BOOL scroll_check_alt(void);
BOOL scroll_check_esc(void);

/* Scroll wheel simulation */
void scroll_send_wheel(POINT move_pt);
void scroll_init_scroll(void);

/* Window/process info */
BOOL scroll_get_path_from_foreground(wchar_t *buf, int bufsize);

#endif
