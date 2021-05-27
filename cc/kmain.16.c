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

extern uint8_t BOOT_DRIVE;

uint8_t kmain16()
{
	memory_heap_t* heap = memory_create_heap_simple(0, 0);
	memory_set_default_heap(heap);

	video_clear_screen();
	video_print("Hello World from stage2!\r\n\0");

	memory_map_t* mmap;
	size_t mmap_e_count = memory_detect_map(&mmap);
	char_t* mmap_e_count_str = itoa(mmap_e_count);
	video_print("mmap entry count: \0");
	video_print(mmap_e_count_str);
	video_print("\n");
	memory_free(mmap_e_count_str);

	SYSTEM_INFO = memory_malloc(sizeof(system_info_t));
	SYSTEM_INFO->mmap_entry_count = mmap_e_count;
	SYSTEM_INFO->mmap = mmap;

	if(cpu_check_longmode() != 0) {
		video_print("No long mode support\0");
		return -1;
	}

	if(BOOT_DRIVE != 0) {
		disk_slot_table_t* st;
		uint8_t k64_copied = 0;
		if(disk_read_slottable(BOOT_DRIVE, &st) == 0) {
			for(uint8_t i = 0; i < sizeof(disk_slot_table_t) / sizeof(disk_slot_t); i++) {
				if(st->slots[i].type == DISK_SLOT_TYPE_STAGE3) {
					uint32_t sc = st->slots[i].end.part_low - st->slots[i].start.part_low + 1;
					uint32_t ss = st->slots[i].start.part_low;
					video_print("k64 kernel start at: \0");
					char_t* tmp_str = itoh(ss);
					video_print(tmp_str);
					memory_free(tmp_str);
					video_print(" size: \0");
					tmp_str = itoh(sc);
					video_print(tmp_str);
					memory_free(tmp_str);
					video_print(" sectors\r\n\0");
					reg_t base_mem = 0x2000;
					for(uint32_t i = 0; i < sc; i++) {
						uint8_t* data;
						uint64_t k4_lba_addr = {ss + i, 0};
						if(disk_read(BOOT_DRIVE, k4_lba_addr, 1, &data) != 0) {
							video_print("can not read k64 sectors\r\n\0");
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
					video_print("k64 kernel copied\r\n\0");
					k64_copied = 1;
					break;
				}
			}
		} else {
			video_print("Slot table can not readed\r\n\0");
			return -1;
		}
		if(k64_copied == 0) {
			video_print("Can not copy k64 kernel\r\n\0");
			return -1;
		}
	} else {
		video_print("pxe boot stage3 already moved\r\n\0");
	}

	if(descriptor_build_idt_register() != 0) {
		video_print("Can not build idt\r\n\0");
		return -1;
	} else {
		video_print("Default idt builded\r\n\0");
	}

	if(descriptor_build_gdt_register() != 0) {
		video_print("Can not build gdt\r\n\0");
		return -1;
	} else {
		video_print("Default gdt builded\r\n\0");
	}

	memory_page_table_t* p4 = memory_paging_build_table();
	if( p4 == NULL) {
		video_print("Can not build default page table\r\n\0");
		return -1;
	} else {
		video_print("Default page table builded\r\n\0");
		memory_paging_switch_table(p4);
		video_print("Default page table switched\r\n\0");
	}

	SYSTEM_INFO->mmap = (memory_map_t*)memory_get_absolute_address((uint32_t)SYSTEM_INFO->mmap);
	SYSTEM_INFO = (system_info_t*)memory_get_absolute_address((uint32_t)SYSTEM_INFO);

	video_print("stage2 completed\r\n\0");
	return 0;
}
