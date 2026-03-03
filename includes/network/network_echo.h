/**
 * @file network_echo.h
 * @brief Network echo protocol implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___NETWORK_ECHO_H
#define ___NETWORK_ECHO_H 0

#include <types.h>
#include <network/network_info.h>

#ifdef __cplusplus
extern "C" {
#endif

int8_t network_echo_init(const network_info_t* ni);

#ifdef __cplusplus
}
#endif

#endif
