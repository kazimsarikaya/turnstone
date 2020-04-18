#include <video.h>
#include <memory.h>
#include <strings.h>
#include <faraccess.h>
#include <diskio.h>
#include <descriptor.h>

extern uint8_t *__kpagetable_p4;
uint8_t kmain16(void)
{
	init_simple_memory();

	video_clear_screen();

	memory_map_t **mmap = simple_kmalloc(sizeof(memory_map_t**));
	int mmap_e_count = detect_memory(mmap);
	char_t * mmap_e_count_str = itoa(mmap_e_count);
	video_print("mmap entry count: ");
	video_print(mmap_e_count_str);
	video_print("\n");
	simple_kfree(mmap_e_count_str);

	if(check_longmode()!=0) {
		video_print("No long mode support");
		return -1;
	}

	video_print("Hello, World!\r\n");

	lbasupport_t lbas;
	if(disk_check_lba_support(&lbas)!=0) {
		video_print("LBA not supported\r\n");
		return -1;
	}

	disk_slot_table_t *st;
	uint8_t k64_copied = 0;
	if(disk_read_slottable(&st) == 0) {
		for(uint8_t i=0; i<0xF0; i++) {
			if(st->slots[i].type==4) {
				uint32_t sc = st->slots[i].end.part_low - st->slots[i].start.part_low +1;
				uint32_t ss = st->slots[i].start.part_low;
				video_print("k64 kernel start at: ");
				char_t *tmp_str = itoh(ss);
				video_print(tmp_str);
				simple_kfree(tmp_str);
				video_print(" size: ");
				tmp_str=itoh(sc);
				video_print(tmp_str);
				simple_kfree(tmp_str);
				video_print(" sectors\r\n");
				for(uint16_t i=0; i<sc; i++) {
					uint8_t *data;
					uint64_t k4_lba_addr = {ss+i,0};
					if(disk_read(k4_lba_addr,1,&data) !=0) {
						video_print("can not read k64 sectors\r\n");
						simple_kfree(data);
						return -1;
					} else {
						video_print("sector readed");
						for(uint16_t j=0; j<512; j++) {
							far_write_8(0x2000,j,data[j]);
						}
						simple_kfree(data);
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

	return 0;
}
