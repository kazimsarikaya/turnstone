/**
 * @file wnd_editor.h
 * @brief window editor header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___WND_EDITOR_H
#define ___WND_EDITOR_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

int8_t windowmanager_create_and_show_editor_window(const char_t* title, const char_t* text, boolean_t is_text_readonly);

#ifdef __cplusplus
}
#endif

#endif // ___WND_EDITOR_H
