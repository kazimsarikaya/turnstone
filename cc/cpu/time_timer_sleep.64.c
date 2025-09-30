/**
 * @file time_timer_spinsleep.64.c
 * @brief Time timer spinsleep implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <time/timer.h>
#include <time.h>
#include <device/hpet.h>
#include <logging.h>

MODULE("turnstone.kernel.timer.sleep");

volatile uint64_t time_timer_rdtsc_delta = 0; // for ms
volatile uint64_t time_timer_rdtsc_delta_us = 0; // for us

void time_timer_spinsleep(uint64_t usecs) {
    uint64_t end_tsc = rdtsc() + (time_timer_rdtsc_delta_us * usecs);
    while(rdtsc() < end_tsc) {
        asm volatile ("pause" ::: "memory");
    }
}

void time_timer_configure_sleep(void) {
    uint64_t total_tsc = 0;

    for(int i = 0; i < 10; i++) {
        volatile uint64_t start_tsc = rdtsc();
        hpet_usleep(1000 * 100); // 100 ms
        volatile uint64_t end_tsc = rdtsc();
        total_tsc += (end_tsc - start_tsc);
    }
    time_timer_rdtsc_delta = total_tsc / 1000; // for ms
    time_timer_rdtsc_delta_us = time_timer_rdtsc_delta / 1000; // for us
}

uint64_t time_timer_get_rdtsc_delta(void) {
    return time_timer_rdtsc_delta;
}

uint64_t time_timer_get_rdtsc_delta_us(void) {
    return time_timer_rdtsc_delta_us;
}
