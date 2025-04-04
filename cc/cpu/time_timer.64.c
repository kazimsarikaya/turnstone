/**
 * @file time_timer.64.c
 * @brief Timer driver.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <time/timer.h>
#include <logging.h>
#include <ports.h>
#include <cpu/task.h>
#include <cpu.h>
#include <apic.h>
#include <device/rtc.h>
#include <device/hpet.h>
#include <time.h>
#include <random.h>
#include <hypervisor/hypervisor_vm.h>

MODULE("turnstone.kernel.timer");

#define TIME_TIMER_PIT_BASE_HZ       1193181
#define TIME_TIMER_PIT_COMMAND_PORT  0x43
#define TIME_TIMER_PIT_COMMAND_PERIODIC 0x36
#define TIME_TIMER_PIT_COMMAND_ONE_SHOT 0x32
#define TIME_TIMER_PIT_DATA_PORT     0x40

__volatile__ uint64_t time_timer_tick_count = 0;
__volatile__ uint64_t time_timer_old_tick_count = 0;
__volatile__ uint64_t time_timer_ap1_tick_count = 0;


extern volatile uint64_t time_timer_spinsleep_counter_value;
extern volatile uint8_t time_timer_start_spinsleep_counter;

void time_timer_reset_tick_count(void) {
    time_timer_tick_count = 0;
}

void video_text_print(const char_t* string);

int8_t time_timer_pit_isr(interrupt_frame_ext_t* frame){
    UNUSED(frame);

    time_timer_tick_count++;

    apic_eoi();

    return 0;
}

void time_timer_pit_set_hz(uint16_t hz) {
    uint16_t divisor = TIME_TIMER_PIT_BASE_HZ / hz; /* Calculate our divisor */
    outb(TIME_TIMER_PIT_COMMAND_PORT, TIME_TIMER_PIT_COMMAND_PERIODIC); /* Set our command byte 0x36 */
    outb(TIME_TIMER_PIT_DATA_PORT, divisor & 0xFF); /* Set low byte of divisor */
    outb(TIME_TIMER_PIT_DATA_PORT, divisor >> 8); /* Set high byte of divisor */
}

void time_timer_pit_disable(void) {
    uint16_t divisor = TIME_TIMER_PIT_BASE_HZ / TIME_TIMER_PIT_HZ_FOR_1MS; /* Calculate our divisor */
    outb(TIME_TIMER_PIT_COMMAND_PORT, TIME_TIMER_PIT_COMMAND_ONE_SHOT); /* Set our command byte 0x36 */
    outb(TIME_TIMER_PIT_DATA_PORT, divisor & 0xFF); /* Set low byte of divisor */
    outb(TIME_TIMER_PIT_DATA_PORT, divisor >> 8); /* Set high byte of divisor */

    int32_t tries = 10;
    uint64_t old_tick_count = time_timer_tick_count;

    while(time_timer_tick_count <= old_tick_count && tries-- > 0) {
        asm volatile ("pause" ::: "memory");
        old_tick_count = time_timer_tick_count;
    }
}

void time_timer_pit_sleep(uint64_t usecs) {
    time_timer_reset_tick_count();
    while(time_timer_tick_count <= usecs);
}

extern volatile boolean_t task_tasking_initialized;

int8_t time_timer_apic_isr(interrupt_frame_ext_t* frame) {
    UNUSED(frame);

    uint32_t apic_id = apic_get_local_apic_id();

    if(apic_id == 0) {
        if(time_timer_tick_count < time_timer_ap1_tick_count) {
            time_timer_tick_count = time_timer_ap1_tick_count;
        }

        time_timer_tick_count++;

        if(time_timer_start_spinsleep_counter) {
            time_timer_start_spinsleep_counter = 0;
        }

        if(TIME_EPOCH == 0 || (time_timer_tick_count % (1000 * 60 * 15)) == 0) {
            TIME_EPOCH = rtc_get_time() * 1000000;
        } else if(!hpet_enabled) {
            TIME_EPOCH += 1000;
        }

        srand(TIME_EPOCH);

        hypervisor_vm_notify_timers();
    }

    if(apic_id == 1) {
        if(time_timer_ap1_tick_count == 0) {
            time_timer_ap1_tick_count = time_timer_tick_count;
        }

        time_timer_ap1_tick_count++;
    }

    if(task_tasking_initialized && (time_timer_tick_count % TASK_MAX_TICK_COUNT) == 0) {
        task_task_switch_set_parameters(true);
        task_switch_task();
        task_task_switch_exit();
    } else {
        apic_eoi();
    }

    return 0;
}

uint64_t time_timer_get_tick_count(void) {
    return time_timer_tick_count;
}

void time_timer_configure_spinsleep(void) {
    time_timer_start_spinsleep_counter = 1;

    while(time_timer_start_spinsleep_counter) {
        time_timer_spinsleep_counter_value++;
    }

    PRINTLOG(TIMER, LOG_TRACE, "spinsleep counter is 0x%llx", time_timer_spinsleep_counter_value);
}

void time_timer_sleep(uint64_t secs) {
    task_current_task_sleep(time_timer_get_tick_count() + secs * 1000);
}

void time_timer_msleep(uint64_t msecs) {
    task_current_task_sleep(time_timer_get_tick_count() + msecs);
}
