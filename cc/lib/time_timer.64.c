/*
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

MODULE("turnstone.lib");

#define TIME_TIMER_PIT_BASE_HZ       1193181
#define TIME_TIMER_PIT_COMMAND_PORT  0x43
#define TIME_TIMER_PIT_COMMAND_PERIODIC 0x36
#define TIME_TIMER_PIT_COMMAND_ONE_SHOT 0x32
#define TIME_TIMER_PIT_DATA_PORT     0x40

__volatile__ uint64_t time_timer_tick_count = 0;
__volatile__ uint64_t time_timer_old_tick_count = 0;
__volatile__ uint64_t time_timer_ap1_tick_count = 0;

__volatile__ uint64_t time_timer_spinsleep_counter_value = 0;
__volatile__ uint8_t time_timer_start_spinsleep_counter = 0;

void time_timer_reset_tick_count(void) {
    time_timer_tick_count = 0;
}

void video_text_print(char_t* string);

int8_t time_timer_pit_isr(interrupt_frame_t* frame, uint8_t intnum){
    UNUSED(frame);
    UNUSED(intnum);

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

boolean_t we_sended_nmi_to_bsp = false;

int8_t time_timer_apic_isr(interrupt_frame_t* frame, uint8_t intnum) {
    UNUSED(frame);
    UNUSED(intnum);

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
    }

    if(apic_id == 1) {
        if(time_timer_ap1_tick_count == 0) {
            time_timer_ap1_tick_count = time_timer_tick_count;
        }

        time_timer_ap1_tick_count++;
    }

    LOGBLOCK(TIMER, LOG_DEBUG) {

        if((time_timer_tick_count % 1000) == 0) {
            PRINTLOG(TIMER, LOG_DEBUG, "timer hits!, value 0x%llx epoch %lli", time_timer_tick_count, TIME_EPOCH);

            memory_heap_stat_t stat;
            memory_get_heap_stat(&stat);

            PRINTLOG(TIMER, LOG_DEBUG, "memory stat ts 0x%llx fs 0x%llx mc 0x%llx fc 0x%llx diff 0x%llx fh 0x%llx", stat.total_size, stat.free_size, stat.malloc_count, stat.free_count, stat.malloc_count - stat.free_count, stat.fast_hit);
        }

    }


    if(apic_id == 0) {
        if((time_timer_tick_count % TASK_MAX_TICK_COUNT) == 0) {
            task_task_switch_set_parameters(true, false);
            task_switch_task();
        } else {
            apic_eoi();
        }
    } else {
        if(apic_id == 1) {
            if(time_timer_tick_count > time_timer_old_tick_count) {
                time_timer_old_tick_count = time_timer_tick_count;
                we_sended_nmi_to_bsp = false;
            } else if(!we_sended_nmi_to_bsp && time_timer_ap1_tick_count > time_timer_old_tick_count + 0x5000) {
                // bsp may be in stuck state, wake it up
                we_sended_nmi_to_bsp = true;
                apic_send_nmi(0);
            }
        }

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

void time_timer_spinsleep(uint64_t usecs) {
    PRINTLOG(TIMER, LOG_TRACE, "spinsleep for 0x%llx", usecs);
    while(usecs--) {
        uint64_t spinsleep_counter = time_timer_spinsleep_counter_value;
        while(spinsleep_counter--);
    }
    PRINTLOG(TIMER, LOG_TRACE, "spinsleep finished");
}

void time_timer_sleep(uint64_t secs) {
    task_current_task_sleep(time_timer_get_tick_count() + secs * 1000);
}
