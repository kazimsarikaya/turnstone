#include <types.h>
#include <video.h>
#include <memory.h>
#include <memory/paging.h>
#include <strings.h>
#include <faraccess.h>
#include <diskio.h>
#include <systeminfo.h>
#include <cpu.h>
#include <cpu/descriptor.h>
#include <ports.h>

extern uint8_t BOOT_DRIVE;

uint8_t kmain16()
{
	memory_heap_t* heap = memory_create_heap_simple(0, 0);
	memory_set_default_heap(heap);

	init_serial(COM1);

	video_clear_screen();

	printf("Hello World from stage2!\r\n");

	memory_map_t* mmap;
	size_t mmap_e_count = memory_detect_map(&mmap);
	printf("mmap entry count: %d\n", mmap_e_count);

	SYSTEM_INFO = memory_malloc(sizeof(system_info_t));
	SYSTEM_INFO->mmap_entry_count = mmap_e_count;
	SYSTEM_INFO->mmap = mmap;

	if(cpu_check_longmode() != 0) {
		printf("No long mode support");
		return -1;
	}

	if(BOOT_DRIVE != 0xFF) {
		disk_slot_table_t* st = NULL;
		uint8_t k64_copied = 0;

		if(disk_read_slottable(BOOT_DRIVE, &st) == 0 && st != NULL) {
			for(uint8_t i = 0; i < sizeof(disk_slot_table_t) / sizeof(disk_slot_t); i++) {
				if(st->slots[i].type == DISK_SLOT_TYPE_STAGE3) {
					uint32_t sc = st->slots[i].end.part_low - st->slots[i].start.part_low + 1;
					uint32_t ss = st->slots[i].start.part_low;

					printf("k64 kernel start at: 0x%04x size: 0x%04x sectors\r\n", ss, sc);

					reg_t base_mem = 0x2000;

					for(uint32_t i = 0; i < sc; i++) {
						uint8_t* data = NULL;
						uint64_t k4_lba_addr = {ss + i, 0};

						if(disk_read(BOOT_DRIVE, k4_lba_addr, 1, &data) != 0) {
							printf("can not read k64 sectors\r\n");
							memory_free(data);

							return -1;
						} else {
							for(uint16_t j = 0; j < 512; j++) {
								far_write_8(base_mem, j, data[j]);
							}

							memory_free(data);
							base_mem += 0x20;
						}
					}

					printf("k64 kernel copied\r\n");
					k64_copied = 1;
					SYSTEM_INFO->boot_type = SYSTEM_INFO_BOOT_TYPE_DISK;

					break;
				}
			}

		} else {
			printf("Slot table can not readed\r\n");
			return -1;
		}

		if(k64_copied == 0) {
			printf("Can not copy k64 kernel\r\n");
			return -1;
		}

	} else {
		printf("pxe boot stage3 already moved\r\n");
		SYSTEM_INFO->boot_type = SYSTEM_INFO_BOOT_TYPE_PXE;
	}

	if(descriptor_build_idt_register() != 0) {
		printf("Can not build idt\r\n");
		return -1;
	} else {
		printf("Default idt builded\r\n");
	}

	if(descriptor_build_gdt_register() != 0) {
		printf("Can not build gdt\r\n");
		return -1;
	} else {
		printf("Default gdt builded\r\n");
	}

	memory_page_table_t* p4 = memory_paging_build_table();
	if( p4 == NULL) {
		printf("Can not build default page table\r\n");
		return -1;
	} else {
		printf("Default page table builded\r\n");
		memory_paging_switch_table(p4);
		printf("Default page table switched to 0x%04p\r\n", p4);
	}

	SYSTEM_INFO->mmap = (memory_map_t*)memory_get_absolute_address((uint32_t)SYSTEM_INFO->mmap);
	SYSTEM_INFO = (system_info_t*)memory_get_absolute_address((uint32_t)SYSTEM_INFO);

	printf("stage2 completed\r\n");

	return 0;
}
