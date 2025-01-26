/**
 * @file wnd_options.h
 * @brief window manager options header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___WND_OPTIONS_H
#define ___WND_OPTIONS_H

#include <windowmanager/wnd_types.h>

#ifdef __cplusplus
extern "C" {
#endif

window_t* windowmanager_add_option_window(window_t* parent, rect_t pos);
window_t* windowmanager_create_primary_options_window(void);

#ifdef __cplusplus
}
#endif

#endif // ___WND_OPTIONS_H
