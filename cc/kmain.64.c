#include <types.h>
#include <helloworld.h>
#include <video.h>
#include <memory.h>
#include <memory/paging.h>
#include <strings.h>
#include <cpu/interrupt.h>
#include <acpi.h>
#include <pci.h>
#include <iterator.h>
#include <ports.h>
#include <apic.h>
#include <linkedlist.h>
#include <cpu.h>

uint8_t kmain64() {
	memory_heap_t* heap = memory_create_heap_simple(0, 0);
	memory_set_default_heap(heap);
	memory_page_table_t* p4 = memory_paging_clone_pagetable();
	memory_paging_switch_table(p4);

	interrupt_init();
	video_clear_screen();
	char_t* data = hello_world();
	video_print(data);

	acpi_xrsdp_descriptor_t* desc = acpi_find_xrsdp();
	if(desc == NULL) {
		video_print("acpi header not found or incorrect checksum\n\0");
	} else{
		video_print("acpi header is ok\n\0");

		acpi_sdt_header_t* madt = acpi_get_table(desc, "APIC");
		if(madt == NULL) {
			video_print("can not find madt or incorrect checksum\n\0");
		} else {
			video_print("madt is found\n\0");
			linkedlist_t apic_entries = acpi_get_apic_table_entries(madt);
			if(apic_init_apic(apic_entries) != 0) {
				video_print("cannot enable apic\n\0");
				return -2;
			} else {
				video_print("apic and ioapic enabled\n\0");
			}
		}

		acpi_table_fadt_t* fadt = (acpi_table_fadt_t*)acpi_get_table(desc, "FACP");
		if(fadt == NULL) {
			video_print("fadt not found\n\0");
			return -1;
		}
		int8_t acpi_enabled = -1;
		if(fadt->smi_command_port == 0) {
			video_print("acpi command port is 0. \0");
			acpi_enabled = 0;
		}
		if(fadt->acpi_enable == 0 && fadt->acpi_disable == 0) {
			video_print("acpi enable/disable is 0. \0");
			acpi_enabled = 0;
		}
		uint16_t pm_1a_port = fadt->pm_1a_control_block_address_64bit.address;
		uint32_t pm_1a_value = inw(pm_1a_port);
		if((pm_1a_value & 0x1) == 0x1) {
			video_print("pm 1a control block acpi en is setted\0");
			acpi_enabled = 0;
		}
		if(acpi_enabled != 0) {
			outb(fadt->smi_command_port, fadt->acpi_enable);
			while((inw(pm_1a_port) & 0x1) != 0x1);
			video_print("acpi enabled");
		}
		video_print("\n\0");

		acpi_table_mcfg_t* mcfg = (acpi_table_mcfg_t*)acpi_get_table(desc, "MCFG");
		if(mcfg == NULL) {
			video_print("can not find mcfg or incorrect checksum\n\0");
		} else {
			video_print("cmfg is found\n\0");
			char_t* mcfg_addr = itoh((size_t)mcfg);
			video_print(mcfg_addr);
			video_print("\n\0");
			memory_free(mcfg_addr);

			iterator_t* iter = pci_iterator_create(mcfg);
			while(iter->end_of_iterator(iter) != 0) {
				video_print("pci dev found  \0");
				pci_dev_t* p = iter->get_item(iter);

				data = itoh(p->group_number);
				video_print(data);
				memory_free(data);
				video_print(":\0");

				data = itoh(p->bus_number);
				video_print(data);
				memory_free(data);
				video_print(":\0");

				data = itoh(p->device_number);
				video_print(data);
				memory_free(data);
				video_print(".\0");

				data = itoh(p->function_number);
				video_print(data);
				memory_free(data);
				video_print(" -> \0");

				pci_common_header_t* ph = p->pci_header;

				data = itoh(ph->vendor_id);
				video_print(data);
				memory_free(data);
				video_print(":\0");

				data = itoh(ph->device_id);
				video_print(data);
				memory_free(data);
				video_print(" -> \0");

				data = itoh(ph->class_code);
				video_print(data);
				memory_free(data);
				video_print(":\0");

				data = itoh(ph->subclass_code);
				video_print(data);
				memory_free(data);

				if(ph->header_type.header_type == 0x0) {
					video_print(" -> ");
					pci_generic_device_t* pg = (pci_generic_device_t*)ph;

					data = itoh(pg->interrupt_line);
					video_print(data);
					memory_free(data);

				}

				video_print("\n\0");
				memory_free(p);

				iter = iter->next(iter);
			}
			iter->destroy(iter);

		}
	}

	video_print("tests completed!...\n\0");
	outl(0x4048, 0x200);
	outl(0x404C, 0xBADC0DE);
	uint32_t test_data = 0;
	char_t* test_str;
	for(size_t i = 0; i <= 30; i++) {
		outl(0x4048, 4 * i);
		test_data = inl(0x404C);
		test_str = itoh(test_data);
		video_print(test_str);
		memory_free(test_str);
		video_print("  ");
	}
	return 0;
}
