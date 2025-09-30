/**
 * @file hpet.64.c
 * @brief HPET timer driver for x86_64
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <device/hpet.h>
#include <logging.h>
#include <memory/paging.h>
#include <apic.h>
#include <cpu/interrupt.h>
#include <cpu.h>
#include <time.h>
#include <device/rtc.h>
#include <random.h>

/*! module name */
MODULE("turnstone.kernel.hw.hpet");

/**
 * @brief stores if hpet is enabled
 */
boolean_t hpet_enabled = false;

volatile uint64_t hpet_tick_count = 0;
uint64_t hpet_rtc_next_update = 0;
uint64_t hpet_last_rdtsc = 0;

#define HPET_RTC_UPDATE_TICK_COUNT  (15 * 60 * 1000 * 1000) / HPET_MIN_US_SLEEP // 15 minutes

void video_text_print(const char_t* str);

/**
 * @brief hpet interrupt service routine
 * @param frame interrupt frame
 * @param irqno irq number
 * @return 0 if interrupt was handled, -1 otherwise
 */
static int8_t hpet_isr(interrupt_frame_ext_t* frame) {
    UNUSED(frame);

    hpet_tick_count++;

    TIME_EPOCH += HPET_MIN_US_SLEEP;

    hpet_last_rdtsc = rdtsc();

    if(hpet_tick_count >= hpet_rtc_next_update) {
        hpet_rtc_next_update = hpet_tick_count + HPET_RTC_UPDATE_TICK_COUNT;
        TIME_EPOCH = rtc_get_time() * 1000000;

        srand(TIME_EPOCH);
    }

    apic_eoi();
    return 0;
}

void hpet_usleep(uint64_t usecs) {
    if(!hpet_enabled) {
        return;
    }

    if(usecs < HPET_MIN_US_SLEEP) {
        usecs = HPET_MIN_US_SLEEP;
    }

    uint64_t old_tick_count = hpet_tick_count;

    uint64_t ticks_needed = (usecs + HPET_MIN_US_SLEEP - 1) / HPET_MIN_US_SLEEP; // round up

    while((hpet_tick_count - old_tick_count) < ticks_needed) {
        asm volatile ("pause" ::: "memory");
    }
}


int8_t hpet_init(void) {
    hpet_table_t * hpet_table = (hpet_table_t *)acpi_get_table(ACPI_CONTEXT->xrsdp_desc, "HPET");

    if (hpet_table == NULL)
    {
        PRINTLOG(HPET, LOG_ERROR, "HPET table not found");

        return -1;
    }

    if(hpet_table->address.address_space_id != 0) {
        PRINTLOG(HPET, LOG_ERROR, "HPET address space is not memory");

        return -1;
    }

    PRINTLOG(HPET, LOG_INFO, "count of comparators at table: %d", hpet_table->comparator_count);

    uint64_t hpet_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(hpet_table->address.address);
    memory_paging_add_page(hpet_va, hpet_table->address.address, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    volatile hpet_t* hpet = (volatile hpet_t*)hpet_va;

    PRINTLOG(HPET, LOG_INFO, "HPET address: 0x%p", hpet);

    hpet_capabilities_t capabilities = (hpet_capabilities_t) hpet->capabilities;


    PRINTLOG(HPET, LOG_INFO, "number of timers: %d", capabilities.fields.number_of_timers);


    if(interrupt_irq_set_handler(17, &hpet_isr) != 0) {
        PRINTLOG(HPET, LOG_ERROR, "cannot set pic timer irq");

        return -1;
    }

    apic_ioapic_setup_irq(17,
                          APIC_IOAPIC_INTERRUPT_ENABLED
                          | APIC_IOAPIC_DELIVERY_MODE_FIXED | APIC_IOAPIC_DELIVERY_STATUS_RELAX
                          | APIC_IOAPIC_DESTINATION_MODE_PHYSICAL
                          | APIC_IOAPIC_TRIGGER_MODE_EDGE | APIC_IOAPIC_PIN_POLARITY_ACTIVE_HIGH);

    uint64_t period_fs = capabilities.fields.counter_clk_period;
    uint64_t ticks_per_ns = 1000000ULL / period_fs; // may round down
    uint64_t ticks_per_us = 1000000000ULL / period_fs;
    uint64_t ticks_per_ms = 1000000000000ULL / period_fs;


    PRINTLOG(HPET, LOG_INFO, "period: %llu fs, ticks: %llu for ns, %llu for us, %llu for ms.",
             period_fs, ticks_per_ns, ticks_per_us, ticks_per_ms);

    hpet_timer_configuration_t tmr0_config = (hpet_timer_configuration_t) hpet->timer0_configuration;

    cpu_cli();

    hpet->configuration = 0;

    tmr0_config.fields.interrupt_type = 0;
    tmr0_config.fields.interrupt_enable = 1;
    tmr0_config.fields.timer_type = 1;
    tmr0_config.fields.value_set = 1;
    tmr0_config.fields.interrupt_route = 17;

    hpet->timer0_configuration = tmr0_config.raw;

    hpet->timer0_comparator_value = ticks_per_us * HPET_MIN_US_SLEEP;

    hpet->main_counter = 0;

    hpet->configuration = 1;

    hpet_enabled = true;

    cpu_sti();

    TIME_EPOCH = rtc_get_time() * 1000000;

    PRINTLOG(HPET, LOG_INFO, "hpet initialized");

    return 0;
}
