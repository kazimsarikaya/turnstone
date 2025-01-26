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
#include <cpu/interrupt.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TIME_TIMER_PIT_HZ_FOR_1MS   1000

void time_timer_reset_tick_count(void);

int8_t time_timer_pit_isr(interrupt_frame_ext_t* frame);
void   time_timer_pit_set_hz(uint16_t hz);
void   time_timer_pit_disable(void);
void   time_timer_pit_sleep(uint64_t usecs);


int8_t time_timer_apic_isr(interrupt_frame_ext_t* frame);

uint64_t time_timer_get_tick_count(void);

void time_timer_configure_spinsleep(void);

void time_timer_spinsleep(uint64_t usecs);

void time_timer_sleep(uint64_t secs);

#ifdef __cplusplus
}
#endif

#endif
