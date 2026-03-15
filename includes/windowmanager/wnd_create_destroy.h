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
void windowmanager_create_and_show_alert_window(windowmanager_alert_window_type_t type, const char16_t* text);

window_top_window_t windowmanager_create_top_window(const char16_t* title, boolean_t has_command_input);

typedef struct windowmanager_create_window_args_t {
    window_t*       parent;
    rect_t          rect;
    color_t         foreground_color;
    color_t         background_color;
    const char16_t* text;
    boolean_t       is_single_sheet;
    boolean_t       is_writable;
} windowmanager_create_window_args_t;

window_t* windowmanager_create_window_internal(windowmanager_create_window_args_t args);

#define windowmanager_create_window(p, r, ...) ({ \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Woverride-init\"") \
        _Pragma("GCC diagnostic ignored \"-Woverride-init-side-effects\"") \
        windowmanager_create_window_args_t __args = { \
        .background_color = (color_t){.color = 0x00000000}, \
        .foreground_color = (color_t){.color = 0xFFFFFFFF}, \
        .text = NULL, \
        .is_single_sheet = false, \
        .is_writable = false, \
        .parent = p, \
        .rect = r, \
        ## __VA_ARGS__ }; \
        window_t* __rc = windowmanager_create_window_internal(__args); \
        _Pragma("GCC diagnostic pop") \
        __rc; \
        })

window_t* windowmanager_create_primary_options_window(void);
window_t* windowmanager_create_greater_window(void);
window_t* windowmanager_command_input_window(window_t* parent, rect_t pos,
                                             const char16_t* label_text,
                                             uint32_t input_length,
                                             const char16_t* default_input_text,
                                             const char16_t* input_text_id,
                                             const char16_t* tooltip_text);

int8_t windowmanager_create_and_show_editor_window(const char16_t* title, const char16_t* text,
                                                   boolean_t is_text_readonly, boolean_t is_editor_readonly);
int8_t windowmanager_create_and_show_spool_browser_window(void);
int8_t windowmanager_create_and_show_task_vm_list_window(void);
int8_t windowmanager_create_and_show_task_vm_create_window(void);

#ifdef __cplusplus
}
#endif

#endif // ___WND_CREATE_DESTROY_H
