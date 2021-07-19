/**
 * @file apic.64.c
 * @brief apic and ioapic implementation
 */
#include <apic.h>
#include <cpu.h>
#include <memory/paging.h>
#include <acpi.h>
#include <iterator.h>
#include <cpu/interrupt.h>
#include <video.h>
#include <time/timer.h>
#include <linkedlist.h>

uint8_t apic_init_ioapic(acpi_table_madt_entry_t* ioapic);
void apic_init_timer();

uint64_t ioapic_bases[2] = {0, 0};
uint8_t ioapic_count = 0;
uint64_t lapic_addr = 0;
int8_t apic_enabled = 0;

linkedlist_t irq_remappings = NULL;

int8_t apic_init_apic(linkedlist_t apic_entries){
	cpu_cpuid_regs_t query = {0x80000001, 0, 0, 0};
	cpu_cpuid_regs_t answer = {0, 0, 0, 0};
	if(cpu_cpuid(query, &answer) != 0) {
		printf("APIC: Fatal cannot read cpuid\n");
		return -1;
	}


	irq_remappings = linkedlist_create_list();
	if(irq_remappings == NULL) {
		return -1;
	}

	uint64_t apic_enable_flag = APIC_MSR_ENABLE_APIC;
	if(answer.ecx & (1 << 21)) {
		printf("APIC: x2apic found\n");
		// apic_enable_flag |= APIC_MSR_ENABLE_X2APIC; // TODO: x2apic needs rdmsr wrmsr
	} else {
		printf("APIC: apic found\n");
	}

	uint64_t apic_msr = cpu_read_msr(APIC_MSR_ADDRESS);

	if((apic_msr & APIC_MSR_ENABLE_APIC) != APIC_MSR_ENABLE_APIC) {
		apic_msr |= apic_enable_flag;
		cpu_write_msr(APIC_MSR_ADDRESS, apic_msr);
	}

	acpi_table_madt_entry_t* la = NULL;


	iterator_t* iter = linkedlist_iterator_create(apic_entries);

	while(iter->end_of_iterator(iter) != 0) {
		acpi_table_madt_entry_t* e = iter->get_item(iter);

		if(e->info.type == ACPI_MADT_ENTRY_TYPE_LOCAL_APIC_ADDRESS_OVERRIDE) {
			la = e;
		}

		if(e->info.type == ACPI_MADT_ENTRY_TYPE_IOAPIC) {
			apic_init_ioapic(e);
		}

		if(e->info.type == ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE) {
			linkedlist_list_insert(irq_remappings, e);
			printf("APIC: source irq 0x%02x is at gsi 0x%02x\n", e->interrupt_source_override.irq_source, e->interrupt_source_override.global_system_interrupt);
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);

	if(la == NULL) {
		la = linkedlist_get_data_at_position(apic_entries, 0);
		lapic_addr = la->local_apic_address.address;
	} else {
		lapic_addr = la->local_apic_address_override.address;
	}

	printf("APIC: local apic address is: 0x%08x\n", lapic_addr);

	memory_paging_add_page(lapic_addr, lapic_addr, MEMORY_PAGING_PAGE_TYPE_4K);

	apic_register_spurious_interrupt_t* ar_si = (apic_register_spurious_interrupt_t*)(lapic_addr + APIC_REGISTER_OFFSET_SPURIOUS_INTERRUPT);
	ar_si->vector = INTERRUPT_VECTOR_SPURIOUS;
	ar_si->apic_software_enable = 1;

	apic_enabled = 1;

	apic_init_timer();

	return 0;
}

void apic_init_timer() {
	uint8_t timer_irq = 0;

	iterator_t* iter = linkedlist_iterator_create(irq_remappings);

	while(iter->end_of_iterator(iter) != 0) {
		acpi_table_madt_entry_t* e = iter->get_item(iter);

		if(e->interrupt_source_override.irq_source == timer_irq) {
			timer_irq = e->interrupt_source_override.global_system_interrupt;
			break;
		}
	}

	iter->destroy(iter);

	time_timer_pit_set_hz(TIME_TIMER_PIT_HZ_FOR_1MS);
	interrupt_irq_set_handler(timer_irq, &time_timer_pit_isr);
	apic_ioapic_setup_irq(timer_irq,
	                      APIC_IOAPIC_INTERRUPT_ENABLED
	                      | APIC_IOAPIC_DELIVERY_MODE_FIXED | APIC_IOAPIC_DELIVERY_STATUS_RELAX
	                      | APIC_IOAPIC_DESTINATION_MODE_PHYSICAL
	                      | APIC_IOAPIC_TRIGGER_MODE_EDGE | APIC_IOAPIC_PIN_POLARITY_ACTIVE_HIGH);


	uint32_t* timer_divier = (uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_TIMER_DIVIDER);
	*timer_divier = 0x3;

	uint32_t* timer_lvt = (uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_TIMER_LVT);

	uint32_t* timer_init = (uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_TIMER_INITIAL_VALUE);
	uint32_t* timer_curr = (uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_TIMER_CURRENT_VALUE);

	uint32_t total_inits = 0;

	for(uint8_t i = 0; i < 10; i++) {
		*timer_init = 0xFFFFFFFF;

		time_timer_pit_sleep(100);

		*timer_lvt ^= APIC_INTERRUPT_DISABLED;

		total_inits += (0xFFFFFFFF - *timer_curr) / 100;
	}

	*timer_init = total_inits / 10;

	interrupt_irq_set_handler(0x0, &time_timer_apic_isr);

	*timer_lvt = APIC_TIMER_PERIODIC | APIC_INTERRUPT_ENABLED | 0x20;


	apic_ioapic_disable_irq(timer_irq);
	time_timer_reset_tick_count();
	time_timer_configure_spinsleep();
}

uint8_t apic_init_ioapic(acpi_table_madt_entry_t* ioapic) {
	uint64_t ioapic_base = ioapic->ioapic.address;

	ioapic_bases[ioapic_count++] = ioapic_base;

	printf("IOAPIC: address is 0x%08x\n", ioapic_base);

	memory_paging_add_page(ioapic_base, ioapic_base, MEMORY_PAGING_PAGE_TYPE_4K);

	__volatile__ apic_ioapic_register_t* io_apic_r = (__volatile__ apic_ioapic_register_t*)ioapic_base;

	io_apic_r->selector = APIC_IOAPIC_REGISTER_IDENTIFICATION;
	io_apic_r->value = (ioapic->ioapic.ioapic_id & 0xF) << 24;

	io_apic_r->selector = APIC_IOAPIC_REGISTER_VERSION;

	printf("IOAPIC: version 0x%02x\n", io_apic_r->value & 0xFF);

	uint8_t max_r_e = APIC_IOAPIC_MAX_REDIRECTION_ENTRY(io_apic_r->value);

	for(uint8_t i = 0; i < max_r_e; i++) {
		uint8_t intnum = interrupt_get_next_empty_interrupt();
		io_apic_r->selector = APIC_IOAPIC_REGISTER_IRQ_BASE + 2 * i;
		io_apic_r->value = intnum | APIC_IOAPIC_INTERRUPT_DISABLED;
		io_apic_r->selector = APIC_IOAPIC_REGISTER_IRQ_BASE + 2 * i + 1;
		io_apic_r->value = 0;

		printf("IOAPIC: irq 0x%02x mapped to 0x%02x\n", i, intnum);
	}

	return max_r_e;
}

int8_t apic_ioapic_setup_irq(uint8_t irq, uint32_t props) {
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
		io_apic_r->value = (base_irq + irq) | props;
		io_apic_r->selector = APIC_IOAPIC_REGISTER_IRQ_BASE + 2 * irq + 1;
		io_apic_r->value = 0;

		printf("IOAPIC: irq 0x%02x mapped to 0x%02x\n", irq, base_irq + irq);

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
			io_apic_r->value |= disabled;
		} else {
			io_apic_r->value ^= disabled;
		}

		printf("IOAPIC: irq 0x%02x %s\n", irq, disabled == 0 ? "enabled" : "disabled");

		return 0;
	}

	return -1;
}


void  apic_eoi() {
	if(apic_enabled) {
		uint32_t* eio = (uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_EOI);
		*eio = 0;
	}
}
