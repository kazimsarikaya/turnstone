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
		video_print("acpi header not found or incorrect checksum\r\n\0");
	} else{
		video_print("acpi header is ok\r\n\0");
		acpi_sdt_header_t* madt = acpi_get_table(desc, "APIC");
		if(madt == NULL) {
			video_print("can not find madt or incorrect checksum\r\n\0");
		} else {
			video_print("madt is found\r\n\0");
		}
		acpi_table_mcfg_t* mcfg = (acpi_table_mcfg_t*)acpi_get_table(desc, "MCFG");
		if(mcfg == NULL) {
			video_print("can not find mcfg or incorrect checksum\r\n\0");
		} else {
			video_print("cmfg is found\r\n\0");
			char_t* mcfg_addr = itoh((size_t)mcfg);
			video_print(mcfg_addr);
			video_print("\r\n\0");
			memory_free(mcfg_addr);

			iterator_t* iter = pci_iterator_create(mcfg);
			while(iter->end_of_iterator(iter) != 0) {
				video_print("pci dev found\r\n\0");
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

				video_print("\r\n\0");
				memory_free(p);

				iter = iter->next(iter);
			}
			iter->destroy(iter);

		}
	}

	video_print("tests completed!...\r\n\0");
	return 0;
}
