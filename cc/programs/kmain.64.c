/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

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
#include <driver/network.h>

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

	if(network_init() != 0) {
		PRINTLOG(KERNEL, LOG_FATAL, "cannot init network. Halting...", 0);
		cpu_hlt();
	}

	ahci_sata_disk_t* d = ahci_get_disk_by_id(0);
	if(d) {
		disk_t* sata0 = gpt_get_or_create_gpt_disk(ahci_disk_impl_open(d));

		PRINTLOG(KERNEL, LOG_INFO, "disk size 0x%lx", sata0->get_disk_size(sata0));

		disk_partition_context_t* part_ctx;

		part_ctx = sata0->get_partition(sata0, 0);
		PRINTLOG(KERNEL, LOG_INFO, "part 0 start lba 0x%lx end lba 0x%lx", part_ctx->start_lba, part_ctx->end_lba);
		memory_free(part_ctx);

		part_ctx = sata0->get_partition(sata0, 1);
		PRINTLOG(KERNEL, LOG_INFO, "part 1 start lba 0x%lx end lba 0x%lx", part_ctx->start_lba, part_ctx->end_lba);
		memory_free(part_ctx);

		sata0->flush(sata0);
	}

	if(kbd_init() != 0) {
		PRINTLOG(KERNEL, LOG_FATAL, "cannot init keyboard. Halting...", 0);
		cpu_hlt();
	}

	PRINTLOG(KERNEL, LOG_INFO, "rdrand %i", cpu_check_rdrand());

	PRINTLOG(KERNEL, LOG_INFO, "all services is up... :)", 0);

	return 0;
}
