/**
 * @file apic.64.c
 * @brief apic and ioapic implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <apic.h>
#include <cpu.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <acpi.h>
#include <iterator.h>
#include <cpu/interrupt.h>
#include <video.h>
#include <time/timer.h>
#include <linkedlist.h>

uint8_t apic_init_ioapic(acpi_table_madt_entry_t* ioapic);
int8_t apic_init_timer();

uint64_t ioapic_bases[2] = {0, 0};
uint8_t ioapic_count = 0;
uint64_t lapic_addr = 0;
int8_t apic_enabled = 0;

linkedlist_t irq_remappings = NULL;

int8_t apic_setup(acpi_xrsdp_descriptor_t* desc) {
	acpi_sdt_header_t* madt = acpi_get_table(desc, "APIC");


	PRINTLOG(APIC, LOG_INFO, "apic and ioapic initialization");

	if(madt == NULL) {
		PRINTLOG(APIC, LOG_ERROR, "can not find madt or incorrect checksum");
		return -1;
	}

	PRINTLOG(APIC, LOG_DEBUG, "madt is found");

	linkedlist_t apic_entries = acpi_get_apic_table_entries(madt);

	if(apic_init_apic(apic_entries) != 0) {
		PRINTLOG(APIC, LOG_ERROR, "cannot enable apic");

		return -1;
	}

	PRINTLOG(APIC, LOG_INFO, "apic and ioapic enabled");

	return 0;
}

int8_t apic_init_apic(linkedlist_t apic_entries){
	cpu_cpuid_regs_t query = {0x80000001, 0, 0, 0};
	cpu_cpuid_regs_t answer = {0, 0, 0, 0};

	if(cpu_cpuid(query, &answer) != 0) {
		PRINTLOG(APIC, LOG_DEBUG, "Fatal cannot read cpuid");

		return -1;
	}


	irq_remappings = linkedlist_create_list();
	if(irq_remappings == NULL) {
		return -1;
	}

	uint64_t apic_enable_flag = APIC_MSR_ENABLE_APIC;
	if(answer.ecx & (1 << 21)) {
		PRINTLOG(APIC, LOG_DEBUG, "x2apic found");
		// apic_enable_flag |= APIC_MSR_ENABLE_X2APIC; // TODO: x2apic needs rdmsr wrmsr
	} else {
		PRINTLOG(APIC, LOG_DEBUG, "apic found");
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
			PRINTLOG(APIC, LOG_DEBUG, "source irq 0x%02x is at gsi 0x%02x", e->interrupt_source_override.irq_source, e->interrupt_source_override.global_system_interrupt);
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

	frame_t* lapic_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, (void*)lapic_addr);

	if(lapic_frames == NULL) {
		PRINTLOG(APIC, LOG_DEBUG, "cannot find frames of lapic 0x%016llx", lapic_addr);
		frame_t tmp_lapic_frm = {lapic_addr, 1, FRAME_TYPE_RESERVED, FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED};

		if(KERNEL_FRAME_ALLOCATOR->reserve_system_frames(KERNEL_FRAME_ALLOCATOR, &tmp_lapic_frm) != 0) {
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

	apic_register_spurious_interrupt_t* ar_si = (apic_register_spurious_interrupt_t*)(lapic_addr + APIC_REGISTER_OFFSET_SPURIOUS_INTERRUPT);
	ar_si->vector = INTERRUPT_VECTOR_SPURIOUS;
	ar_si->apic_software_enable = 1;

	apic_enabled = 1;

	return apic_init_timer();
}

uint8_t apic_get_irq_override(uint8_t old_irq){
	iterator_t* iter = linkedlist_iterator_create(irq_remappings);

	while(iter->end_of_iterator(iter) != 0) {
		acpi_table_madt_entry_t* e = iter->get_item(iter);

		if(e->interrupt_source_override.irq_source == old_irq) {
			old_irq = e->interrupt_source_override.global_system_interrupt;
			break;
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);

	return old_irq;
}

int8_t apic_init_timer() {
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

	PRINTLOG(APIC, LOG_DEBUG, "apic timer configuration started");

	if(interrupt_irq_set_handler(0x0, &time_timer_apic_isr) != 0) {
		PRINTLOG(APIC, LOG_ERROR, "cannot set apic timer irq");

		return -1;
	}

	*timer_lvt = APIC_TIMER_PERIODIC | APIC_INTERRUPT_ENABLED | 0x20;

	PRINTLOG(APIC, LOG_DEBUG, "apic timer initialized");

	apic_ioapic_disable_irq(timer_irq);
	interrupt_irq_remove_handler(timer_irq, &time_timer_pit_isr);
	PRINTLOG(APIC, LOG_DEBUG, "pic timer disabled");

	time_timer_reset_tick_count();
	time_timer_configure_spinsleep();

	return 0;
}

uint8_t apic_init_ioapic(acpi_table_madt_entry_t* ioapic) {
	uint64_t ioapic_base = ioapic->ioapic.address;

	PRINTLOG(IOAPIC, LOG_DEBUG, "address is 0x%08llx", ioapic_base);


	frame_t* ioapic_frames = KERNEL_FRAME_ALLOCATOR->get_reserved_frames_of_address(KERNEL_FRAME_ALLOCATOR, (void*)ioapic_base);

	if(ioapic_frames == NULL) {
		PRINTLOG(APIC, LOG_DEBUG, "cannot find frames of ioapic 0x%016llx", ioapic_base);
		frame_t tmp_ioapic_frm = {ioapic_base, 1, FRAME_TYPE_RESERVED, FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED};

		if(KERNEL_FRAME_ALLOCATOR->reserve_system_frames(KERNEL_FRAME_ALLOCATOR, &tmp_ioapic_frm) != 0) {
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


void  apic_eoi() {
	if(apic_enabled) {
		uint32_t* eio = (uint32_t*)(lapic_addr + APIC_REGISTER_OFFSET_EOI);
		*eio = 0;
	}
}
