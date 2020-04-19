#include <video.h>
#include <memory.h>
#include <strings.h>
#include <faraccess.h>
#include <diskio.h>
#include <descriptor.h>
#include <systeminfo.h>

uint8_t kmain16()
{
	memory_simple_init();

	video_clear_screen();

	memory_map_t* mmap;
	size_t mmap_e_count = memory_detect_map(&mmap);
	char_t* mmap_e_count_str = itoa(mmap_e_count);
	video_print("mmap entry count: ");
	video_print(mmap_e_count_str);
	video_print("\n");
	memory_simple_kfree(mmap_e_count_str);

	if(check_longmode()!=0) {
		video_print("No long mode support");
		return -1;
	}

	video_print("Hello, World!\r\n");

	disk_slot_table_t* st;
	uint8_t k64_copied = 0;
	if(disk_pio_read_slottable(&st) == 0) {
		for(uint8_t i = 0; i<0xF0; i++) {
			if(st->slots[i].type==4) {
				uint32_t sc = st->slots[i].end.part_low - st->slots[i].start.part_low + 1;
				uint32_t ss = st->slots[i].start.part_low;
				video_print("k64 kernel start at: ");
				char_t* tmp_str = itoh(ss);
				video_print(tmp_str);
				memory_simple_kfree(tmp_str);
				video_print(" size: ");
				tmp_str = itoh(sc);
				video_print(tmp_str);
				memory_simple_kfree(tmp_str);
				video_print(" sectors\r\n");
				reg_t base_mem = 0x2000;
				for(uint16_t i = 0; i<sc; i++) {
					uint8_t* data;
					uint64_t k4_lba_addr = {ss + i, 0};
					if(disk_pio_read(k4_lba_addr, 1, &data) !=0) {
						video_print("can not read k64 sectors\r\n");
						memory_simple_kfree(data);
						return -1;
					} else {
						for(uint16_t j = 0; j<512; j++) {
							far_write_8(base_mem, j, data[j]);
						}
						memory_simple_kfree(data);
						base_mem += 0x20;
					}
				}
				video_print("k64 kernel copied\r\n");
				k64_copied = 1;
				break;
			}
		}
	} else {
		video_print("Data not readed\r\n");
	}
	if(k64_copied == 0) {
		video_print("Can not copy k64 kernel\r\n");
		return -1;
	}

	if(descriptor_build_idt_register() != 0) {
		video_print("Can not build idt\r\n");
		return -1;
	} else {
		video_print("Default idt builded\r\n");
	}

	if(descriptor_build_gdt_register() != 0) {
		video_print("Can not build gdt\r\n");
		return -1;
	} else {
		video_print("Default gdt builded\r\n");
	}

	if(memory_build_page_table()!=0) {
		video_print("Can not build default page table\r\n");
		return -1;
	} else {
		video_print("Default page table builded\r\n");
	}

	SYSTEM_INFO = memory_simple_kmalloc(sizeof(system_info_t));
	SYSTEM_INFO->mmap = (memory_map_t*)memory_get_absolute_address((uint32_t)mmap);
	SYSTEM_INFO->mmap_entry_count = mmap_e_count;
	SYSTEM_INFO = (system_info_t*)memory_get_absolute_address((uint32_t)SYSTEM_INFO);
	return 0;
}
