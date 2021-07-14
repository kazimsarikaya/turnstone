#include <linker.h>
#include <memory.h>
#include <video.h>

int8_t linker_memcopy_program_and_relink(uint64_t src_program_addr, uint64_t dst_program_addr, uint64_t entry_point_addr) {
	program_header_t* src_program = (program_header_t*)src_program_addr;
	program_header_t* dst_program = (program_header_t*)dst_program_addr;

	memory_memcopy(src_program, dst_program, src_program->program_size);

	uint64_t reloc_start = dst_program->reloc_start;
	reloc_start += dst_program_addr;
	uint64_t reloc_count = dst_program->reloc_count;

	linker_direct_relocation_t* relocs = (linker_direct_relocation_t*)reloc_start;

	uint8_t* dst_bytes = (uint8_t*)dst_program;

	for(uint64_t i = 0; i < reloc_count; i++) {
		if(relocs[i].type == LINKER_RELOCATION_TYPE_64_32) {
			uint32_t* target = (uint32_t*)&dst_bytes[relocs[i].offset];

			*target = (uint32_t)dst_program_addr + (uint32_t)relocs[i].addend;
		} else if(relocs[i].type == LINKER_RELOCATION_TYPE_64_32S) {
			int32_t* target = (int32_t*)&dst_bytes[relocs[i].offset];

			*target = (int32_t)dst_program_addr + (int32_t)relocs[i].addend;
		}  else if(relocs[i].type == LINKER_RELOCATION_TYPE_64_64) {
			uint64_t* target = (uint64_t*)&dst_bytes[relocs[i].offset];

			*target = dst_program_addr + relocs[i].addend;
		} else {
			printf("LINKER: Fatal unknown relocation type %i", relocs[i].type);
			return -1;
		}
	}

	linker_section_locations_t bss_sl = dst_program->section_locations[LINKER_SECTION_TYPE_BSS];
	uint8_t* dst_bss = (uint8_t*)(bss_sl.section_start + dst_program_addr);

	memory_memclean(dst_bss, bss_sl.section_size);

	uint64_t ep = (uint64_t)entry_point_addr;
	dst_program->entry_point_address_pc_relative = ep - src_program_addr - 4;

	return 0;
}
