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
		PRINTLOG(KERNEL, LOG_DEBUG, "Default page table builded at 0x%lp", p4);
		memory_paging_switch_table(p4);

		SYSTEM_INFO->my_page_table = 1;

		video_refresh_frame_buffer_address();
		PRINTLOG(KERNEL, LOG_DEBUG, "Default page table switched to 0x%lp", p4);
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

	if(acpi_setup_events() != 0) {
		PRINTLOG(KERNEL, LOG_FATAL, "cannot setup acpi events", 0);
		cpu_hlt();
	}

	if(task_init_tasking_ext(heap) != 0) {
		PRINTLOG(KERNEL, LOG_FATAL, "cannot init tasking. Halting...", 0);
		cpu_hlt();
	}

	PRINTLOG(KERNEL, LOG_INFO, "tasking initialized", 0);

	int8_t sata_port_cnt = ahci_init(heap, PCI_CONTEXT->storage_controllers);

	if(sata_port_cnt == -1) {
		PRINTLOG(KERNEL, LOG_FATAL, "cannot init ahci. Halting...", 0);
		cpu_hlt();
	}

	uint8_t buffer[512] = {0};

	future_t fut = ahci_read(0, 1, 512, buffer);

	if(fut) {
		uint8_t* res = future_get_data_and_destroy(fut);

		for(int16_t i = 0; i < 512; i += 16) {
			for(int16_t j = 0; j < 16; j++) {
				printf("0x%02x ", res[i + j]);
			}

			printf("\n");
		}

	}

	PRINTLOG(KERNEL, LOG_ERROR, "Implement remaining ops with frame allocator", 0);

	return 0;


	return kmain64_init();
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

int8_t kmain64_init() {
	char_t* data = hello_world();

	printf("%s\n", data);

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
