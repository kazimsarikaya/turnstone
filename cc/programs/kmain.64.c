#include <types.h>
#include <helloworld.h>
#include <video.h>
#include <memory.h>
#include <memory/paging.h>
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
#include <cpu/task.h>
#include <linker.h>
#include <driver/ahci.h>
#include <random.h>
#include <memory/frame.h>

int8_t kmain64(size_t entry_point);
int8_t kmain64_init();
void move_kernel(size_t src, size_t dst);

int8_t linker_remap_kernel() {
	program_header_t* kernel = (program_header_t*)SYSTEM_INFO->kernel_start;

	uint64_t data_start = 1 << 30; // 1gib

	linker_section_locations_t sec;
	uint64_t sec_size;
	uint64_t sec_start;

	linker_section_locations_t old_section_locations[LINKER_SECTION_TYPE_NR_SECTIONS];

	memory_memcopy(kernel->section_locations, old_section_locations, sizeof(old_section_locations));

	for(uint64_t i = 0; i < LINKER_SECTION_TYPE_NR_SECTIONS; i++) {
		old_section_locations[i].section_start = (old_section_locations[i].section_start >> 12) << 12;
		old_section_locations[i].section_start += SYSTEM_INFO->kernel_start;

		if(old_section_locations[i].section_size % FRAME_SIZE) {
			old_section_locations[i].section_size += (FRAME_SIZE - (old_section_locations[i].section_size % FRAME_SIZE));
		}
	}

	old_section_locations[LINKER_SECTION_TYPE_TEXT].section_start = SYSTEM_INFO->kernel_start;

	sec = kernel->section_locations[LINKER_SECTION_TYPE_TEXT];
	sec_start = sec.section_start;
	sec_size = sec.section_size;

	if(sec_size % FRAME_SIZE) {
		sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
	}

	kernel->section_locations[LINKER_SECTION_TYPE_TEXT].section_start = 2 << 20;
	kernel->section_locations[LINKER_SECTION_TYPE_TEXT].section_size = sec_size;

	sec = kernel->section_locations[LINKER_SECTION_TYPE_RELOCATION_TABLE];
	sec_start = sec.section_start;
	sec_size = sec.section_size + (FRAME_SIZE - (sec.section_size % FRAME_SIZE));
	kernel->section_locations[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_start = data_start;
	kernel->section_locations[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_size = sec_size;


	printf("sec 0x%08x  0x%08x\n", data_start, sec_size);

	for(uint64_t i = 0; i < sec_size; i += FRAME_SIZE) {
		memory_paging_add_page_ext(NULL, NULL, data_start, SYSTEM_INFO->kernel_start + sec_start + i, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC);

		data_start += FRAME_SIZE;
	}

	sec = kernel->section_locations[LINKER_SECTION_TYPE_RODATA];
	sec_start = sec.section_start;
	sec_size = sec.section_size;

	if(sec_size % FRAME_SIZE) {
		sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
	}

	kernel->section_locations[LINKER_SECTION_TYPE_RODATA].section_start = data_start;
	kernel->section_locations[LINKER_SECTION_TYPE_RODATA].section_size = sec_size;

	printf("sec 0x%08x  0x%08x\n", data_start, sec_size);

	for(uint64_t i = 0; i < sec_size; i += FRAME_SIZE) {
		memory_paging_add_page_ext(NULL, NULL, data_start, SYSTEM_INFO->kernel_start + sec_start + i, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC);

		memory_paging_toggle_attributes(SYSTEM_INFO->kernel_start + sec_start + i, MEMORY_PAGING_PAGE_TYPE_READONLY);

		data_start += FRAME_SIZE;
	}

	sec = kernel->section_locations[LINKER_SECTION_TYPE_BSS];
	sec_start = sec.section_start;
	sec_size = sec.section_size;

	if(sec_size % FRAME_SIZE) {
		sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
	}

	kernel->section_locations[LINKER_SECTION_TYPE_BSS].section_start = data_start;
	kernel->section_locations[LINKER_SECTION_TYPE_BSS].section_size = sec_size;

	printf("sec 0x%08x  0x%08x\n", data_start, sec_size);

	for(uint64_t i = 0; i < sec_size; i += FRAME_SIZE) {
		memory_paging_add_page_ext(NULL, NULL, data_start, SYSTEM_INFO->kernel_start + sec_start + i, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC);

		data_start += FRAME_SIZE;
	}

	sec = kernel->section_locations[LINKER_SECTION_TYPE_DATA];
	sec_start = sec.section_start;
	sec_size = sec.section_size;

	if(sec_size % FRAME_SIZE) {
		sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
	}

	kernel->section_locations[LINKER_SECTION_TYPE_DATA].section_start = data_start;
	kernel->section_locations[LINKER_SECTION_TYPE_DATA].section_size = sec_size;

	printf("sec 0x%08x  0x%08x\n", data_start, sec_size);

	for(uint64_t i = 0; i < sec_size; i += FRAME_SIZE) {
		memory_paging_add_page_ext(NULL, NULL, data_start, SYSTEM_INFO->kernel_start + sec_start + i, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC);

		data_start += FRAME_SIZE;
	}

	sec = kernel->section_locations[LINKER_SECTION_TYPE_HEAP];
	sec_start = sec.section_start;
	sec_size = sec.section_size;

	if(sec_size % FRAME_SIZE) {
		sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
	}

	data_start = 4ULL << 30;
	kernel->section_locations[LINKER_SECTION_TYPE_HEAP].section_start = data_start;
	kernel->section_locations[LINKER_SECTION_TYPE_HEAP].section_size = sec_size;

	printf("sec 0x%016lx  0x%08x\n", data_start, sec_size);

	for(uint64_t i = 0; i < sec_size; i += FRAME_SIZE) {
		memory_paging_add_page_ext(NULL, NULL, data_start, SYSTEM_INFO->kernel_start + sec_start + i, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC);

		data_start += FRAME_SIZE;
	}

	uint64_t stack_top = 4ULL << 30;

	sec = kernel->section_locations[LINKER_SECTION_TYPE_STACK];
	sec_start = sec.section_start;
	sec_size = sec.section_size;

	if(sec_size % FRAME_SIZE) {
		sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
	}

	kernel->section_locations[LINKER_SECTION_TYPE_STACK].section_start = stack_top - sec_size;
	kernel->section_locations[LINKER_SECTION_TYPE_STACK].section_size = sec_size;

	data_start = stack_top - sec_size;

	printf("sec 0x%08x  0x%08x\n", data_start, sec_size);

	for(uint64_t i = 0; i < sec_size; i += FRAME_SIZE) {
		memory_paging_add_page_ext(NULL, NULL, data_start, SYSTEM_INFO->kernel_start + sec_start + i, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC);

		data_start += FRAME_SIZE;
	}

	PRINTLOG("LINKER", "DEBUG", "new sections locations are created on page table", 0);


	SYSTEM_INFO->remapped = 1;

	frame_t* sysinfo_frms;

	KERNEL_FRAME_ALLOCATOR->rebuild_reserved_mmap(KERNEL_FRAME_ALLOCATOR);

	uint64_t sysinfo_size = SYSTEM_INFO->mmap_size + SYSTEM_INFO->reserved_mmap_size + sizeof(video_frame_buffer_t) + sizeof(system_info_t);
	uint64_t sysinfo_frm_count = (sysinfo_size + FRAME_SIZE - 1) / FRAME_SIZE;

	while(1) {
		KERNEL_FRAME_ALLOCATOR->allocate_frame_by_count(KERNEL_FRAME_ALLOCATOR, sysinfo_frm_count,
		                                                FRAME_ALLOCATION_TYPE_BLOCK | FRAME_ALLOCATION_TYPE_RESERVED,
		                                                &sysinfo_frms, NULL);


		for(uint64_t i = 0; i < sysinfo_frm_count; i++) {
			memory_paging_add_page_ext(NULL, NULL,
			                           sysinfo_frms->frame_address  + i * FRAME_SIZE,
			                           sysinfo_frms->frame_address  + i * FRAME_SIZE,
			                           MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC);
		}

		KERNEL_FRAME_ALLOCATOR->rebuild_reserved_mmap(KERNEL_FRAME_ALLOCATOR);

		uint64_t new_sysinfo_size = SYSTEM_INFO->mmap_size + SYSTEM_INFO->reserved_mmap_size + sizeof(video_frame_buffer_t) + sizeof(system_info_t);
		uint64_t new_sysinfo_frm_count = (new_sysinfo_size + FRAME_SIZE - 1) / FRAME_SIZE;

		if(sysinfo_frm_count != new_sysinfo_frm_count) {
			sysinfo_frm_count = new_sysinfo_frm_count;

			KERNEL_FRAME_ALLOCATOR->release_frame(KERNEL_FRAME_ALLOCATOR, sysinfo_frms);
		} else {
			break;
		}
	}

	PRINTLOG("LINKER", "DEBUG", "stored system info frame address 0x%016lx", sysinfo_frms->frame_address);

	uint8_t* mmap_data = (uint8_t*)sysinfo_frms->frame_address;
	uint8_t* reserved_mmap_data = (uint8_t*)(sysinfo_frms->frame_address + SYSTEM_INFO->mmap_size);
	video_frame_buffer_t* vfb = (video_frame_buffer_t*)(sysinfo_frms->frame_address + SYSTEM_INFO->mmap_size + SYSTEM_INFO->reserved_mmap_size);
	system_info_t* new_sysinfo = (system_info_t*)(sysinfo_frms->frame_address + SYSTEM_INFO->mmap_size + SYSTEM_INFO->reserved_mmap_size + sizeof(video_frame_buffer_t));

	memory_memcopy(SYSTEM_INFO->mmap_data, mmap_data, SYSTEM_INFO->mmap_size);
	memory_memcopy(SYSTEM_INFO->reserved_mmap_data, reserved_mmap_data, SYSTEM_INFO->reserved_mmap_size);
	memory_memcopy(SYSTEM_INFO->frame_buffer, vfb, sizeof(video_frame_buffer_t));
	memory_memcopy(SYSTEM_INFO, new_sysinfo, sizeof(system_info_t));
	new_sysinfo->mmap_data = mmap_data;
	new_sysinfo->reserved_mmap_data = reserved_mmap_data;
	new_sysinfo->frame_buffer = vfb;


	PRINTLOG("LINKER", "DEBUG", "new system info copy created at 0x%08x", new_sysinfo);

	uint8_t* dst_bytes = (uint8_t*)SYSTEM_INFO->kernel_start;
	linker_direct_relocation_t* relocs = (linker_direct_relocation_t*)(SYSTEM_INFO->kernel_start + kernel->reloc_start);
	kernel->reloc_start = kernel->section_locations[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_start;
	uint64_t kernel_start = SYSTEM_INFO->kernel_start;

	for(uint64_t i = 0; i < kernel->reloc_count; i++)
	{
		linker_section_type_t addend_section = LINKER_SECTION_TYPE_NR_SECTIONS;

		uint64_t pc_offset = 0;
		if(relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_PC32) {
			pc_offset = 4 + relocs[i].offset;
		}

		pc_offset += kernel_start;

		for(uint64_t j = 0; j < LINKER_SECTION_TYPE_NR_SECTIONS; j++) {
			if((relocs[i].addend + pc_offset) >= old_section_locations[j].section_start && (relocs[i].addend + pc_offset) < (old_section_locations[j].section_start + old_section_locations[j].section_size)) {
				addend_section = j;
				break;
			}
		}

		relocs[i].addend += kernel->section_locations[addend_section].section_start - old_section_locations[addend_section].section_start;

		if(relocs[i].relocation_type != LINKER_RELOCATION_TYPE_64_PC32) {
			relocs[i].addend += kernel_start;
		}
	}

	PRINTLOG("LINKER", "DEBUG", "relocs are computed. linking started", 0);

	for(uint64_t i = 0; i < kernel->reloc_count; i++) {
		if(relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_32) {
			uint32_t* target = (uint32_t*)&dst_bytes[relocs[i].offset];
			uint32_t target_value = (uint32_t)relocs[i].addend;
			*target = target_value;
		} else if(relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_32S || relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_PC32) {
			int32_t* target = (int32_t*)&dst_bytes[relocs[i].offset];
			int32_t target_value = (int32_t)relocs[i].addend;
			*target = target_value;
		} else if(relocs[i].relocation_type == LINKER_RELOCATION_TYPE_64_64) {
			uint64_t* target = (uint64_t*)&dst_bytes[relocs[i].offset];
			uint64_t target_value = relocs[i].addend;
			*target = target_value;
		} else {
			printf("LINKER: Fatal unknown relocation type %i\n", relocs[i].relocation_type);
			cpu_hlt();
		}
	}

	PRINTLOG("LINKER", "DEBUG", "relocs are finished.", 0);

	sec = kernel->section_locations[LINKER_SECTION_TYPE_BSS];
	sec_start = sec.section_start;
	sec_size = sec.section_size;

	printf("reloc completed\nnew sysinfo at 0x%p.\ncleaning bss and try to re-jump kernel\n", new_sysinfo);

	memory_memclean((uint8_t*)sec_start, sec_size);

	asm volatile (
		"jmp *%%rax\n"
		: : "c" (new_sysinfo), "a" (kernel_start)
		);

	return 0;
}

int8_t kmain64(size_t entry_point) {
	srand(0x123456789);
	memory_heap_t* heap = memory_create_heap_simple(0, 0);
	memory_set_default_heap(heap);

	video_init();
	video_clear_screen();

	if(heap == NULL) {
		printf("KERNEL: Error at creating heap\n");
		return -1;
	}

	PRINTLOG("KERNEL", "INFO", "Initializing stage 3 with remapped kernel? %i", SYSTEM_INFO->remapped);
	printf("KERNEL: Info new heap created at 0x%lx\n", heap);
	printf("Entry point of kernel is 0x%lx\n", entry_point);

	video_frame_buffer_t* new_vfb = memory_malloc(sizeof(video_frame_buffer_t));
	memory_memcopy(SYSTEM_INFO->frame_buffer, new_vfb, sizeof(video_frame_buffer_t));

	uint8_t* new_mmap_data = memory_malloc(SYSTEM_INFO->mmap_size);
	memory_memcopy(SYSTEM_INFO->mmap_data, new_mmap_data, SYSTEM_INFO->mmap_size);

	uint8_t* new_reserved_mmap_data = NULL;

	if(SYSTEM_INFO->reserved_mmap_size) {
		new_reserved_mmap_data = memory_malloc(SYSTEM_INFO->reserved_mmap_size);
		memory_memcopy(SYSTEM_INFO->reserved_mmap_data, new_reserved_mmap_data, SYSTEM_INFO->reserved_mmap_size);
	}

	system_info_t* new_system_info = memory_malloc(sizeof(system_info_t));
	memory_memcopy(SYSTEM_INFO, new_system_info, sizeof(system_info_t));

	new_system_info->frame_buffer = new_vfb;
	new_system_info->mmap_data = new_mmap_data;
	new_system_info->reserved_mmap_data = new_reserved_mmap_data;
	new_system_info->heap = heap;

	SYSTEM_INFO = new_system_info;

	frame_allocator_t* fa = frame_allocator_new_ext(heap);

	if(fa) {
		printf("frame allocator created\n");
		KERNEL_FRAME_ALLOCATOR = fa;

		frame_t kernel_frames = {SYSTEM_INFO->kernel_start, SYSTEM_INFO->kernel_4k_frame_count, FRAME_TYPE_USED, 0};
		if(fa->allocate_frame(fa, &kernel_frames) != 0) {
			PRINTLOG("KERNEL", "Panic", "cannot allocate kernel frames", 0);
			cpu_hlt();
		}
	} else {
		PRINTLOG("KERNEL", "Panic", "cannot allocate frame allocator. Halting...", 0);
		cpu_hlt();
	}

	if(descriptor_build_idt_register() != 0) {
		printf("Can not build idt\n");
		return -1;
	} else {
		printf("Default idt builded\n");
	}

	printf("KERNEL: Initializing interrupts\n");

	if(interrupt_init() != 0) {
		printf("CPU: Fatal cannot init interrupts\n");

		return -1;
	}

	printf("interrupts initialized\n");

	if(descriptor_build_gdt_register() != 0) {
		printf("Can not build gdt\n");
		return -1;
	} else {
		printf("Default gdt builded\n");
	}

	memory_page_table_t* p4 = memory_paging_build_table();
	if( p4 == NULL) {
		printf("Can not build default page table. Halting\n");
		cpu_hlt();
	} else {
		printf("Default page table builded at 0x%p\n", p4);
		memory_paging_switch_table(p4);
		video_refresh_frame_buffer_address();
		printf("Default page table switched to 0x%08p\n", p4);
	}

	if(SYSTEM_INFO->remapped == 0) {
		linker_remap_kernel();
	}

	printf("vfb address 0x%p\n", SYSTEM_INFO->frame_buffer);
	printf("Frame buffer at 0x%lp and size 0x%lx\n", SYSTEM_INFO->frame_buffer->virtual_base_address, SYSTEM_INFO->frame_buffer->buffer_size);
	printf("Screen resultion %ix%i\n", SYSTEM_INFO->frame_buffer->width, SYSTEM_INFO->frame_buffer->height);

	if(SYSTEM_INFO->acpi_table) {
		printf("acpi rsdp table version: %i address 0x%p\n", SYSTEM_INFO->acpi_version, SYSTEM_INFO->acpi_table);
	}

	char_t* test_data = "çok güzel bir kış ayı İĞÜŞÖÇ ığüşöç";
	printf("address 0x%p %s\n", test_data, test_data);

	printf("random data 0x%x\n", rand());

	KERNEL_FRAME_ALLOCATOR->cleanup(KERNEL_FRAME_ALLOCATOR);

	frame_allocator_print(KERNEL_FRAME_ALLOCATOR);

	PRINTLOG("KERNEL", "DEBUG", "Implement remaining ops with frame allocator", 0);

	while(1);

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
