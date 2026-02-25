/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the MIT License.
 */

#ifndef W10WHEEL_CONFIG_H
#define W10WHEEL_CONFIG_H

#include "types.h"

/* ========== Global state ========== */

/* Trigger & input */
Trigger       cfg_get_trigger(void);
void          cfg_set_trigger(Trigger t);
int           cfg_get_poll_timeout(void);
int           cfg_get_drag_threshold(void);
BOOL          cfg_is_keyboard_hook(void);
int           cfg_get_target_vk_code(void);
BOOL          cfg_is_send_middle_click(void);

/* Pass mode */
BOOL          cfg_is_pass_mode(void);
void          cfg_set_pass_mode(BOOL b);

/* Scroll state */
BOOL          cfg_is_scroll_mode(void);
void          cfg_start_scroll(const MSLLHOOKSTRUCT *info);
void          cfg_start_scroll_k(const KBDLLHOOKSTRUCT *info);
void          cfg_exit_scroll(void);
BOOL          cfg_check_exit_scroll(DWORD time);
void          cfg_get_scroll_start_point(int *x, int *y);
BOOL          cfg_is_released_scroll(void);
BOOL          cfg_is_pressed_scroll(void);
void          cfg_set_released_scroll(void);
void          cfg_set_starting_scroll(void);
BOOL          cfg_is_starting_scroll(void);

/* Scroll options */
int           cfg_get_scroll_locktime(void);
BOOL          cfg_is_cursor_change(void);
BOOL          cfg_is_reverse_scroll(void);
BOOL          cfg_is_horizontal_scroll(void);
BOOL          cfg_is_dragged_lock(void);
BOOL          cfg_is_swap_scroll(void);

/* Real wheel */
BOOL          cfg_is_real_wheel_mode(void);
int           cfg_get_wheel_delta(void);
int           cfg_get_v_wheel_move(void);
int           cfg_get_h_wheel_move(void);
BOOL          cfg_is_quick_first(void);
BOOL          cfg_is_quick_turn(void);

/* Acceleration */
BOOL          cfg_is_accel_table(void);
const int    *cfg_get_accel_threshold(int *count);
const double *cfg_get_accel_multiplier(int *count);
AccelPreset   cfg_get_accel_preset(void);
BOOL          cfg_is_custom_accel(void);

/* VH adjuster */
BOOL          cfg_is_vh_adjuster_mode(void);
BOOL          cfg_is_vh_adjuster_switching(void);
BOOL          cfg_is_first_prefer_vertical(void);
int           cfg_get_first_min_threshold(void);
int           cfg_get_switching_threshold(void);

/* Thresholds */
int           cfg_get_vertical_threshold(void);
int           cfg_get_horizontal_threshold(void);

/* Trigger helpers */
BOOL          cfg_is_trigger(Trigger t);
BOOL          cfg_is_trigger_event(MouseEventType t);
BOOL          cfg_is_drag_trigger_event(MouseEventType t);
BOOL          cfg_is_lr_trigger(void);
BOOL          cfg_is_single_trigger(void);
BOOL          cfg_is_double_trigger(void);
BOOL          cfg_is_drag_trigger(void);
BOOL          cfg_is_none_trigger(void);
BOOL          cfg_is_trigger_key(const KeyboardEvent *ke);

/* Language */
const wchar_t *cfg_get_language(void);
const wchar_t *cfg_conv_lang(const wchar_t *msg);

/* Priority */
Priority      cfg_get_priority(void);

/* LastFlags */
LastFlags    *cfg_last_flags(void);
void          cfg_last_flags_init(void);
void          cfg_last_flags_set_resent(const MouseEvent *me);
BOOL          cfg_last_flags_get_reset_resent(const MouseEvent *me);
void          cfg_last_flags_set_passed(const MouseEvent *me);
BOOL          cfg_last_flags_get_reset_passed(const MouseEvent *me);
void          cfg_last_flags_set_suppressed(const MouseEvent *me);
void          cfg_last_flags_set_suppressed_k(const KeyboardEvent *ke);
BOOL          cfg_last_flags_get_reset_suppressed(const MouseEvent *me);
BOOL          cfg_last_flags_get_reset_suppressed_k(const KeyboardEvent *ke);
void          cfg_last_flags_reset_lr(const MouseEvent *me);

/* Properties file I/O */
void          cfg_load_properties_file_only(void);
void          cfg_load_properties(BOOL update);
void          cfg_store_properties(void);
void          cfg_reload_properties(void);
void          cfg_set_selected_properties(const wchar_t *name);
const wchar_t *cfg_get_selected_properties(void);

/* Properties path helpers */
void          cfg_get_properties_path(const wchar_t *name, wchar_t *buf, int bufsize);
BOOL          cfg_properties_exists(const wchar_t *name);
void          cfg_properties_copy(const wchar_t *src, const wchar_t *dest);
void          cfg_properties_delete(const wchar_t *name);
int           cfg_get_prop_files(wchar_t names[][256], int maxcount);
const wchar_t *cfg_get_user_dir(void);

/* Initialization */
void          cfg_init(void);

/* Callbacks (break circular dependencies) */
typedef void (*VoidCallback)(void);
typedef void (*SendWheelRawCallback)(int, int);

void          cfg_set_init_scroll_cb(VoidCallback f);
void          cfg_set_change_trigger_cb(VoidCallback f);
void          cfg_set_init_state_meh_cb(VoidCallback f);
void          cfg_set_init_state_keh_cb(VoidCallback f);

void          cfg_init_state(void);
void          cfg_exit_action(void);

/* Number settings by name */
int           cfg_get_number(const wchar_t *name);
void          cfg_set_number(const wchar_t *name, int n);

/* Boolean settings by name */
BOOL          cfg_get_boolean(const wchar_t *name);
void          cfg_set_boolean(const wchar_t *name, BOOL b);

/* String settings */
void          cfg_set_accel_multiplier_name(const wchar_t *name);
void          cfg_set_priority_name(const wchar_t *name);
void          cfg_set_vk_code_name(const wchar_t *name);
void          cfg_set_vh_method_name(const wchar_t *name);
void          cfg_set_language(const wchar_t *lang);
void          cfg_set_trigger_name(const wchar_t *name);

#endif
