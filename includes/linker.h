/**
 * @file linker.h
 * @brief linker functions and types
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___LINKER_H
/*! prevent duplicate header error macro */
#define ___LINKER_H 0

#include <types.h>

typedef enum {
	LINKER_SECTION_TYPE_TEXT,
	LINKER_SECTION_TYPE_DATA,
	LINKER_SECTION_TYPE_RODATA,
	LINKER_SECTION_TYPE_BSS,
	LINKER_SECTION_TYPE_RELOCATION_TABLE,
	LINKER_SECTION_TYPE_STACK,
	LINKER_SECTION_TYPE_HEAP,
	LINKER_SECTION_TYPE_NR_SECTIONS,
} linker_section_type_t;

typedef enum {
	LINKER_RELOCATION_TYPE_32_16,
	LINKER_RELOCATION_TYPE_32_32,
	LINKER_RELOCATION_TYPE_32_PC16,
	LINKER_RELOCATION_TYPE_32_PC32,
	LINKER_RELOCATION_TYPE_64_32,
	LINKER_RELOCATION_TYPE_64_32S,
	LINKER_RELOCATION_TYPE_64_64,
	LINKER_RELOCATION_TYPE_64_PC32,
} linker_relocation_type_t;

typedef struct {
	linker_section_type_t section_type;
	linker_relocation_type_t relocation_type;
	uint64_t offset;
	uint64_t addend;
}__attribute__((packed)) linker_direct_relocation_t;

typedef struct {
	uint64_t section_start;
	uint64_t section_pyhsical_start;
	uint64_t section_size;
}linker_section_locations_t;

typedef struct {
	uint8_t jmp_code;
	uint32_t entry_point_address_pc_relative;
	uint8_t padding[11];
	uint64_t program_size;
	uint64_t reloc_start;
	uint64_t reloc_count;
	linker_section_locations_t section_locations[LINKER_SECTION_TYPE_NR_SECTIONS];
}__attribute__((packed)) program_header_t;

int8_t linker_memcopy_program_and_relink(uint64_t src_program_addr, uint64_t dst_program_addr);

int8_t linker_remap_kernel();

#endif
