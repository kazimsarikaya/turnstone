/**
 * @file apic.64.c
 * @brief apic and ioapic implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <apic.h>
#include <cpu.h>
#include <cpu/cpu_state.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <acpi.h>
#include <iterator.h>
#include <cpu/interrupt.h>
#include <logging.h>
#include <time/timer.h>
#include <list.h>
#include <time.h>

MODULE("turnstone.kernel.cpu.apic");

uint8_t apic_init_ioapic(const acpi_table_madt_entry_t* ioapic);
int8_t  apic_init_timer(void);

uint64_t ioapic_bases[2] = {0, 0};
uint8_t ioapic_count = 0;
uint64_t lapic_addr = 0;
int8_t apic_enabled = 0;
uint32_t lapic_initial_timer_count = 0;
uint64_t apic_ap_count = 0;
boolean_t apic_x2apic = false;

list_t* irq_remappings = NULL;

extern volatile uint64_t time_timer_rdtsc_delta;

typedef uint32_t (*lock_get_local_apic_id_getter_f)(void);
extern lock_get_local_apic_id_getter_f lock_get_local_apic_id_getter;

extern boolean_t local_apic_id_is_valid;
extern cpu_state_t __seg_gs * cpu_state;

static inline uint64_t apic_read_timer_current_value(void) {
    if(apic_x2apic) {
        return cpu_read_msr(APIC_X2APIC_MSR_TIMER_CURRENT_VALUE);
    } else {
        return *((volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_TIMER_CURRENT_VALUE));
    }
}

static inline void apic_write_timer_initial_value(uint32_t value) {
    if(apic_x2apic) {
        cpu_write_msr(APIC_X2APIC_MSR_TIMER_INITIAL_VALUE, value);
    } else {
        *((volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_TIMER_INITIAL_VALUE)) = value;
    }
}

static inline void apic_write_timer_divide_configuration(uint32_t value) {
    if(apic_x2apic) {
        cpu_write_msr(APIC_X2APIC_MSR_TIMER_DIVIDER, value);
    } else {
        *((volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_TIMER_DIVIDER)) = value;
    }
}

static inline void apic_write_timer_lvt(uint32_t value) {
    if(apic_x2apic) {
        cpu_write_msr(APIC_X2APIC_MSR_LVT_TIMER, value);
    } else {
        *((volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_TIMER_LVT)) = value;
    }
}

static inline uint32_t apic_read_timer_lvt(void) {
    if(apic_x2apic) {
        return cpu_read_msr(APIC_X2APIC_MSR_LVT_TIMER);
    } else {
        return *((volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_TIMER_LVT));
    }
}

static inline void apic_write_spurious_interrupt_vector(uint32_t value) {
    if(apic_x2apic) {
        cpu_write_msr(APIC_X2APIC_MSR_SIVR, value);
    } else {
        *((volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_SPURIOUS_INTERRUPT)) = value;
    }
}

static inline void apic_write_lvt_lint0(uint32_t value) {
    if(apic_x2apic) {
        cpu_write_msr(APIC_X2APIC_MSR_LVT_LINT0, value);
    } else {
        *((volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_LINT0_LVT)) = value;
    }
}

static inline void apic_write_lvt_lint1(uint32_t value) {
    if(apic_x2apic) {
        cpu_write_msr(APIC_X2APIC_MSR_LVT_LINT1, value);
    } else {
        *((volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_LINT1_LVT)) = value;
    }
}

int8_t apic_setup(acpi_xrsdp_descriptor_t* desc) {
    acpi_sdt_header_t* madt = acpi_get_table(desc, "APIC");


    PRINTLOG(APIC, LOG_INFO, "apic and ioapic initialization");

    if(madt == NULL) {
        PRINTLOG(APIC, LOG_ERROR, "can not find madt or incorrect checksum");
        return -1;
    }

    PRINTLOG(APIC, LOG_DEBUG, "madt is found");

    list_t* apic_entries = acpi_get_apic_table_entries(madt);

    if(apic_init_apic(apic_entries) != 0) {
        PRINTLOG(APIC, LOG_ERROR, "cannot enable apic");

        return -1;
    }

    PRINTLOG(APIC, LOG_INFO, "apic and ioapic enabled");

    return 0;
}

int8_t apic_init_apic(list_t* apic_entries){
    cpu_cpuid_regs_t query = {0x1, 0, 0, 0};
    cpu_cpuid_regs_t answer = {0, 0, 0, 0};

    if(cpu_cpuid(query, &answer) != 0) {
        PRINTLOG(APIC, LOG_DEBUG, "Fatal cannot read cpuid");

        return -1;
    }


    irq_remappings = list_create_list();
    if(irq_remappings == NULL) {
        return -1;
    }

    uint64_t apic_enable_flag = APIC_MSR_ENABLE_APIC;
    if(answer.ecx & (1 << 21)) {
        PRINTLOG(APIC, LOG_INFO, "x2apic found");
        apic_enable_flag |= APIC_MSR_ENABLE_X2APIC;
        apic_x2apic = true;
    } else {
        PRINTLOG(APIC, LOG_INFO, "apic found");
    }

    uint64_t apic_msr = cpu_read_msr(APIC_MSR_ADDRESS);
    apic_msr |= apic_enable_flag;
    cpu_write_msr(APIC_MSR_ADDRESS, apic_msr);

    const acpi_table_madt_entry_t* la = NULL;


    iterator_t* iter = list_iterator_create(apic_entries);

    while(iter->end_of_iterator(iter) != 0) {
        const acpi_table_madt_entry_t* e = iter->get_item(iter);

        if(e->info.type == ACPI_MADT_ENTRY_TYPE_LOCAL_APIC_ADDRESS_OVERRIDE) {
            la = e;
        }

        if(e->info.type == ACPI_MADT_ENTRY_TYPE_IOAPIC) {
            apic_init_ioapic(e);
        }

        if(e->info.type == ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE) {
            list_list_insert(irq_remappings, e);
            PRINTLOG(APIC, LOG_DEBUG, "source irq 0x%02x is at gsi 0x%02x", e->interrupt_source_override.irq_source, e->interrupt_source_override.global_system_interrupt);
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    if(la == NULL) {
        la = list_get_data_at_position(apic_entries, 0);
        lapic_addr = la->local_apic_address.address;
    } else {
        lapic_addr = la->local_apic_address_override.address;
    }

    frame_t* lapic_frames = frame_get_allocator()->get_reserved_frames_of_address(frame_get_allocator(), (void*)lapic_addr);

    if(lapic_frames == NULL) {
        PRINTLOG(APIC, LOG_DEBUG, "cannot find frames of lapic 0x%016llx", lapic_addr);
        frame_t tmp_lapic_frm = {lapic_addr, 1, FRAME_TYPE_RESERVED, FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED};

        if(frame_get_allocator()->reserve_system_frames(frame_get_allocator(), &tmp_lapic_frm) != 0) {
            PRINTLOG(APIC, LOG_ERROR, "cannot reserve frames of lapic 0x%016llx", lapic_addr);

            return -1;
        }

        memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(tmp_lapic_frm.frame_address), &tmp_lapic_frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

        lapic_addr = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(lapic_addr);

        PRINTLOG(APIC, LOG_DEBUG, "lapic address mapped to 0x%016llx", lapic_addr);

    } else if((lapic_frames->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) != FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) {
        PRINTLOG(APIC, LOG_TRACE, "frames of lapic 0x%016llx is 0x%llx 0x%llx", lapic_addr, lapic_frames->frame_address, lapic_frames->frame_count);

        memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(lapic_frames->frame_address), lapic_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

        lapic_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;

        lapic_addr = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(lapic_addr);
    }

    PRINTLOG(APIC, LOG_DEBUG, "local apic address is: 0x%08llx", lapic_addr);

    apic_write_spurious_interrupt_vector(0x10f);

    apic_ap_count = apic_get_ap_count();
    lock_get_local_apic_id_getter = &apic_get_local_apic_id;
    apic_enabled = 1;

    return apic_init_timer();
}

uint8_t apic_get_irq_override(uint8_t old_irq){
    iterator_t* iter = list_iterator_create(irq_remappings);

    while(iter->end_of_iterator(iter) != 0) {
        const acpi_table_madt_entry_t* e = iter->get_item(iter);

        if(e->interrupt_source_override.irq_source == old_irq) {
            old_irq = e->interrupt_source_override.global_system_interrupt;
            break;
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return old_irq;
}

int8_t apic_init_timer(void) {
    PRINTLOG(APIC, LOG_DEBUG, "timer init started");

    uint8_t timer_irq = apic_get_irq_override(0);

    PRINTLOG(APIC, LOG_DEBUG, "pic timer irq is: 0x%02x", timer_irq);

    time_timer_pit_set_hz(TIME_TIMER_PIT_HZ_FOR_1MS);

    if(interrupt_irq_set_handler(timer_irq, &time_timer_pit_isr) != 0) {
        PRINTLOG(APIC, LOG_ERROR, "cannot set pic timer irq");

        return -1;
    }

    apic_ioapic_setup_irq(timer_irq,
                          APIC_IOAPIC_INTERRUPT_ENABLED
                          | APIC_IOAPIC_DELIVERY_MODE_FIXED | APIC_IOAPIC_DELIVERY_STATUS_RELAX
                          | APIC_IOAPIC_DESTINATION_MODE_PHYSICAL
                          | APIC_IOAPIC_TRIGGER_MODE_EDGE | APIC_IOAPIC_PIN_POLARITY_ACTIVE_HIGH);


    PRINTLOG(APIC, LOG_DEBUG, "pic timer enabled");


    apic_write_timer_divide_configuration(0x3);

    uint32_t total_inits = 0;

    for(uint8_t i = 0; i < 10; i++) {
        apic_write_timer_initial_value(0xFFFFFFFF);

        time_timer_pit_sleep(100);

        total_inits += (0xFFFFFFFF - apic_read_timer_current_value()) / 100;
    }

    lapic_initial_timer_count = total_inits / 10;

    apic_write_timer_initial_value(lapic_initial_timer_count);

    PRINTLOG(APIC, LOG_DEBUG, "apic timer configuration started");

    if(interrupt_irq_set_handler(0x0, &time_timer_apic_isr) != 0) {
        PRINTLOG(APIC, LOG_ERROR, "cannot set apic timer irq");

        return -1;
    }

    time_timer_pit_disable();

    apic_write_timer_lvt(APIC_TIMER_PERIODIC | APIC_INTERRUPT_ENABLED | 0x20);

    PRINTLOG(APIC, LOG_DEBUG, "apic timer initialized");

    apic_ioapic_disable_irq(timer_irq);
    interrupt_irq_remove_handler(timer_irq, &time_timer_pit_isr);
    PRINTLOG(APIC, LOG_DEBUG, "pic timer disabled");

    time_timer_reset_tick_count();
    time_timer_configure_spinsleep();

    uint64_t old_tsc = rdtsc();
    time_timer_spinsleep(1000); // 1ms
    uint64_t new_tsc = rdtsc();

    uint64_t delta = new_tsc - old_tsc;
    time_timer_rdtsc_delta = delta;

    PRINTLOG(APIC, LOG_INFO, "delta is 0x%016llx", delta);


    return 0;
}

void apic_enable_lapic(void) {
    uint64_t apic_msr = cpu_read_msr(APIC_MSR_ADDRESS);
    apic_msr |= APIC_MSR_ENABLE_APIC;

    if(apic_x2apic) {
        apic_msr |= APIC_MSR_ENABLE_X2APIC;
    }

    cpu_write_msr(APIC_MSR_ADDRESS, apic_msr);
}

uint8_t apic_configure_lapic(void) {
    apic_write_spurious_interrupt_vector(0x10f);

    apic_write_lvt_lint0(APIC_ICR_DELIVERY_MODE_EXTERNAL_INT);
    apic_write_lvt_lint1(APIC_ICR_DELIVERY_MODE_NMI);

    apic_write_timer_divide_configuration(0x3);
    apic_write_timer_initial_value(lapic_initial_timer_count);

    apic_write_timer_lvt(APIC_TIMER_PERIODIC | APIC_INTERRUPT_ENABLED | 0x20);

    return 0;
}

boolean_t apic_is_waiting_timer(void) {
    if(apic_enabled) {
        uint32_t current_lvt = apic_read_timer_lvt();
        uint8_t timer_irq = current_lvt & 0xFF;

        uint32_t isr_index = timer_irq / 32;
        uint32_t isr_bit = timer_irq % 32;

        if(apic_x2apic) {
            uint32_t isr = cpu_read_msr(APIC_X2APIC_MSR_ISR0 + isr_index);

            return (isr & (1 << isr_bit)) != 0;
        } else {
            volatile uint32_t* isr = (volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_ISR0 + isr_index * 0x10);

            return (*isr & (1 << isr_bit)) != 0;
        }

    }

    return false;
}

uint8_t apic_init_ioapic(const acpi_table_madt_entry_t* ioapic) {
    uint64_t ioapic_base = ioapic->ioapic.address;

    PRINTLOG(IOAPIC, LOG_DEBUG, "address is 0x%08llx", ioapic_base);


    frame_t* ioapic_frames = frame_get_allocator()->get_reserved_frames_of_address(frame_get_allocator(), (void*)ioapic_base);

    if(ioapic_frames == NULL) {
        PRINTLOG(APIC, LOG_DEBUG, "cannot find frames of ioapic 0x%016llx", ioapic_base);
        frame_t tmp_ioapic_frm = {ioapic_base, 1, FRAME_TYPE_RESERVED, FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED};

        if(frame_get_allocator()->reserve_system_frames(frame_get_allocator(), &tmp_ioapic_frm) != 0) {
            PRINTLOG(APIC, LOG_ERROR, "cannot reserve frames of ioapic 0x%016llx", ioapic_base);

            return -1;
        }

        memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(tmp_ioapic_frm.frame_address), &tmp_ioapic_frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

        ioapic_base = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(ioapic_base);

        PRINTLOG(APIC, LOG_DEBUG, "ioapic address mapped to 0x%016llx", ioapic_base);

    } else if((ioapic_frames->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) != FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) {
        PRINTLOG(APIC, LOG_TRACE, "frames of ioapic 0x%016llx is 0x%llx 0x%llx", ioapic_base, ioapic_frames->frame_address, ioapic_frames->frame_count);

        memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(ioapic_frames->frame_address), ioapic_frames, MEMORY_PAGING_PAGE_TYPE_NOEXEC);

        ioapic_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;

        ioapic_base = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(ioapic_base);
    }

    ioapic_bases[ioapic_count++] = ioapic_base;

    __volatile__ apic_ioapic_register_t* io_apic_r = (__volatile__ apic_ioapic_register_t*)ioapic_base;

    io_apic_r->selector = APIC_IOAPIC_REGISTER_IDENTIFICATION;
    io_apic_r->value = (ioapic->ioapic.ioapic_id & 0xF) << 24;

    io_apic_r->selector = APIC_IOAPIC_REGISTER_VERSION;

    PRINTLOG(IOAPIC, LOG_DEBUG, "version 0x%02x", io_apic_r->value & 0xFF);

    uint8_t max_r_e = APIC_IOAPIC_MAX_REDIRECTION_ENTRY(io_apic_r->value);

    for(uint8_t i = 0; i < max_r_e; i++) {
        uint8_t intnum = interrupt_get_next_empty_interrupt();
        io_apic_r->selector = APIC_IOAPIC_REGISTER_IRQ_BASE + 2 * i;
        io_apic_r->value = intnum | APIC_IOAPIC_INTERRUPT_DISABLED;
        io_apic_r->selector = APIC_IOAPIC_REGISTER_IRQ_BASE + 2 * i + 1;
        io_apic_r->value = 0;

        PRINTLOG(IOAPIC, LOG_DEBUG, "irq 0x%02x mapped to 0x%02x", i, intnum);
    }

    return max_r_e;
}

int8_t apic_ioapic_setup_irq(uint8_t irq, uint32_t props) {
    uint8_t base_irq = INTERRUPT_IRQ_BASE;

    // props |= APIC_IOAPIC_DESTINATION_MODE_LOGICAL;
    uint32_t apic_id = apic_get_local_apic_id();
    uint32_t dest = apic_id;
    dest = dest << 24;

    for(uint8_t i = 0; i < ioapic_count; i++) {
        __volatile__ apic_ioapic_register_t* io_apic_r = (__volatile__ apic_ioapic_register_t*)ioapic_bases[i];

        io_apic_r->selector = APIC_IOAPIC_REGISTER_VERSION;
        uint8_t max_r_e = APIC_IOAPIC_MAX_REDIRECTION_ENTRY(io_apic_r->value);

        if(irq >= max_r_e) {
            irq -= max_r_e;
            base_irq += max_r_e;
            continue;
        }

        io_apic_r->selector = APIC_IOAPIC_REGISTER_IRQ_BASE + 2 * irq;
        io_apic_r->value = (base_irq + irq) | props;
        io_apic_r->selector = APIC_IOAPIC_REGISTER_IRQ_BASE + 2 * irq + 1;
        io_apic_r->value = dest;

        PRINTLOG(IOAPIC, LOG_DEBUG, "irq 0x%02x mapped to 0x%02x", irq, base_irq + irq);

        return 0;
    }

    return -1;
}

int8_t apic_ioapic_switch_irq(uint8_t irq, uint32_t disabled){
    uint8_t base_irq = INTERRUPT_IRQ_BASE;

    for(uint8_t i = 0; i < ioapic_count; i++) {
        __volatile__ apic_ioapic_register_t* io_apic_r = (__volatile__ apic_ioapic_register_t*)ioapic_bases[i];

        io_apic_r->selector = APIC_IOAPIC_REGISTER_VERSION;
        uint8_t max_r_e = APIC_IOAPIC_MAX_REDIRECTION_ENTRY(io_apic_r->value);

        if(irq >= max_r_e) {
            irq -= max_r_e;
            base_irq += max_r_e;

            continue;
        }

        io_apic_r->selector = APIC_IOAPIC_REGISTER_IRQ_BASE + 2 * irq;
        if(disabled) {
            io_apic_r->value |= APIC_IOAPIC_INTERRUPT_DISABLED;
        } else {
            io_apic_r->value &= ~APIC_IOAPIC_INTERRUPT_DISABLED;
        }

        PRINTLOG(IOAPIC, LOG_DEBUG, "irq 0x%02x %s", irq, disabled == 0 ? "enabled" : "disabled");

        return 0;
    }

    return -1;
}


void  apic_eoi(void) {
    if(apic_enabled) {
        if(apic_x2apic) {
            cpu_write_msr(APIC_X2APIC_MSR_EOI, 0);
        } else {
            volatile uint32_t* eio = (volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_EOI);
            *eio = 0;
        }
    }
}

uint32_t apic_get_local_apic_id(void) {
    if(local_apic_id_is_valid) {
        return cpu_state->local_apic_id;
    } else if(apic_enabled) {
        if(apic_x2apic) {
            uint64_t msr = cpu_read_msr(APIC_X2APIC_MSR_APICID);
            return msr & 0xFFFFFFFF;
        } else {
            volatile uint32_t* id = (volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_ID);
            return (*id >> 24) & 0xFF;
        }
    } else {
        cpu_cpuid_regs_t query = {1, 0, 0, 0};
        cpu_cpuid_regs_t answer = {0};
        cpu_cpuid(query, &answer);
        return answer.ebx >> 24;
    }

    return 0;
}

void apic_send_init(uint8_t destination) {
    if(apic_enabled) {
        if(apic_x2apic) {
            cpu_write_msr(APIC_X2APIC_MSR_ICR,
                          (uint64_t)destination << 32 |
                          APIC_ICR_DELIVERY_MODE_INIT | APIC_ICR_LEVEL_ASSERT | APIC_ICR_TRIGGER_MODE_EDGE | APIC_ICR_DESTINATION_MODE_PHYSICAL | APIC_ICR_DELIVERY_STATUS_IDLE);

            while(cpu_read_msr(APIC_X2APIC_MSR_ICR) & APIC_ICR_DELIVERY_STATUS_SEND_PENDING);
        } else {
            volatile uint32_t* icr_high = (volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_ICR_HIGH);
            volatile uint32_t* icr_low = (volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_ICR_LOW);

            *icr_high = destination << 24;
            *icr_low = APIC_ICR_DELIVERY_MODE_INIT | APIC_ICR_LEVEL_ASSERT | APIC_ICR_TRIGGER_MODE_EDGE | APIC_ICR_DESTINATION_MODE_PHYSICAL | APIC_ICR_DELIVERY_STATUS_IDLE;

            while(*icr_low & APIC_ICR_DELIVERY_STATUS_SEND_PENDING);
        }
    }
}

void apic_send_sipi(uint8_t destination, uint8_t vector) {
    if(apic_enabled) {
        if(apic_x2apic) {
            cpu_write_msr(APIC_X2APIC_MSR_ICR,
                          (uint64_t)destination << 32 |
                          APIC_ICR_DELIVERY_MODE_STARTUP | APIC_ICR_LEVEL_ASSERT | APIC_ICR_TRIGGER_MODE_EDGE | APIC_ICR_DESTINATION_MODE_PHYSICAL | APIC_ICR_DELIVERY_STATUS_IDLE |
                          vector);

            while(cpu_read_msr(APIC_X2APIC_MSR_ICR) & APIC_ICR_DELIVERY_STATUS_SEND_PENDING);
        } else {
            volatile uint32_t* icr_high = (volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_ICR_HIGH);
            volatile uint32_t* icr_low = (volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_ICR_LOW);

            *icr_high = destination << 24;
            *icr_low = APIC_ICR_DELIVERY_MODE_STARTUP | APIC_ICR_LEVEL_ASSERT | APIC_ICR_TRIGGER_MODE_EDGE | APIC_ICR_DESTINATION_MODE_PHYSICAL | APIC_ICR_DELIVERY_STATUS_IDLE |
                       vector;

            while(*icr_low & APIC_ICR_DELIVERY_STATUS_SEND_PENDING);
        }
    }
}

void apic_send_ipi(uint8_t destination, uint8_t vector, boolean_t wait) {
    if(apic_enabled) {
        if(apic_x2apic) {
            cpu_write_msr(APIC_X2APIC_MSR_ICR,
                          (uint64_t)destination << 32 |
                          APIC_ICR_DELIVERY_MODE_FIXED | APIC_ICR_LEVEL_ASSERT | APIC_ICR_TRIGGER_MODE_EDGE | APIC_ICR_DESTINATION_MODE_PHYSICAL | APIC_ICR_DELIVERY_STATUS_IDLE |
                          vector);

            if(wait) while(cpu_read_msr(APIC_X2APIC_MSR_ICR) & APIC_ICR_DELIVERY_STATUS_SEND_PENDING);
        } else {
            volatile uint32_t* icr_high = (volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_ICR_HIGH);
            volatile uint32_t* icr_low = (volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_ICR_LOW);

            *icr_high = destination << 24;
            *icr_low = APIC_ICR_DELIVERY_MODE_FIXED | APIC_ICR_LEVEL_ASSERT | APIC_ICR_TRIGGER_MODE_EDGE | APIC_ICR_DESTINATION_MODE_PHYSICAL | APIC_ICR_DELIVERY_STATUS_IDLE |
                       vector;

            if(wait) while(*icr_low & APIC_ICR_DELIVERY_STATUS_SEND_PENDING);
        }
    }
}

void apic_send_nmi(uint8_t destination) {
    if(apic_enabled) {
        if(apic_x2apic) {
            cpu_write_msr(APIC_X2APIC_MSR_ICR,
                          (uint64_t)destination << 32 |
                          APIC_ICR_DELIVERY_MODE_NMI | APIC_ICR_LEVEL_ASSERT | APIC_ICR_TRIGGER_MODE_EDGE | APIC_ICR_DESTINATION_MODE_PHYSICAL | APIC_ICR_DELIVERY_STATUS_IDLE);

            while(cpu_read_msr(APIC_X2APIC_MSR_ICR) & APIC_ICR_DELIVERY_STATUS_SEND_PENDING);
        } else {
            volatile uint32_t* icr_high = (volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_ICR_HIGH);
            volatile uint32_t* icr_low = (volatile uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_ICR_LOW);

            *icr_high = destination << 24;
            *icr_low = APIC_ICR_DELIVERY_MODE_NMI | APIC_ICR_LEVEL_ASSERT | APIC_ICR_TRIGGER_MODE_EDGE | APIC_ICR_DESTINATION_MODE_PHYSICAL | APIC_ICR_DELIVERY_STATUS_IDLE;

            while(*icr_low & APIC_ICR_DELIVERY_STATUS_SEND_PENDING);
        }
    }
}

uint64_t apic_get_ap_count(void) {
    uint64_t ap_count = 0;

    uint8_t lcl_apic_id = apic_get_local_apic_id();

    acpi_sdt_header_t* madt = acpi_get_table(ACPI_CONTEXT->xrsdp_desc, "APIC");

    list_t* apic_entries = acpi_get_apic_table_entries(madt);

    iterator_t* iter = list_iterator_create(apic_entries);

    while(iter->end_of_iterator(iter) != 0) {
        const acpi_table_madt_entry_t* e = iter->get_item(iter);

        if(e->info.type == ACPI_MADT_ENTRY_TYPE_PROCESSOR_LOCAL_APIC) {
            uint8_t apic_id = e->processor_local_apic.apic_id;

            if (apic_id != lcl_apic_id) {
                ap_count++;
            }
        }

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    return ap_count;
}

int32_t apic_get_first_irr_interrupt(void) {
    if(apic_enabled) {
        if(apic_x2apic) {
            uint64_t irr = 0;
            uint64_t intnum = 0;
            uint64_t intbase = 0;
            uint64_t msrbase = APIC_X2APIC_MSR_IRR7;

            for(int32_t i = 0; i < 8; i++) {
                irr = cpu_read_msr(msrbase);

                if(find_lsb(irr, &intnum)) {
                    return intbase + intnum;
                }

                intbase += 0x20;
                msrbase--; // x2apic irr step is 1
            }

            return -1;
        } else {
            uint64_t intnum = 0;
            uint64_t intbase = 0;
            uint64_t irrbase = APIC_X2APIC_MSR_IRR7; // irr needs to be read from 7 to 0 bigger is higher priority

            for(int32_t i = 0; i < 8; i++) {
                volatile uint32_t* irr = (volatile uint32_t*)(lapic_addr + irrbase);

                if(find_lsb(*irr, &intnum)) {
                    return intbase + intnum;
                }

                intbase += 0x20;
                irrbase -= 0x10; // apic irr step is 0x10
            }

            return -1;
        }
    }

    return -1;
}

int32_t apic_get_isr_interrupt(void) {
    if(apic_enabled) {
        if(apic_x2apic) {
            uint64_t irr = 0;
            uint64_t intnum = 0;
            uint64_t intbase = 0;
            uint64_t msrbase = APIC_X2APIC_MSR_ISR0;


            for(int32_t i = 0; i < 8; i++) {
                irr = cpu_read_msr(msrbase);

                if(find_lsb(irr, &intnum)) {
                    return intbase + intnum;
                }

                intbase += 0x20;
                msrbase++; // x2apic isr step is 1
            }

            return -1;
        } else {
            uint64_t intnum = 0;
            uint64_t intbase = 0;
            uint64_t isrbase = APIC_REGISTER_OFFSET_ISR0;

            for(int32_t i = 0; i < 8; i++) {
                volatile uint32_t* irr = (volatile uint32_t*)(lapic_addr + isrbase);

                if(find_lsb(*irr, &intnum)) {
                    return intbase + intnum;
                }

                intbase += 0x20;
                isrbase += 0x10; // apic isr step is 0x10
            }

            return -1;
        }
    }

    return -1;
}
