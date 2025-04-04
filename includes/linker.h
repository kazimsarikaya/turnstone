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
#include <utils.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum linker_section_type_t
 * @brief linker section types.
 *
 */
typedef enum linker_section_type_t {
    LINKER_SECTION_TYPE_TEXT, ///< executable (text) section
    LINKER_SECTION_TYPE_DATA, ///< read-write data section
    LINKER_SECTION_TYPE_DATARELOC, ///< read-write relocation data section
    LINKER_SECTION_TYPE_RODATA, ///< readonly data section
    LINKER_SECTION_TYPE_RODATARELOC, ///< readonly relocation data section
    LINKER_SECTION_TYPE_BSS, ///< bss section
    LINKER_SECTION_TYPE_PLT, ///< procedure linkage table section
    LINKER_SECTION_TYPE_RELOCATION_TABLE, ///< relocation table section
    LINKER_SECTION_TYPE_GOT_RELATIVE_RELOCATION_TABLE, ///< got relative relocation table section
    LINKER_SECTION_TYPE_GOT, ///< global offset table section
    LINKER_SECTION_TYPE_STACK, ///< stack section
    LINKER_SECTION_TYPE_HEAP, ///< heap section
    LINKER_SECTION_TYPE_NR_SECTIONS, ///< hack four enum item count
} linker_section_type_t; ///< shorthand for enum

/**
 * @var linker_section_type_names
 * @brief linker section type names
 */
extern const char_t*const linker_section_type_names[LINKER_SECTION_TYPE_NR_SECTIONS];

/**
 * @enum linker_relocation_type_t
 * @brief relocation stypes
 */
typedef enum linker_relocation_type_t {
    LINKER_RELOCATION_TYPE_32_16, ///< 32 bit width 16 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_32_32, ///< 32 bit width 32 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_32_PC16, ///< 32 bit width 16 bit addend program counter relative relocation
    LINKER_RELOCATION_TYPE_32_PC32, ///< 32 bit width 32 bit addend program counter relative relocation
    LINKER_RELOCATION_TYPE_64_8, ///< 64 bit width 8 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_64_16, ///< 64 bit width 16 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_64_32, ///< 64 bit width 32 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_64_32S, ///< 64 bit width signed 32 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_64_64, ///< 64 bit width 64 bit addend section relative relocation
    LINKER_RELOCATION_TYPE_64_PC32, ///< 64 bit width 32 bit addend program counter relative relocation
    LINKER_RELOCATION_TYPE_64_PC64, ///< 64 bit width 64 bit addend program counter relative relocation
    LINKER_RELOCATION_TYPE_64_GOT64, ///< 64 bit width 64 bit got direct
    LINKER_RELOCATION_TYPE_64_GOTOFF64, ///< 64 bit width 64 bit got offset relocation
    LINKER_RELOCATION_TYPE_64_GOTPC64, ///< 64 bit width 64 bit got pc relative relocation
    LINKER_RELOCATION_TYPE_64_PLTOFF64, ///< 64 bit width 64 bit plt offset relocation
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
    boolean_t             binded; ///< is binded
    linker_section_type_t section_type : 8; ///< section type
    linker_symbol_type_t  symbol_type  : 8; ///< symbol type
    linker_symbol_scope_t symbol_scope : 8; ///< symbol scope
    uint8_t               padding[3]; ///< align padding
    uint64_t              module_id; ///< module id
    uint64_t              symbol_id; ///< symbol id
    uint64_t              symbol_size; ///< symbol size
    uint64_t              symbol_value; ///< symbol value
    uint64_t              symbol_name_offset; ///< symbol name offset
}__attribute__((packed)) linker_global_offset_table_entry_t; ///< shorthand for struct

_Static_assert(sizeof(linker_global_offset_table_entry_t) % 8 == 0, "linker_global_offset_table_entry_t align mismatch");
_Static_assert(sizeof(linker_global_offset_table_entry_t) == 0x38, "linker_global_offset_table_entry_t size mismatch");

/*! linker executable or library magic TOSEL */
#define TOS_EXECUTABLE_OR_LIBRARY_MAGIC "TOSELF"
/**
 * @struct program_header_t
 * @brief program definition header.
 */
typedef struct program_header_t {
    uint8_t  jmp_code; ///< the jmp machine code 0xe9
    uint32_t trampoline_address_pc_relative; ///< jmp address of main
    uint8_t  magic[11]; ///< magic string
    uint64_t total_size; ///< total size of program
    uint64_t header_virtual_address; ///< program virtual address
    uint64_t header_physical_address; ///< program physical address
    uint64_t program_offset; ///< program offset
    uint64_t program_size; ///< program size
    uint64_t program_entry; ///< program entry
    uint64_t program_stack_size; ///< program stack size
    uint64_t program_stack_virtual_address; ///< program stack address
    uint64_t program_stack_physical_address; ///< program stack address
    uint64_t program_heap_size; ///< program heap size
    uint64_t program_heap_virtual_address; ///< program heap address
    uint64_t program_heap_physical_address; ///< program heap address
    uint64_t got_offset; ///< global offset table offset
    uint64_t got_size; ///< global offset table size
    uint64_t got_virtual_address; ///< global offset table virtual address
    uint64_t got_physical_address; ///< global offset table physical address
    uint64_t relocation_table_offset; ///< relocation table offset
    uint64_t relocation_table_size; ///< relocation table size
    uint64_t relocation_table_virtual_address; ///< relocation table virtual address
    uint64_t relocation_table_physical_address; ///< relocation table physical address
    uint64_t metadata_offset; ///< metadata offset
    uint64_t metadata_size; ///< metadata size
    uint64_t metadata_virtual_address; ///< metadata virtual address
    uint64_t metadata_physical_address; ///< metadata physical address
    uint64_t symbol_table_offset; ///< symbol table offset
    uint64_t symbol_table_size; ///< symbol table size
    uint64_t symbol_table_virtual_address; ///< symbol table virtual address
    uint64_t symbol_table_physical_address; ///< symbol table physical address

    uint64_t page_table_context_address; ///< page table address

    uint8_t trampoline_code[] __attribute__((aligned(256))); ///< trampoline code
}__attribute__((packed)) program_header_t; ///< shorthand for struct

_Static_assert(offsetof_field(program_header_t, trampoline_code) == 256, "program_header_t trampoline code align mismatch");

#define LINKER_GOT_SYMBOL_ID  0x1
#define LINKER_GOT_SECTION_ID 0x1

typedef struct linker_section_t {
    uint64_t  virtual_start;
    uint64_t  physical_start;
    uint64_t  size;
    buffer_t* section_data;
} linker_section_t;

typedef struct linker_module_t {
    uint64_t         id;
    uint64_t         module_name_offset;
    uint64_t         virtual_start;
    uint64_t         physical_start;
    hashmap_t*       plt_offsets;
    linker_section_t sections[LINKER_SECTION_TYPE_NR_SECTIONS];
} linker_module_t;


typedef struct linker_context_t {
    uint64_t   program_start_physical;
    uint64_t   program_start_virtual;
    uint64_t   program_size;
    uint64_t   global_offset_table_size;
    uint64_t   relocation_table_size;
    uint64_t   metadata_address_physical;
    uint64_t   metadata_address_virtual;
    uint64_t   metadata_size;
    uint64_t   symbol_table_size;
    uint64_t   entrypoint_symbol_id;
    uint64_t   got_address_physical;
    uint64_t   got_address_virtual;
    uint64_t   entrypoint_address_virtual;
    uint64_t   size_of_sections[LINKER_SECTION_TYPE_NR_SECTIONS];
    hashmap_t* modules;
    buffer_t*  got_table_buffer;
    buffer_t*  symbol_table_buffer;
    hashmap_t* got_symbol_index_map;
    tosdb_t*   tdb;
    uint64_t   page_table_helper_frames;
    boolean_t  for_hypervisor_application;
} linker_context_t;

typedef enum linker_program_dump_type_t {
    LINKER_PROGRAM_DUMP_TYPE_NONE = 0,
    LINKER_PROGRAM_DUMP_TYPE_CODE = 1,
    LINKER_PROGRAM_DUMP_TYPE_GOT = 2,
    LINKER_PROGRAM_DUMP_TYPE_RELOCATIONS = 4,
    LINKER_PROGRAM_DUMP_TYPE_METADATA = 8,
    LINKER_PROGRAM_DUMP_TYPE_HEADER = 0x10,
    LINKER_PROGRAM_DUMP_TYPE_ALL_WITHOUT_PAGE_TABLE = 0x1f,
    LINKER_PROGRAM_DUMP_TYPE_BUILD_PAGE_TABLE = 0x20,
    LINKER_PROGRAM_DUMP_TYPE_SYMBOLS = 0x40,
    LINKER_PROGRAM_DUMP_TYPE_ALL = 0x7f,
} linker_program_dump_type_t;


int8_t    linker_destroy_context(linker_context_t* ctx);
int8_t    linker_build_module(linker_context_t* ctx, uint64_t module_id, boolean_t recursive);
int8_t    linker_build_symbols(linker_context_t* ctx, uint64_t module_id, uint64_t section_id, uint8_t section_type, uint64_t section_offset);
int8_t    linker_build_relocations(linker_context_t* ctx, uint64_t section_id, uint8_t section_type, uint64_t section_offset, linker_module_t* module, boolean_t recursive);
boolean_t linker_is_all_symbols_resolved(linker_context_t* ctx);
int8_t    linker_calculate_program_size(linker_context_t* ctx);
int8_t    linker_bind_linear_addresses(linker_context_t* ctx);
int8_t    linker_bind_got_entry_values(linker_context_t* ctx);
int8_t    linker_link_program(linker_context_t* ctx);
int64_t   linker_get_section_count_without_relocations(linker_context_t* ctx);
buffer_t* linker_build_efi_image_relocations(linker_context_t* ctx);
buffer_t* linker_build_efi_image_section_headers_without_relocations(linker_context_t* ctx);
buffer_t* linker_build_efi(linker_context_t* ctx);
int8_t    linker_dump_program_to_array(linker_context_t* ctx, linker_program_dump_type_t dump_type, uint8_t* array);

void linker_build_modules_at_memory(void);
void linker_print_modules_at_memory(void);
void linker_print_module_info_at_memory(uint64_t module_id);

#ifdef __cplusplus
}
#endif

#endif
