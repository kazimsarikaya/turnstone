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

/**
 * @enum linker_section_type_e
 * @brief linker section types.
 *
 */
typedef enum linker_section_type_e {
    LINKER_SECTION_TYPE_TEXT, ///< executable (text) section
    LINKER_SECTION_TYPE_DATA, ///< read-write data section
    LINKER_SECTION_TYPE_RODATA, ///< readonly data section
    LINKER_SECTION_TYPE_BSS, ///< bss section
    LINKER_SECTION_TYPE_RELOCATION_TABLE, ///< relocation table section
    LINKER_SECTION_TYPE_STACK, ///< stack section
    LINKER_SECTION_TYPE_HEAP, ///< heap section
    LINKER_SECTION_TYPE_NR_SECTIONS, ///< hack four enum item count
} linker_section_type_t; ///< shorthand for enum

/**
 * @enum linker_relocation_type_e
 * @brief relocation stypes
 */
typedef enum linker_relocation_type_e {
    LINKER_RELOCATION_TYPE_32_16, ///< 32 bit wtih 16 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_32_32, ///< 32 bit wtih 32 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_32_PC16, ///< 32 bit wtih 16 bit addend program counter relative relocation
    LINKER_RELOCATION_TYPE_32_PC32, ///< 32 bit wtih 32 bit addend program counter relative relocation
    LINKER_RELOCATION_TYPE_64_32, ///< 64 bit wtih 32 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_64_32S, ///< 64 bit wtih signed 32 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_64_64, ///< 64 bit wtih 64 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_64_PC32, ///< 64 bit wtih 32 bit addend program counter relative relocation
} linker_relocation_type_t; ///< shorthand for enum

/**
 * @struct linker_direct_relocation_s
 * @brief relocation information
 */
typedef struct linker_direct_relocation_s {
    linker_section_type_t    section_type; ///< relocation's section type
    linker_relocation_type_t relocation_type; ///< relocation type
    uint64_t                 offset; ///< where relocation value will be placed from start of program
    uint64_t                 addend; ///< destination displacement
}__attribute__((packed)) linker_direct_relocation_t; ///< shorthand for struct

/**
 * @struct linker_section_locations_s
 * @brief section information
 */
typedef struct linker_section_locations_s {
    uint64_t section_start; ///< section start for virtual memory
    uint64_t section_pyhsical_start; ///< section start for physical memory
    uint64_t section_size; ///< section size
}linker_section_locations_t; ///< shorthand for struct

/**
 * @struct program_header_s
 * @brief program definition header.
 */
typedef struct program_header_s {
    uint8_t                    jmp_code; ///< the jmp machine code 0x90
    uint32_t                   entry_point_address_pc_relative; ///< jmp address of main
    uint8_t                    padding[11]; ///< align padding
    uint64_t                   program_size; ///< size of program with all data
    uint64_t                   reloc_start; ///< relocations' start location
    uint64_t                   reloc_count; ///< count of relocations defined as @ref linker_section_locations_t
    linker_section_locations_t section_locations[LINKER_SECTION_TYPE_NR_SECTIONS]; ///< section location table
}__attribute__((packed)) program_header_t; ///< shorthand for struct

/**
 * @brief linear linking program from start address to destination address. do not arrange section virtual addresses.
 * @param[in] src_program_addr source program address.
 * @param[in] dst_program_addr destination program address
 * @return 0 if success.
 */
int8_t linker_memcopy_program_and_relink(uint64_t src_program_addr, uint64_t dst_program_addr);

/**
 * @brief remaps kernel for moving sections to new virtual addresses
 * @return 0 if success
 */
int8_t linker_remap_kernel();

#endif
