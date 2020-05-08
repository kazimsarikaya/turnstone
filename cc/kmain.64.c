#include <types.h>
#include <helloworld.h>
#include <video.h>
#include <memory.h>
#include <memory/paging.h>
#include <strings.h>
#include <cpu/interrupt.h>
#include <acpi.h>
#include <pci.h>

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
		acpi_sdt_header_t* mcfg = acpi_get_table(desc, "MCFG");
		if(mcfg == NULL) {
			video_print("can not find mcfg or incorrect checksum\r\n\0");
		} else {
			video_print("cmfg is found\r\n\0");
			char_t* mcfg_addr = itoh((size_t)mcfg);
			video_print(mcfg_addr);
			video_print("\r\n\0");
			memory_free(mcfg_addr);
		}
	}


	video_print("pci common header size: \0");
	char_t* pch_size = itoh(sizeof(pci_cardbus_bridge_t));
	video_print(pch_size);
	video_print("\r\n\0");
	memory_free(pch_size);

	return 0;
}
