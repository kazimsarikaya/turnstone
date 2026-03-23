/**
 * @file timer.h
 * @brief time interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___TIME_TIMER_H
/*! prevent duplicate header error macro */
#define ___TIME_TIMER_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

void time_timer_configure_sleep(void);

void time_timer_spinsleep(uint64_t usecs);

uint64_t time_timer_get_rdtsc_delta(void);
uint64_t time_timer_get_rdtsc_delta_us(void);

void time_timer_set_rdtsc_delta(uint64_t delta);
void time_timer_set_rdtsc_delta_us(uint64_t delta_us);

#ifdef __cplusplus
}
#endif

#endif
