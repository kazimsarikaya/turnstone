/**
 * @file wnd_task_manager.h
 * @brief window manager task manager header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___WND_TASK_MANAGER_H
#define ___WND_TASK_MANAGER_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

int8_t windowmanager_create_and_show_task_vm_list_window(void);
int8_t windowmanager_create_and_show_task_vm_create_window(void);

#ifdef __cplusplus
}
#endif

#endif // ___WND_TASKMGR_H
