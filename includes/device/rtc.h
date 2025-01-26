/**
 * @file rtc.h
 * @brief RTC header.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___RTC_H
#define ___RTC_H 0

#include <types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

time_t rtc_get_time(void);

#ifdef __cplusplus
}
#endif

#endif
