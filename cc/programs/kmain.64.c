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
#include <time/timer.h>

int8_t kmain64(size_t entry_point);
int8_t kmain64_init();
void move_kernel(size_t src, size_t dst);

int8_t linker_remap_kernel() {
	program_header_t* kernel = (program_header_t*)SYSTEM_INFO->kernel_start;

	uint64_t data_start = 1 << 30; // 1gib

	linker_section_locations_t sec;
	uint64_t sec_size;
	uint64_t sec_start;
	frame_t f;

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


	PRINTLOG(LINKER, LOG_TRACE, "sec 0x%08x  0x%08x\n", data_start, sec_size);

	f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
	f.frame_count = sec_size / FRAME_SIZE;

	if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
		return -1;
	}

	data_start += sec_size;

	sec = kernel->section_locations[LINKER_SECTION_TYPE_RODATA];
	sec_start = sec.section_start;
	sec_size = sec.section_size;

	if(sec_size % FRAME_SIZE) {
		sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
	}

	kernel->section_locations[LINKER_SECTION_TYPE_RODATA].section_start = data_start;
	kernel->section_locations[LINKER_SECTION_TYPE_RODATA].section_size = sec_size;

	PRINTLOG(LINKER, LOG_TRACE, "sec 0x%08x  0x%08x\n", data_start, sec_size);

	f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
	f.frame_count = sec_size / FRAME_SIZE;

	if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
		return -1;
	}

	for(uint64_t i = 0; i < sec_size; i += FRAME_SIZE) {
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

	PRINTLOG(LINKER, LOG_TRACE, "sec 0x%08x  0x%08x\n", data_start, sec_size);

	f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
	f.frame_count = sec_size / FRAME_SIZE;

	if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
		return -1;
	}

	data_start += sec_size;

	sec = kernel->section_locations[LINKER_SECTION_TYPE_DATA];
	sec_start = sec.section_start;
	sec_size = sec.section_size;

	if(sec_size % FRAME_SIZE) {
		sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
	}

	kernel->section_locations[LINKER_SECTION_TYPE_DATA].section_start = data_start;
	kernel->section_locations[LINKER_SECTION_TYPE_DATA].section_size = sec_size;

	PRINTLOG(LINKER, LOG_TRACE, "sec 0x%08x  0x%08x\n", data_start, sec_size);

	f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
	f.frame_count = sec_size / FRAME_SIZE;

	if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
		return -1;
	}

	data_start += sec_size;

	sec = kernel->section_locations[LINKER_SECTION_TYPE_HEAP];
	sec_start = sec.section_start;
	sec_size = sec.section_size;

	if(sec_size % FRAME_SIZE) {
		sec_size = sec_size + (FRAME_SIZE - (sec_size % FRAME_SIZE));
	}

	data_start = 4ULL << 30;
	kernel->section_locations[LINKER_SECTION_TYPE_HEAP].section_start = data_start;
	kernel->section_locations[LINKER_SECTION_TYPE_HEAP].section_size = sec_size;

	PRINTLOG(LINKER, LOG_TRACE, "sec 0x%016lx  0x%08x\n", data_start, sec_size);

	f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
	f.frame_count = sec_size / FRAME_SIZE;

	if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
		return -1;
	}

	data_start += sec_size;

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

	PRINTLOG(LINKER, LOG_TRACE, "sec 0x%08x  0x%08x\n", data_start, sec_size);

	f.frame_address = SYSTEM_INFO->kernel_start + sec_start;
	f.frame_count = sec_size / FRAME_SIZE;

	if(memory_paging_add_va_for_frame(data_start, &f, MEMORY_PAGING_PAGE_TYPE_4K | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
		return -1;
	}

	data_start += sec_size;

	PRINTLOG(LINKER, LOG_DEBUG, "new sections locations are created on page table", 0);


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

	PRINTLOG(LINKER, LOG_DEBUG, "stored system info frame address 0x%016lx", sysinfo_frms->frame_address);

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


	PRINTLOG(LINKER, LOG_DEBUG, "new system info copy created at 0x%08x", new_sysinfo);

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

	PRINTLOG(LINKER, LOG_DEBUG, "relocs are computed. linking started", 0);

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
			PRINTLOG(LINKER, LOG_FATAL, "unknown relocation type %i\n", relocs[i].relocation_type);
			cpu_hlt();
		}
	}

	PRINTLOG(LINKER, LOG_DEBUG, "relocs are finished.", 0);

	sec = kernel->section_locations[LINKER_SECTION_TYPE_BSS];
	sec_start = sec.section_start;
	sec_size = sec.section_size;

	PRINTLOG(LINKER, LOG_DEBUG, "reloc completed\nnew sysinfo at 0x%p.\ncleaning bss and try to re-jump kernel\n", new_sysinfo);

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
		PRINTLOG(KERNEL, LOG_FATAL, "creating heap", 0);
		return -1;
	}

	PRINTLOG(KERNEL, LOG_INFO, "Initializing stage 3 with remapped kernel? %i", SYSTEM_INFO->remapped);
	PRINTLOG(KERNEL, LOG_INFO, "new heap created at 0x%lx", heap);
	PRINTLOG(KERNEL, LOG_INFO, "Entry point of kernel is 0x%lx", entry_point);

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
		PRINTLOG(KERNEL, LOG_DEBUG, "frame allocator created", 0);
		KERNEL_FRAME_ALLOCATOR = fa;

		frame_t kernel_frames = {SYSTEM_INFO->kernel_start, SYSTEM_INFO->kernel_4k_frame_count, FRAME_TYPE_USED, 0};
		if(fa->allocate_frame(fa, &kernel_frames) != 0) {
			PRINTLOG(KERNEL, LOG_PANIC, "cannot allocate kernel frames", 0);
			cpu_hlt();
		}
	} else {
		PRINTLOG(KERNEL, LOG_PANIC, "cannot allocate frame allocator. Halting...", 0);
		cpu_hlt();
	}

	if(descriptor_build_idt_register() != 0) {
		PRINTLOG(KERNEL, LOG_PANIC, "Can not build idt. Halting...", 0);
		cpu_hlt();
	} else {
		PRINTLOG(KERNEL, LOG_DEBUG, "Default idt builded", 0);
	}

	PRINTLOG(KERNEL, LOG_DEBUG, "Initializing interrupts", 0);

	if(interrupt_init() != 0) {
		PRINTLOG(KERNEL, LOG_PANIC, "cannot init interrupts", 0);

		return -1;
	}

	PRINTLOG(KERNEL, LOG_DEBUG, "interrupts initialized", 0);

	if(descriptor_build_gdt_register() != 0) {
		PRINTLOG(KERNEL, LOG_PANIC, "Can not build gdt", 0);
		return -1;
	} else {
		PRINTLOG(KERNEL, LOG_DEBUG, "Default gdt builded", 0);
	}

	memory_page_table_t* p4 = memory_paging_build_table();
	if( p4 == NULL) {
		PRINTLOG(KERNEL, LOG_PANIC, "Can not build default page table. Halting", 0);
		cpu_hlt();
	} else {
		PRINTLOG(KERNEL, LOG_DEBUG, "Default page table builded at 0x%p", p4);
		memory_paging_switch_table(p4);
		video_refresh_frame_buffer_address();
		PRINTLOG(KERNEL, LOG_DEBUG, "Default page table switched to 0x%08p", p4);
	}

	if(SYSTEM_INFO->remapped == 0) {
		linker_remap_kernel();
	}

	PRINTLOG(KERNEL, LOG_DEBUG, "vfb address 0x%p", SYSTEM_INFO->frame_buffer);
	PRINTLOG(KERNEL, LOG_DEBUG, "Frame buffer at 0x%lp and size 0x%016lx", SYSTEM_INFO->frame_buffer->virtual_base_address, SYSTEM_INFO->frame_buffer->buffer_size);
	PRINTLOG(KERNEL, LOG_DEBUG, "Screen resultion %ix%i", SYSTEM_INFO->frame_buffer->width, SYSTEM_INFO->frame_buffer->height);

	if(SYSTEM_INFO->acpi_table) {
		PRINTLOG(ACPI, LOG_DEBUG, "acpi rsdp table version: %i address 0x%lp", SYSTEM_INFO->acpi_version, SYSTEM_INFO->acpi_table);
	}

	char_t* test_data = "çok güzel bir kış ayı İĞÜŞÖÇ ığüşöç";
	printf("address 0x%p %s\n", test_data, test_data);

	printf("random data 0x%x\n", rand());

	KERNEL_FRAME_ALLOCATOR->cleanup(KERNEL_FRAME_ALLOCATOR);

	LOGBLOCK(FRAMEALLOCATOR, LOG_DEBUG){
		frame_allocator_print(KERNEL_FRAME_ALLOCATOR);
	}

	acpi_xrsdp_descriptor_t* desc = acpi_find_xrsdp();

	if(desc == NULL) {
		PRINTLOG(KERNEL, LOG_FATAL, "acpi header not found or incorrect checksum. Halting...", 0);
		cpu_hlt();
	}

	if(acpi_setup(desc) != 0) {
		PRINTLOG(KERNEL, LOG_FATAL, "acpi setup failed. Halting", 0);
		cpu_hlt();
	}

	if(pci_setup(NULL) != 0) {
		PRINTLOG(KERNEL, LOG_FATAL, "pci setup failed. Halting", 0);
		cpu_hlt();
	}

	if(apic_setup(ACPI_CONTEXT->xrsdp_desc) != 0) {
		PRINTLOG(KERNEL, LOG_FATAL, "apic setup failed. Halting", 0);
		cpu_hlt();
	}

	if(task_init_tasking_ext(heap) != 0) {
		PRINTLOG(KERNEL, LOG_FATAL, "cannot init tasking. Halting...", 0);
		cpu_hlt();
	}

	PRINTLOG(KERNEL, LOG_INFO, "tasking initialized", 0);

	time_timer_spinsleep(1000ULL * 1000ULL * 5000ULL);

	PRINTLOG(KERNEL, LOG_ERROR, "Implement remaining ops with frame allocator", 0);

	acpi_poweroff();

	cpu_hlt();


	return kmain64_init(heap);
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


	linkedlist_t sata_controllers = linkedlist_create_list_with_heap(heap);


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

	//task_create_task(heap, 0x1000, test_task1);

	printf("tests completed!...\n");

	return 0;
}
