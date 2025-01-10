/**
 * @file time_timer_spinsleep.64.c
 * @brief Time timer spinsleep implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <time/timer.h>
#include <logging.h>

MODULE("turnstone.kernel.timer.spinsleep");

volatile uint64_t time_timer_spinsleep_counter_value = 0;
volatile uint8_t time_timer_start_spinsleep_counter = 0;
volatile uint64_t time_timer_rdtsc_delta = 0;

void time_timer_spinsleep(uint64_t usecs) {
    PRINTLOG(TIMER, LOG_TRACE, "spinsleep for 0x%llx", usecs);
    while(usecs--) {
        uint64_t spinsleep_counter = time_timer_spinsleep_counter_value;
        while(spinsleep_counter--);
    }
    PRINTLOG(TIMER, LOG_TRACE, "spinsleep finished");
}
