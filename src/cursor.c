/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#include "cursor.h"
#include "types.h"

#ifndef SPI_SETCURSORS
#define SPI_SETCURSORS 0x0057
#endif
#ifndef IMAGE_CURSOR
#define IMAGE_CURSOR   2
#endif

static HCURSOR g_cursor_v;
static HCURSOR g_cursor_h;

void cursor_init(void) {
    g_cursor_v = (HCURSOR)LoadImageW(NULL, MAKEINTRESOURCEW(W10_OCR_SIZENS),
                                     IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    g_cursor_h = (HCURSOR)LoadImageW(NULL, MAKEINTRESOURCEW(W10_OCR_SIZEWE),
                                     IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
}

static void cursor_change(HCURSOR hcur) {
    SetSystemCursor(CopyIcon(hcur), W10_OCR_NORMAL);
    SetSystemCursor(CopyIcon(hcur), W10_OCR_IBEAM);
    SetSystemCursor(CopyIcon(hcur), W10_OCR_HAND);
}

void cursor_change_v(void) { cursor_change(g_cursor_v); }
void cursor_change_h(void) { cursor_change(g_cursor_h); }

void cursor_restore(void) {
    SystemParametersInfoW(SPI_SETCURSORS, 0, NULL, 0);
}
