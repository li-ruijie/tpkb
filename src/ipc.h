/*
 * Copyright (c) 2026 Li Ruijie
 * Licensed under the GNU General Public License v3.0.
 */

#ifndef W10WHEEL_IPC_H
#define W10WHEEL_IPC_H

#include <windows.h>

void ipc_send_exit(void);
void ipc_send_pass_mode(BOOL b);
void ipc_send_reload_prop(void);
void ipc_send_init_state(void);

void ipc_proc_message(int msg);
void ipc_server_start(void);
void ipc_server_stop(void);

#endif
