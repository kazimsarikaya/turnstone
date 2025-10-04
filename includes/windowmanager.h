/**
 * @file windowmanager.h
 * @brief window manager header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___WINDOWMANAGER_H
#define ___WINDOWMANAGER_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct windowmanager_t windowmanager_t;

int8_t    windowmanager_init(void);
boolean_t windowmanager_is_initialized(void);
boolean_t windowmanager_set_initialized(boolean_t initialized);

windowmanager_t* windowmanager_get_instance(void);

#ifdef __cplusplus
}
#endif

#endif
