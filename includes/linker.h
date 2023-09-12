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
 * @enum linker_section_type_t
 * @brief linker section types.
 *
 */
typedef enum linker_section_type_t {
    LINKER_SECTION_TYPE_TEXT, ///< executable (text) section
    LINKER_SECTION_TYPE_DATA, ///< read-write data section
    LINKER_SECTION_TYPE_RODATA, ///< readonly data section
    LINKER_SECTION_TYPE_ROREL, ///< readonly relocation data section
    LINKER_SECTION_TYPE_BSS, ///< bss section
    LINKER_SECTION_TYPE_RELOCATION_TABLE, ///< relocation table section
    LINKER_SECTION_TYPE_GOT_RELATIVE_RELOCATION_TABLE, ///< got relative relocation table section
    LINKER_SECTION_TYPE_GOT, ///< global offset table section
    LINKER_SECTION_TYPE_STACK, ///< stack section
    LINKER_SECTION_TYPE_HEAP, ///< heap section
    LINKER_SECTION_TYPE_NR_SECTIONS, ///< hack four enum item count
} linker_section_type_t; ///< shorthand for enum

/**
 * @enum linker_relocation_type_t
 * @brief relocation stypes
 */
typedef enum linker_relocation_type_t {
    LINKER_RELOCATION_TYPE_32_16, ///< 32 bit width 16 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_32_32, ///< 32 bit width 32 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_32_PC16, ///< 32 bit width 16 bit addend program counter relative relocation
    LINKER_RELOCATION_TYPE_32_PC32, ///< 32 bit width 32 bit addend program counter relative relocation
    LINKER_RELOCATION_TYPE_64_32, ///< 64 bit width 32 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_64_32S, ///< 64 bit width signed 32 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_64_64, ///< 64 bit width 64 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_64_PC32, ///< 64 bit width 32 bit addend program counter relative relocation
    LINKER_RELOCATION_TYPE_64_GOT64,///< 64 bit width 64 bit got direct
    LINKER_RELOCATION_TYPE_64_GOTOFF64, ///< 64 bit width 64 bit got offset relocation
    LINKER_RELOCATION_TYPE_64_GOTPC64, ///< 64 bit width 64 bit got pc relative relocation
} linker_relocation_type_t; ///< shorthand for enum


/**
 * @enum linker_symbol_type_t
 * @brief symbol types
 */
typedef enum linker_symbol_type_t {
    LINKER_SYMBOL_TYPE_UNDEF, ///< undefined
    LINKER_SYMBOL_TYPE_OBJECT, ///< object
    LINKER_SYMBOL_TYPE_FUNCTION, ///< function
    LINKER_SYMBOL_TYPE_SECTION, ///< section
    LINKER_SYMBOL_TYPE_SYMBOL, ///< symbol
} linker_symbol_type_t; ///< shorthand for enum


/**
 * @enum linker_symbol_scope_t
 * @brief symbol scope
 */
typedef enum linker_symbol_scope_t {
    LINKER_SYMBOL_SCOPE_LOCAL, ///< local
    LINKER_SYMBOL_SCOPE_GLOBAL, ///< global
} linker_symbol_scope_t; ///< shorthand for enum

/**
 * @struct linker_direct_relocation_t
 * @brief relocation information
 */
typedef struct linker_direct_relocation_t {
    linker_section_type_t    section_type    : 8; ///< relocation's section type
    linker_relocation_type_t relocation_type : 8; ///< relocation type
    uint64_t                 symbol_id; ///< symbol id
    uint64_t                 offset; ///< where relocation value will be placed from start of program
    uint64_t                 addend; ///< destination displacement
}__attribute__((packed)) linker_direct_relocation_t; ///< shorthand for struct

/**
 * @struct linker_section_locations_t
 * @brief section information
 */
typedef struct linker_section_locations_t {
    uint64_t section_start; ///< section start for virtual memory
    uint64_t section_pyhsical_start; ///< section start for physical memory
    uint64_t section_size; ///< section size
}__attribute__((packed)) linker_section_locations_t; ///< shorthand for struct

typedef struct linker_global_offset_table_entry_t {
    uint64_t              entry_value; ///< entry value
    boolean_t             resolved; ///< is resolved
    linker_section_type_t section_type : 8; ///< section type
    linker_symbol_type_t  symbol_type  : 8; ///< symbol type
    linker_symbol_scope_t symbol_scope : 8; ///< symbol scope
    uint64_t              module_id; ///< module id
    uint64_t              symbol_id; ///< symbol id
    uint64_t              symbol_size; ///< symbol size
    uint64_t              symbol_value; ///< symbol value
    uint8_t               padding[4]; ///< align padding
}__attribute__((packed)) linker_global_offset_table_entry_t; ///< shorthand for struct

_Static_assert(sizeof(linker_global_offset_table_entry_t) % 8 == 0, "linker_global_offset_table_entry_t align mismatch");

/*! linker executable or library magic TOSEL */
#define TOS_EXECUTABLE_OR_LIBRARY_MAGIC "TOSELF"
/**
 * @struct program_header_t
 * @brief program definition header.
 */
typedef struct program_header_t {
    uint8_t                    jmp_code; ///< the jmp machine code 0x90
    uint32_t                   entry_point_address_pc_relative; ///< jmp address of main
    uint8_t                    padding[11]; ///< align padding
    uint64_t                   program_size; ///< size of program with all data
    uint64_t                   reloc_start; ///< relocations' start location
    uint64_t                   reloc_count; ///< count of relocations defined as @ref linker_section_locations_t
    uint64_t                   got_rel_reloc_start; ///< got relative relocations' start location
    uint64_t                   got_rel_reloc_count; ///< count of got relative relocations defined as @ref linker_section_locations_t
    uint64_t                   got_entry_count; ///< entry count at got
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
int8_t linker_remap_kernel(void);

#endif
