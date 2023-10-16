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

/*! module name */
MODULE("turnstone.kernel.hw.hpet");

/**
 * @brief stores if hpet is enabled
 */
boolean_t hpet_enabled = false;

/**
 * @brief hpet interrupt service routine
 * @param frame interrupt frame
 * @param irqno irq number
 * @return 0 if interrupt was handled, -1 otherwise
 */
int8_t hpet_isr(interrupt_frame_t* frame, uint8_t irqno);

int8_t hpet_isr(interrupt_frame_t* frame, uint8_t irqno) {
    UNUSED(frame);
    UNUSED(irqno);

    apic_eoi();
    return 0;
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

    PRINTLOG(HPET, LOG_TRACE, "count of comparators at table: %d", hpet_table->comparator_count);

    uint64_t hpet_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(hpet_table->address.address);
    memory_paging_add_page(hpet_va, hpet_table->address.address, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

    volatile hpet_t* hpet = (volatile hpet_t*)hpet_va;

    PRINTLOG(HPET, LOG_INFO, "HPET address: %p", hpet);

    hpet_capabilities_t capabilities = (hpet_capabilities_t) hpet->capabilities;


    PRINTLOG(HPET, LOG_TRACE, "number of timers: %d", capabilities.fields.number_of_timers);


    if(interrupt_irq_set_handler(17, &hpet_isr) != 0) {
        PRINTLOG(HPET, LOG_ERROR, "cannot set pic timer irq");

        return -1;
    }

    apic_ioapic_setup_irq(17,
                          APIC_IOAPIC_INTERRUPT_ENABLED
                          | APIC_IOAPIC_DELIVERY_MODE_FIXED | APIC_IOAPIC_DELIVERY_STATUS_RELAX
                          | APIC_IOAPIC_DESTINATION_MODE_PHYSICAL
                          | APIC_IOAPIC_TRIGGER_MODE_EDGE | APIC_IOAPIC_PIN_POLARITY_ACTIVE_HIGH);

    uint64_t counter_frequency = capabilities.fields.counter_clk_period;
    uint64_t period_nanoseconds = 1; // 1 nanosecond

    uint64_t period_femtoseconds = period_nanoseconds * 1000000;

    uint64_t comparator_value = (counter_frequency * period_femtoseconds) / 1000000000;

    PRINTLOG(HPET, LOG_TRACE, "counter frequency: 0x%llx", counter_frequency);
    PRINTLOG(HPET, LOG_TRACE, "period nanoseconds: 0x%llx", period_nanoseconds);
    PRINTLOG(HPET, LOG_TRACE, "comparator value: 0x%llx", comparator_value);

    hpet_timer_configuration_t tmr0_config = (hpet_timer_configuration_t) hpet->timer0_configuration;

    cpu_cli();

    //hpet->configuration = 1;

    tmr0_config.fields.interrupt_type = 0;
    tmr0_config.fields.interrupt_enable = 1;
    tmr0_config.fields.timer_type = 1;
    tmr0_config.fields.value_set = 1;
    tmr0_config.fields.interrupt_route = 17;

    hpet->timer0_configuration = tmr0_config.raw;

    hpet->timer0_comparator_value = comparator_value + hpet->main_counter;
    hpet->timer0_comparator_value = comparator_value;

    //hpet_enabled = true;

    cpu_sti();

    return 0;
}
