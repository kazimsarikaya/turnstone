#include <video.h>
#include <memory.h>
#include <strings.h>
#include <faraccess.h>
#include <diskio.h>
#include <descriptor.h>
#include <systeminfo.h>
#include <cpu.h>

extern uint8_t BOOT_DRIVE;

uint8_t kmain16()
{
	memory_simple_init();

	video_clear_screen();

	memory_map_t* mmap;
	size_t mmap_e_count = memory_detect_map(&mmap);
	char_t* mmap_e_count_str = itoa(mmap_e_count);
	video_print("mmap entry count: \0");
	video_print(mmap_e_count_str);
	video_print("\n");
	memory_simple_kfree(mmap_e_count_str);

	SYSTEM_INFO = memory_simple_kmalloc(sizeof(system_info_t));
	SYSTEM_INFO->mmap_entry_count = mmap_e_count;
	SYSTEM_INFO->mmap = mmap;

	if(cpu_check_longmode() != 0) {
		video_print("No long mode support\0");
		return -1;
	}

	video_print("Hello, World!\r\n\0");

	disk_slot_table_t* st;
	uint8_t k64_copied = 0;
	if(disk_read_slottable(BOOT_DRIVE, &st) == 0) {
		for(uint8_t i = 0; i < 0xF0; i++) {
			if(st->slots[i].type == 4) {
				uint32_t sc = st->slots[i].end.part_low - st->slots[i].start.part_low + 1;
				uint32_t ss = st->slots[i].start.part_low;
				video_print("k64 kernel start at: \0");
				char_t* tmp_str = itoh(ss);
				video_print(tmp_str);
				memory_simple_kfree(tmp_str);
				video_print(" size: \0");
				tmp_str = itoh(sc);
				video_print(tmp_str);
				memory_simple_kfree(tmp_str);
				video_print(" sectors\r\n\0");
				reg_t base_mem = 0x2000;
				for(uint32_t i = 0; i < sc; i++) {
					uint8_t* data;
					uint64_t k4_lba_addr = {ss + i, 0};
					if(disk_read(BOOT_DRIVE, k4_lba_addr, 1, &data) != 0) {
						video_print("can not read k64 sectors\r\n\0");
						memory_simple_kfree(data);
						return -1;
					} else {
						for(uint16_t j = 0; j < 512; j++) {
							far_write_8(base_mem, j, data[j]);
						}
						memory_simple_kfree(data);
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

	if(memory_build_page_table() != 0) {
		video_print("Can not build default page table\r\n\0");
		return -1;
	} else {
		video_print("Default page table builded\r\n\0");
	}

	SYSTEM_INFO->mmap = (memory_map_t*)memory_get_absolute_address((uint32_t)SYSTEM_INFO->mmap);
	SYSTEM_INFO = (system_info_t*)memory_get_absolute_address((uint32_t)SYSTEM_INFO);
	return 0;
}
