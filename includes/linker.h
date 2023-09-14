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
#include <buffer.h>
#include <hashmap.h>
#include <tosdb/tosdb.h>

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
typedef struct linker_relocation_entry_t {
    linker_section_type_t    section_type    : 8; ///< relocation's section type
    linker_relocation_type_t relocation_type : 8; ///< relocation type
    uint64_t                 symbol_id; ///< symbol id
    uint64_t                 offset; ///< where relocation value will be placed from start of program
    uint64_t                 addend; ///< destination displacement
}__attribute__((packed)) linker_relocation_entry_t; ///< shorthand for struct

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


#define LINKER_GOT_SYMBOL_ID  0x1
#define LINKER_GOT_SECTION_ID 0x1

typedef struct linker_section_t {
    uint64_t virtual_start;
    uint64_t physical_start;
    uint64_t size;
    buffer_t section_data;
} linker_section_t;

typedef struct linker_module_t {
    uint64_t         id;
    uint64_t         virtual_start;
    uint64_t         physical_start;
    linker_section_t sections[LINKER_SECTION_TYPE_NR_SECTIONS];
} linker_module_t;


typedef struct linker_context_t {
    uint64_t   program_start_physical;
    uint64_t   program_start_virtual;
    uint64_t   program_size;
    uint64_t   global_offset_table_size;
    uint64_t   relocation_table_size;
    uint64_t   metadata_size;
    uint64_t   entrypoint_symbol_id;
    uint64_t   got_address_physical;
    uint64_t   got_address_virtual;
    uint64_t   entrypoint_address_virtual;
    uint64_t   size_of_sections[LINKER_SECTION_TYPE_NR_SECTIONS];
    hashmap_t* modules;
    buffer_t   got_table_buffer;
    hashmap_t* got_symbol_index_map;
    tosdb_t*   tdb;
} linker_context_t;

typedef enum linker_program_dump_type_t {
    LINKER_PROGRAM_DUMP_TYPE_NONE = 0,
    LINKER_PROGRAM_DUMP_TYPE_CODE = 1,
    LINKER_PROGRAM_DUMP_TYPE_GOT = 2,
    LINKER_PROGRAM_DUMP_TYPE_RELOCATIONS = 4,
    LINKER_PROGRAM_DUMP_TYPE_METADATA = 8,
    LINKER_PROGRAM_DUMP_TYPE_ALL = 0xF,
} linker_program_dump_type_t;


int8_t    linker_destroy_context(linker_context_t* ctx);
int8_t    linker_build_module(linker_context_t* ctx, uint64_t module_id, boolean_t recursive);
int8_t    linker_build_symbols(linker_context_t* ctx, uint64_t module_id, uint64_t section_id, uint8_t section_type, uint64_t section_offset);
int8_t    linker_build_relocations(linker_context_t* ctx, uint64_t section_id, uint8_t section_type, uint64_t section_offset, linker_section_t* section, boolean_t recursive);
boolean_t linker_is_all_symbols_resolved(linker_context_t* ctx);
int8_t    linker_calculate_program_size(linker_context_t* ctx);
int8_t    linker_bind_linear_addresses(linker_context_t* ctx);
int8_t    linker_bind_got_entry_values(linker_context_t* ctx);
int8_t    linker_link_program(linker_context_t* ctx);
int64_t   linker_get_section_count_without_relocations(linker_context_t* ctx);
buffer_t  linker_build_efi_image_relocations(linker_context_t* ctx);
buffer_t  linker_build_efi_image_section_headers_without_relocations(linker_context_t* ctx);
buffer_t  linker_build_efi(linker_context_t* ctx);
int8_t    linker_dump_program_to_array(linker_context_t* ctx, linker_program_dump_type_t dump_type, uint8_t* array);

#endif
