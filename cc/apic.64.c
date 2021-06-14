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

uint8_t apic_init_ioapic(acpi_table_madt_entry_t* ioapic, uint8_t base_irq);

int8_t apic_init_apic(linkedlist_t apic_entries){
	uint64_t apic_msr = cpu_read_msr(APIC_MSR_ADDRESS);

	if((apic_msr & APIC_MSR_ENABLE_APIC) != APIC_MSR_ENABLE_APIC) {
		apic_msr |= APIC_MSR_ENABLE_APIC;
		cpu_write_msr(APIC_MSR_ADDRESS, APIC_MSR_ENABLE_APIC);
	}

	uint8_t irq_base = INTERRUPT_IRQ_BASE;
	acpi_table_madt_entry_t* la = NULL;

	iterator_t* iter = linkedlist_iterator_create(apic_entries);

	while(iter->end_of_iterator(iter) != 0) {
		acpi_table_madt_entry_t* e = iter->get_item(iter);

		if(e->info.type == ACPI_MADT_ENTRY_TYPE_LOCAL_APIC_ADDRESS_OVERRIDE) {
			la = e;
		}

		if(e->info.type == ACPI_MADT_ENTRY_TYPE_IOAPIC) {
			irq_base += apic_init_ioapic(e, irq_base);
		}

		if(e->info.type == ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE) {
			printf("APIC: source irq 0x%02x is at gsi 0x%02x\n", e->interrupt_source_override.irq_source, e->interrupt_source_override.global_system_interrupt);
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);

	uint64_t lapic_addr = 0;

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

	return 0;
}

uint8_t apic_init_ioapic(acpi_table_madt_entry_t* ioapic, uint8_t base_irq) {
	uint64_t ioapic_base = ioapic->ioapic.address;

	printf("IOAPIC: address is 0x%08x\n", ioapic_base);

	memory_paging_add_page(ioapic_base, ioapic_base, MEMORY_PAGING_PAGE_TYPE_4K);

	__volatile__ apic_ioapic_register_t* io_apic_r = (__volatile__ apic_ioapic_register_t*)ioapic_base;

	io_apic_r->selector = APIC_IOAPIC_REGISTER_IDENTIFICATION;
	io_apic_r->value = (ioapic->ioapic.ioapic_id & 0xF) << 24;

	io_apic_r->selector = APIC_IOAPIC_REGISTER_VERSION;

	printf("IOAPIC: version 0x%02x\n", io_apic_r->value & 0xFF);

	uint8_t max_r_e = APIC_IOAPIC_MAX_REDIRECTION_ENTRY(io_apic_r->value);

	for(uint8_t i = 0; i < max_r_e; i++) {
		io_apic_r->selector = APIC_IOAPIC_REGISTER_IRQ_BASE + 2 * i;
		io_apic_r->value = (base_irq + i) | APIC_IOAPIC_INTERRUPT_DISABLED;
		io_apic_r->selector = APIC_IOAPIC_REGISTER_IRQ_BASE + 2 * i + 1;
		io_apic_r->value = 0;

		printf("IOAPIC: irq 0x%02x mapped to 0x%02x\n", i, base_irq + i);
	}

	return max_r_e;
}
