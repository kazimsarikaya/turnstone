/**
 * @file timer.h
 * @brief time interface
 */
#ifndef ___TIME_TIMER_H
/*! prevent duplicate header error macro */
#define ___TIME_TIMER_H 0

#include <types.h>
#include <cpu/interrupt.h>

#define TIME_TIMER_PIT_HZ_FOR_1MS   1000

void time_timer_reset_tick_count();

void time_timer_pit_isr(interrupt_frame_t* frame, uint8_t intnum);
void time_timer_pit_set_hz(uint16_t hz);
void time_timer_pit_sleep(uint64_t usecs);


void time_timer_apic_isr(interrupt_frame_t* frame, uint8_t intnum);

uint64_t time_timer_get_tick_count();

void time_timer_configure_spinsleep();

void time_timer_spinsleep(uint64_t usecs);

#endif
