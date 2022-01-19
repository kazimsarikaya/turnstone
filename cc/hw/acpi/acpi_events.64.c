/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <acpi.h>
#include <acpi/aml_resource.h>
#include <apic.h>
#include <cpu.h>
#include <cpu/interrupt.h>
#include <ports.h>
#include <video.h>

int8_t acpi_events_isr(interrupt_frame_t* frame, uint8_t intnum){
	UNUSED(frame);
	UNUSED(intnum);

	boolean_t os_poweroff = 0;
	boolean_t os_reset = 0;
	boolean_t irq_handled = 0;

	if(ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.address) {
		PRINTLOG(ACPI, LOG_DEBUG, "acpi pm1a address %i 0x%lx %i", ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.address_space, ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.address, ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.bit_width);

		if(ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_MEMORY) {

		} else if(ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_IO) {
			uint16_t pm1a_port = (uint16_t)ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.address;
			uint16_t value = inw(pm1a_port);
			if(value) {
				PRINTLOG(ACPI, LOG_DEBUG, "pm1a event 0x%04x", value);

				if(value & 0x100) {
					PRINTLOG(ACPI, LOG_DEBUG, "pm1a event is poweroff", 0);
					os_poweroff = 1;
				} else if(value & 0x200) {
					PRINTLOG(ACPI, LOG_DEBUG, "pm1a event is reset", 0);
					os_reset = 1;
				} else {
					PRINTLOG(ACPI, LOG_ERROR, "pm1a event is unknown 0x%04x", value);
				}

				outw(pm1a_port, 0xFFFF);
			}
		} else {
			PRINTLOG(ACPI, LOG_ERROR, "unknown address type of pm1a", ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.address_space);
		}

		irq_handled = 1;
	}

	if(ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.address) {
		PRINTLOG(ACPI, LOG_DEBUG, "acpi pm1b address %i 0x%lx %i", ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.address_space, ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.address, ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.bit_width);

		if(ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_MEMORY) {

		} else if(ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_IO) {
			uint16_t pm1b_port = (uint16_t)ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.address;
			uint16_t value = inw(pm1b_port);
			if(value) {
				PRINTLOG(ACPI, LOG_DEBUG, "pm1b event 0x%04x", value);

				if(value & 0x100) {
					PRINTLOG(ACPI, LOG_DEBUG, "pm1b event is poweroff", 0);
					os_poweroff = 1;
				} else if(value & 0x200) {
					PRINTLOG(ACPI, LOG_DEBUG, "pm1b event is reset", 0);
					os_reset = 1;
				} else {
					PRINTLOG(ACPI, LOG_ERROR, "pm1b event is unknown 0x%04x", value);
				}

				outw(pm1b_port, 0xFFFF);
			}
		} else {
			PRINTLOG(ACPI, LOG_ERROR, "unknown address type of pm1b", ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.address_space);
		}

		irq_handled = 1;
	}

	if(ACPI_CONTEXT->fadt->gpe0_block_address_64bit.address) {
		PRINTLOG(ACPI, LOG_DEBUG, "acpi gpe0 address %i 0x%lx %i", ACPI_CONTEXT->fadt->gpe0_block_address_64bit.address_space, ACPI_CONTEXT->fadt->gpe0_block_address_64bit.address, ACPI_CONTEXT->fadt->gpe0_block_address_64bit.bit_width);

		if(ACPI_CONTEXT->fadt->gpe0_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_MEMORY) {

		} else if(ACPI_CONTEXT->fadt->gpe0_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_IO) {
			uint16_t gpe0_port = (uint16_t)ACPI_CONTEXT->fadt->gpe0_block_address_64bit.address;
			uint16_t value = inb(gpe0_port);
			if(value) {
				PRINTLOG(ACPI, LOG_ERROR, "gpe0 event 0x%02x", value);
				outb(gpe0_port, 0xFF);
			}
		} else {
			PRINTLOG(ACPI, LOG_ERROR, "unknown address type of gpe0", ACPI_CONTEXT->fadt->gpe0_block_address_64bit.address_space);
		}

		irq_handled = 1;
	}

	if(ACPI_CONTEXT->fadt->gpe1_block_address_64bit.address) {
		PRINTLOG(ACPI, LOG_DEBUG, "acpi gpe1 address %i 0x%lx %i", ACPI_CONTEXT->fadt->gpe1_block_address_64bit.address_space, ACPI_CONTEXT->fadt->gpe1_block_address_64bit.address, ACPI_CONTEXT->fadt->gpe1_block_address_64bit.bit_width);

		if(ACPI_CONTEXT->fadt->gpe1_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_MEMORY) {

		} else if(ACPI_CONTEXT->fadt->gpe1_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_IO) {
			uint16_t gpe1_port = (uint16_t)ACPI_CONTEXT->fadt->gpe1_block_address_64bit.address;
			uint16_t value = inb(gpe1_port);
			if(value) {
				PRINTLOG(ACPI, LOG_ERROR, "gpe1 event 0x%02x", value);
				outb(gpe1_port, 0xFF);
			}
		} else {
			PRINTLOG(ACPI, LOG_ERROR, "unknown address type of gpe1", ACPI_CONTEXT->fadt->gpe1_block_address_64bit.address_space);
		}

		irq_handled = 1;
	}

	if(os_poweroff) {
		acpi_poweroff();
		cpu_hlt();
	}

	if(os_reset) {
		acpi_reset();
		cpu_hlt();
	}

	if(irq_handled) {
		apic_eoi();
		cpu_sti();

		return 0;
	}

	return -1;
}



int8_t acpi_setup_events() {
	PRINTLOG(ACPI, LOG_INFO, "acpi event setup started", 0);

	uint8_t irq = ACPI_CONTEXT->fadt->sci_interrupt;

	PRINTLOG(ACPI, LOG_DEBUG, "acpi old sci irq 0x%02x", irq);

	irq = apic_get_irq_override(irq);

	PRINTLOG(ACPI, LOG_DEBUG, "acpi overrided sci irq 0x%02x", irq);

	interrupt_irq_set_handler(irq, &acpi_events_isr);
	apic_ioapic_setup_irq(irq, APIC_IOAPIC_TRIGGER_MODE_LEVEL);
	apic_ioapic_enable_irq(irq);

	if(ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.address) {
		PRINTLOG(ACPI, LOG_DEBUG, "acpi pm1a address %i 0x%lx %i", ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.address_space, ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.address, ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.bit_width);

		if(ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_MEMORY) {

		} else if(ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_IO) {
			uint16_t pm1a_port = (uint16_t)ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.address;
			outw(pm1a_port, 0xFFFF);
			PRINTLOG(ACPI, LOG_DEBUG, "acpi pm1a status 0x%04x", pm1a_port);
			pm1a_port += ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.bit_width / (8 * 2);
			uint16_t value = inw(pm1a_port);
			value |= 0x310;
			outw(pm1a_port, value);
			PRINTLOG(ACPI, LOG_DEBUG, "acpi pm1a enable 0x%04x", pm1a_port);
		} else {
			PRINTLOG(ACPI, LOG_ERROR, "unknown address type of pm1a", ACPI_CONTEXT->fadt->pm_1a_event_block_address_64bit.address_space);
		}

	}

	if(ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.address) {
		PRINTLOG(ACPI, LOG_DEBUG, "acpi pm1b address %i 0x%lx %i", ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.address_space, ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.address, ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.bit_width);

		if(ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_MEMORY) {

		} else if(ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_IO) {
			uint16_t pm1b_port = (uint16_t)ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.address;
			outw(pm1b_port, 0xFFFF);
			PRINTLOG(ACPI, LOG_DEBUG, "acpi pm1b status 0x%04x", pm1b_port);
			pm1b_port += ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.bit_width / (8 * 2);
			uint16_t value = inw(pm1b_port);
			value |= 0x310;
			outw(pm1b_port, value);
			PRINTLOG(ACPI, LOG_DEBUG, "acpi pm1b enable 0x%04x", pm1b_port);
		} else {
			PRINTLOG(ACPI, LOG_ERROR, "unknown address type of pm1b", ACPI_CONTEXT->fadt->pm_1b_event_block_address_64bit.address_space);
		}

	}

	if(ACPI_CONTEXT->fadt->gpe0_block_address_64bit.address) {
		PRINTLOG(ACPI, LOG_DEBUG, "acpi gpe0 address %i 0x%lx %i", ACPI_CONTEXT->fadt->gpe0_block_address_64bit.address_space, ACPI_CONTEXT->fadt->gpe0_block_address_64bit.address, ACPI_CONTEXT->fadt->gpe0_block_address_64bit.bit_width);

		if(ACPI_CONTEXT->fadt->gpe0_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_MEMORY) {

		} else if(ACPI_CONTEXT->fadt->gpe0_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_IO) {
			uint16_t gpe0_port = (uint16_t)ACPI_CONTEXT->fadt->gpe0_block_address_64bit.address;
			outb(gpe0_port, 0xFF);
			PRINTLOG(ACPI, LOG_DEBUG, "acpi gpe0 status 0x%04x", gpe0_port);
			gpe0_port += ACPI_CONTEXT->fadt->gpe0_block_address_64bit.bit_width / (8 * 2);
			outb(gpe0_port, 0xFF);
			PRINTLOG(ACPI, LOG_DEBUG, "acpi gpe0 enable 0x%04x", gpe0_port);
		} else {
			PRINTLOG(ACPI, LOG_ERROR, "unknown address type of gpe0", ACPI_CONTEXT->fadt->gpe0_block_address_64bit.address_space);
		}
	}

	if(ACPI_CONTEXT->fadt->gpe1_block_address_64bit.address) {
		PRINTLOG(ACPI, LOG_DEBUG, "acpi gpe1 address %i 0x%lx %i", ACPI_CONTEXT->fadt->gpe1_block_address_64bit.address_space, ACPI_CONTEXT->fadt->gpe1_block_address_64bit.address, ACPI_CONTEXT->fadt->gpe1_block_address_64bit.bit_width);

		if(ACPI_CONTEXT->fadt->gpe1_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_MEMORY) {

		} else if(ACPI_CONTEXT->fadt->gpe1_block_address_64bit.address_space == ACPI_AML_RESOURCE_ADDRESS_SPACE_ID_IO) {
			uint16_t gpe1_port = (uint16_t)ACPI_CONTEXT->fadt->gpe1_block_address_64bit.address;
			outb(gpe1_port, 0xFF);
			PRINTLOG(ACPI, LOG_DEBUG, "acpi gpe1 status 0x%04x", gpe1_port);
			gpe1_port += ACPI_CONTEXT->fadt->gpe1_block_address_64bit.bit_width / (8 * 2);
			outb(gpe1_port, 0xFF);
			PRINTLOG(ACPI, LOG_DEBUG, "acpi gpe1 enable 0x%04x", gpe1_port);
		} else {
			PRINTLOG(ACPI, LOG_ERROR, "unknown address type of gpe1", ACPI_CONTEXT->fadt->gpe1_block_address_64bit.address_space);
		}
	}


	PRINTLOG(ACPI, LOG_INFO, "sci interrupt 0x%02x registered", irq)

	return 0;
}
