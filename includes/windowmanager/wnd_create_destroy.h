/**
 * @file wnd_create_destroy.h
 * @brief window manager create and destroy header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___WND_CREATE_DESTROY_H
#define ___WND_CREATE_DESTROY_H

#include <windowmanager/wnd_types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum windowmanager_alert_window_type {
    WINDOWMANAGER_ALERT_WINDOW_TYPE_INFO,
    WINDOWMANAGER_ALERT_WINDOW_TYPE_WARNING,
    WINDOWMANAGER_ALERT_WINDOW_TYPE_ERROR,
}windowmanager_alert_window_type_t;

void windowmanager_destroy_window(window_t* window);
void windowmanager_destroy_child_window(window_t* window, window_t* child);
void windowmanager_destroy_all_child_windows(window_t* window);
void windowmanager_insert_and_set_current_window(window_t* window);
void windowmanager_remove_and_set_current_window(window_t* window);
void windowmanager_create_and_show_alert_window(windowmanager_alert_window_type_t type, const char_t* text);

window_top_window_t windowmanager_create_top_window(const char_t* title, boolean_t has_command_input);

window_t* windowmanager_create_window(window_t* parent, char_t* text, rect_t rect, color_t background_color, color_t foreground_color);
window_t* windowmanager_create_primary_options_window(void);
window_t* windowmanager_create_greater_window(void);
window_t* windowmanager_command_input_window(window_t* parent, rect_t pos,
                                             const char_t* label_text,
                                             const char_t* input_text,
                                             const char_t* input_text_id,
                                             const char_t* tooltip_text);

int8_t windowmanager_create_and_show_editor_window(const char_t* title, const char_t* text, boolean_t is_text_readonly);
int8_t windowmanager_create_and_show_spool_browser_window(void);
int8_t windowmanager_create_and_show_task_vm_list_window(void);
int8_t windowmanager_create_and_show_task_vm_create_window(void);

#ifdef __cplusplus
}
#endif

#endif // ___WND_CREATE_DESTROY_H
