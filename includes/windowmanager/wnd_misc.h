/**
 * @file wnd_misc.h
 * @brief window miscellaneous utilities/operations header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___WND_MISC_H
#define ___WND_MISC_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

int8_t wndmgr_reboot(void);
int8_t wndmgr_power_off(void);

#ifdef __cplusplus
}
#endif

#endif // ___WND_MISC_H
