#include <types.h>
#include <helloworld.h>
#include <video.h>
#include <memory.h>
#include <memory/paging.h>
#include <memory/mmap.h>
#include <systeminfo.h>
#include <strings.h>
#include <cpu/interrupt.h>
#include <acpi.h>
#include <acpi/aml.h>
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

	char_t* mmap_data;
	for(size_t i = 0; i < SYSTEM_INFO->mmap_entry_count; i++) {
		mmap_data = itoh(SYSTEM_INFO->mmap[i].base);
		video_print(mmap_data);
		memory_free(mmap_data);
		video_print(" \0");
		mmap_data = itoh(SYSTEM_INFO->mmap[i].length);
		video_print(mmap_data);
		memory_free(mmap_data);
		video_print(" \0");
		mmap_data = itoh(SYSTEM_INFO->mmap[i].type);
		video_print(mmap_data);
		memory_free(mmap_data);
		video_print(" \n\0");
	}

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

		char_t* dsdt_addr = itoh(fadt->dsdt_address_32bit);
		video_print(dsdt_addr);
		memory_free(dsdt_addr);
		video_print("\n\0");
		memory_paging_add_page(fadt->dsdt_address_32bit, fadt->dsdt_address_32bit, MEMORY_PAGING_PAGE_TYPE_2M);

		acpi_sdt_header_t* dsdt = (acpi_sdt_header_t*)((uint64_t)(fadt->dsdt_address_32bit));
		if(acpi_validate_checksum(dsdt) == 0) {
			video_print("dsdt ok\n\0");
			uint64_t acpi_hs = 0x1000000;
			uint64_t acpi_he = 0x2000000;
			uint64_t step = 0x200000;
			for(uint64_t i = acpi_hs; i < acpi_he; i += step) {
				memory_paging_add_page(i, i, MEMORY_PAGING_PAGE_TYPE_2M);
			}

			memory_heap_t* acpi_heap = memory_create_heap_simple(acpi_hs, acpi_he);
			memory_set_default_heap(acpi_heap);


			int64_t aml_size = dsdt->length - sizeof(acpi_sdt_header_t);
			uint8_t* aml = (uint8_t*)(dsdt + 1);
			acpi_aml_parser_context_t* pctx = acp_aml_parser_context_create(aml, aml_size);
			if(pctx != NULL) {
				video_print("aml parser ctx created\n\0");
				if(acpi_aml_parse_all_items(pctx, NULL, NULL) == 0) {
					video_print("aml parsed\n\0");
				} else {
					video_print("aml not parsed\n\0");
				}
			} else {
				video_print("aml parser creation failed\n\0");
			}


			memory_set_default_heap(heap);
		} else {
			video_print("dsdt not ok\n\0");
		}
		acpi_table_mcfg_t* mcfg = (acpi_table_mcfg_t*)acpi_get_table(desc, "MCFG");
		if(mcfg == NULL) {
			video_print("can not find mcfg or incorrect checksum\n\0");
		} else {
			video_print("mcfg is found\n\0");
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

	return 0;
}
