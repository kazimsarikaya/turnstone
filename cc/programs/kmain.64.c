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
#include <utils.h>
#include <device/kbd.h>
#include <diskio.h>
#include <cpu/task.h>
#include <linker.h>
#include <driver/ahci.h>

int8_t kmain64_init();
void move_kernel(size_t src, size_t dst);

int8_t kmain64(size_t entry_point) {
	video_clear_screen();

	printf("Initializing stage 3\n");
	printf("Entry point of kernel is 0x%lx\n", entry_point);

	memory_heap_t* heap = memory_create_heap_simple(0, 0);

	if(heap == NULL) {
		printf("KERNEL: Error at creating heap\n");
		return -1;
	}

	printf("KERNEL: Info new heap created at 0x%lx\n", heap);

	memory_set_default_heap(heap);

	printf("KERNEL: Initializing interrupts\n");

	if(interrupt_init() != 0) {
		printf("CPU: Fatal cannot init interrupts\n");

		return -1;
	}

	printf("interrupts initialized\n");

	memory_page_table_t* p4 = memory_paging_clone_pagetable();

	if(p4 == NULL) {
		printf("KERNEL: Fatal cannot create new page table\n");
		return -1;
	}

	memory_paging_switch_table(p4);

	uint64_t kernel_start = entry_point - 0x100;

	program_header_t* kernel = (program_header_t*)kernel_start;

	printf("kernel size %lx reloc start %lx reloc count %lx\n", kernel->program_size, kernel_start + kernel->reloc_start, kernel->reloc_count);

	//move_kernel(kernel_start, 64 << 20); /* for testing */

	if(task_init_tasking_ext(heap) != 0) {
		printf("TASKING: Fatal cannot init tasking\n");

		return -1;
	}

	printf("tasking initialized\n");

	return kmain64_init(heap);
}




void second_init(size_t kernel_loc) {
	video_clear_screen();
	printf("hello from second kernel at %lx\n", kernel_loc);
	__asm__ __volatile__ ("1: hlt\n jmp 1b\n");
}

void move_kernel(size_t src, size_t dst) {
	uint64_t new_area_start = dst;
	uint64_t new_area_end = dst + (128 << 20);
	uint64_t step = 0x200000;

	for(uint64_t i = new_area_start; i < new_area_end; i += step) {
		memory_paging_add_page(i, i, MEMORY_PAGING_PAGE_TYPE_2M);
	}

	if(linker_memcopy_program_and_relink(src, dst, (uint64_t)second_init) != 0) {
		printf("KERNEL: Fatal cannot move kernel\n");
		return;
	}

	printf("jumping new kernel at %lx\n", dst);

	__asm__ __volatile__ ( "call *%%rax\n" : : "D" (dst), "a" (dst)  );
}

void test_task1() {
	printf("task starting %li\n", task_get_id());
	for(uint64_t i = 0; i < 0x10; i++) {
		printf("hello world from task %li try: 0x%lx\n", task_get_id(), i);

		uint8_t* tmp = memory_malloc(1);
		memory_free(tmp);

		for(uint64_t j = 0; j < 0x800; j++) {
			printf("%li", task_get_id());
			if((j % 0x200) == 0) {
				task_yield();
			}
		}
		printf("\n");
	}
	printf("task %li ending\n", task_get_id());
}

int8_t kmain64_init(memory_heap_t* heap) {
	char_t* data = hello_world();

	printf("%s\n", data);

	printf("memory map table\n");
	printf("base\t\tlength\t\ttype\n");
	for(size_t i = 0; i < SYSTEM_INFO->mmap_entry_count; i++) {
		printf("0x%08lx\t0x%08lx\t0x%04lx\t0x%x\n",
		       SYSTEM_INFO->mmap[i].base,
		       SYSTEM_INFO->mmap[i].length,
		       SYSTEM_INFO->mmap[i].type,
		       SYSTEM_INFO->mmap[i].acpi);

		if(SYSTEM_INFO->mmap[i].type == MEMORY_MMAP_TYPE_RESERVED || SYSTEM_INFO->mmap[i].type == MEMORY_MMAP_TYPE_ACPI) {
			uint64_t base = SYSTEM_INFO->mmap[i].base;
			uint64_t len = SYSTEM_INFO->mmap[i].length;

			printf("MMAP: adding page for address 0x%lx with length 0x%lx\n", base, len);

			memory_paging_page_type_t pt;
			uint64_t pl;

			while(len > 0) {
				pt = MEMORY_PAGING_PAGE_TYPE_4K;
				pl = MEMORY_PAGING_PAGE_LENGTH_4K;

				if(len >= MEMORY_PAGING_PAGE_LENGTH_2M) {
					pt = MEMORY_PAGING_PAGE_TYPE_2M;
					pl = MEMORY_PAGING_PAGE_LENGTH_2M;
				}

				memory_paging_add_page(base, base, pt);

				if(pl > len) {
					break;
				}

				base += pl;
				len -= pl;
			}
		}
	}

	if(SYSTEM_INFO->boot_type == SYSTEM_INFO_BOOT_TYPE_PXE) {
		printf("System booted from pxe\n");

		disk_slot_t* initrd = (disk_slot_t*)DISK_SLOT_PXE_INITRD_BASE;
		for(int8_t i = 0; i < DISK_SLOT_INITRD_MAX_COUNT; i++) {
			if(initrd->type == DISK_SLOT_TYPE_UNUSED) {
				break;
			} else if(initrd->type != DISK_SLOT_TYPE_PXEINITRD) {
				printf("PXEINITRD: Fatal unknown slot type\n");
				return -1;
			}

			printf("Initrd start: 0x%08x end: 0x%08x\n", initrd->start, initrd->end);

			initrd++;
		}
	} else {
		printf("System booted from disk\n");
	}

	acpi_xrsdp_descriptor_t* desc = acpi_find_xrsdp();

	if(desc == NULL) {
		printf("acpi header not found or incorrect checksum\n");
		return -1;
	}

	if(apic_setup(desc) != 0) {
		printf("apic setup failed\n");
		return -1;
	}

	if(acpi_setup(desc) != 0) {
		printf("acpi setup failed\n");
	}



	acpi_table_mcfg_t* mcfg = acpi_get_mcfg_table(desc);

	if(mcfg == NULL) {
		printf("can not find mcfg or incorrect checksum\n");

		return -1;
	}

	printf("mcfg is found at 0x%08p\n", mcfg);

	linkedlist_t sata_controllers = linkedlist_create_list_with_heap(heap);

	uint64_t pci_hs = 0x1000000;
	uint64_t pci_he = 0x2000000;
	uint64_t pci_step = 0x200000;

	for(uint64_t i = pci_hs; i < pci_he; i += pci_step) {
		memory_paging_add_page(i, i, MEMORY_PAGING_PAGE_TYPE_2M);
	}

	memory_heap_t* pci_heap = memory_create_heap_simple(pci_hs, pci_he);

	iterator_t* iter = pci_iterator_create_with_heap(pci_heap, mcfg);

	while(iter->end_of_iterator(iter) != 0) {
		pci_dev_t* p = iter->get_item(iter);

		printf("pci dev found  %02x:%02x:%02x.%02x -> %04x:%04x -> %02x:%02x ",
		       p->group_number, p->bus_number, p->device_number, p->function_number,
		       p->pci_header->vendor_id, p->pci_header->device_id,
		       p->pci_header->class_code, p->pci_header->subclass_code);

		if( p->pci_header->class_code == PCI_DEVICE_CLASS_MASS_STORAGE_CONTROLLER &&
		    p->pci_header->subclass_code == PCI_DEVICE_SUBCLASS_SATA_CONTROLLER) {
			linkedlist_list_insert(sata_controllers, p);
		}


		if(p->pci_header->header_type.header_type == PCI_HEADER_TYPE_GENERIC_DEVICE) {
			pci_generic_device_t* pg = (pci_generic_device_t*)p->pci_header;

			printf("int %02x:%02x ", pg->interrupt_line, pg->interrupt_pin);

			if(pg->common_header.status.capabilities_list) {
				printf("caps ");
				pci_capability_t* pci_cap = (pci_capability_t*)(((uint8_t*)pg) + pg->capabilities_pointer);


				while(pci_cap->capability_id != 0xFF) {
					printf("0x%x \n", pci_cap->capability_id );

					if(pci_cap->next_pointer == NULL) {
						break;
					}

					pci_cap = (pci_capability_t*)(((uint8_t*)pci_cap ) + pci_cap->next_pointer);
				}
			}
		}

		printf("\n");

		if( p->pci_header->class_code != PCI_DEVICE_CLASS_MASS_STORAGE_CONTROLLER &&
		    p->pci_header->subclass_code != PCI_DEVICE_SUBCLASS_SATA_CONTROLLER) {
			memory_free_ext(pci_heap, p);
		}

		iter = iter->next(iter);
	}
	iter->destroy(iter);

	int8_t sata_port_cnt = ahci_init(heap, sata_controllers, 34 * (1 << 20));

	printf("KERNEL: sata port count: %i\n", sata_port_cnt);

	if(sata_port_cnt) {
		uint64_t r_size = 2048;
		uint8_t* rt_buf = memory_malloc(r_size);
		if(ahci_read(0, 0, r_size, rt_buf) == 0) {
			printf("%i bytes readed at 0x%p\n", r_size, rt_buf);

			uint8_t* wt_buf = memory_malloc(r_size);
			strcpy("hello world from ahci write", (char_t*)wt_buf);
			if(ahci_write(0, 1024, r_size, wt_buf) == 0) {
				printf("write success\n");
			} else {
				printf("write fail\n");
			}
		}

		// for(uint16_t i = 0; i <= 0xff; i++) {
		// 	ahci_read(0, 0, r_size, rt_buf);
		// }

		ahci_read(0, 0x400100, r_size, rt_buf);

		ahci_flush(0);
	}

	//task_create_task(heap, 0x1000, test_task1);

	interrupt_irq_set_handler(0x1, &dev_kbd_isr);
	apic_ioapic_setup_irq(0x1,
	                      APIC_IOAPIC_INTERRUPT_ENABLED
	                      | APIC_IOAPIC_DELIVERY_MODE_FIXED | APIC_IOAPIC_DELIVERY_STATUS_RELAX
	                      | APIC_IOAPIC_DESTINATION_MODE_PHYSICAL
	                      | APIC_IOAPIC_TRIGGER_MODE_EDGE | APIC_IOAPIC_PIN_POLARITY_ACTIVE_HIGH);


	printf("aml resource size %li\n", sizeof(acpi_aml_resource_t));
	uint8_t data1[] = {0x47, 0x01, 0x60, 0x00, 0x60, 0x00, 0x01, 0x01};
	uint8_t data2[] = {0x86, 0x09, 0x00, 0x00, 0x00, 0x00, 0xD0, 0xFE, 0x00, 0x04, 0x00, 0x00};
	acpi_aml_resource_t* res1 = (acpi_aml_resource_t*)data1;
	acpi_aml_resource_t* res2 = (acpi_aml_resource_t*)data2;
	printf("si %li %li %li\n", res1->type.type, res1->smallitem.name, res1->smallitem.length);
	printf("decode %i min 0x%x max 0x%x  aln 0x%x len 0x%x\n",
	       res1->smallitem.io.decode,
	       res1->smallitem.io.min,
	       res1->smallitem.io.max,
	       res1->smallitem.io.align,
	       res1->smallitem.io.length);

	printf("li %li %li %li\n", res2->type.type, res2->largeitem.name, res2->largeitem.length);

	//task_create_task(heap, 0x1000, test_task1);

	printf("tests completed!...\n");

	return 0;
}
