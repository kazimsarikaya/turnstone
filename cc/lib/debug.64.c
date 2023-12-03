/**
 * @file debug.64.c
 * @brief debug implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <debug.h>
#include <hashmap.h>
#include <memory.h>
#include <logging.h>
#include <systeminfo.h>
#include <linker.h>
#include <memory/paging.h>

MODULE("turnstone.lib");


typedef enum debug_breakpoint_type_t {
    DEBUG_BREAKPOINT_TYPE_SINGLE_TIME,
} debug_breakpoint_type_t;

typedef struct debug_breakpoint_t {
    debug_breakpoint_type_t type;
    uint64_t                address;
    uint8_t                 original_instruction;
} debug_breakpoint_t;


hashmap_t* debug_location_map = NULL;
hashmap_t* debug_symbol_map = NULL;


static const char_t* debug_get_symbol_name_by_symbol_name_offset(uint64_t symbol_name_offset) {
    program_header_t* ph = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;

    const char_t* symbol_name = (const char_t*)(ph->symbol_table_virtual_address + symbol_name_offset);

    return symbol_name;
}


int8_t debug_init(void) {
    debug_location_map = hashmap_integer(128);

    if(!debug_location_map) {
        return -1;
    }

    debug_symbol_map = hashmap_string(128);

    if(!debug_symbol_map) {
        return -1;
    }

    program_header_t* ph = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;

    linker_global_offset_table_entry_t* got = (linker_global_offset_table_entry_t*)ph->got_virtual_address;

    uint64_t got_entry_count = ph->got_size / sizeof(linker_global_offset_table_entry_t);

    for(uint64_t i = 0; i < got_entry_count; i++) {
        if(got[i].symbol_type == LINKER_SYMBOL_TYPE_FUNCTION) {
            const char_t* symbol_name = debug_get_symbol_name_by_symbol_name_offset(got[i].symbol_name_offset);
            hashmap_put(debug_symbol_map, (void*)symbol_name, (void*)got[i].entry_value);
        }
    }

    PRINTLOG(KERNEL, LOG_INFO, "Initialized debug");

    return 0;
}

uint64_t debug_get_symbol_address(const char_t* symbol_name) {
    return (uint64_t)hashmap_get(debug_symbol_map, (void*)symbol_name);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
void debug_put_single_time_breakpoint_at_address(uint64_t address) {
    if(!debug_location_map) {
        debug_location_map = hashmap_integer(128);
    }

    debug_breakpoint_t* breakpoint = memory_malloc(sizeof(debug_breakpoint_t));

    if(!breakpoint) {
        PRINTLOG(KERNEL, LOG_ERROR, "Failed to allocate memory for breakpoint");
        return;
    }

    PRINTLOG(KERNEL, LOG_INFO, "Putting breakpoint at 0x%016llx", address);

    breakpoint->type = DEBUG_BREAKPOINT_TYPE_SINGLE_TIME;
    breakpoint->address = address;
    breakpoint->original_instruction = *((uint8_t*)address);

    hashmap_put(debug_location_map, (void*)address, breakpoint);

    memory_paging_toggle_attributes(address, MEMORY_PAGING_PAGE_TYPE_READONLY);
    *((uint8_t*)address) = 0xCC;
    memory_paging_toggle_attributes(address, MEMORY_PAGING_PAGE_TYPE_READONLY);
}
#pragma GCC diagnostic pop

void debug_remove_single_time_breakpoint_from_address(uint64_t address) {
    const debug_breakpoint_t* breakpoint = hashmap_get(debug_location_map, (void*)address);

    if(!breakpoint) {
        return;
    }

    *((uint8_t*)address) = breakpoint->original_instruction;

    hashmap_delete(debug_location_map, (void*)address);

    memory_free((void*)breakpoint);
}

void debug_put_breakpoint_at_symbol(const char_t* symbol_name) {
    uint64_t address = debug_get_symbol_address(symbol_name);

    if(!address) {
        PRINTLOG(KERNEL, LOG_ERROR, "Symbol %s not found", symbol_name);
        return;
    }

    PRINTLOG(KERNEL, LOG_INFO, "Putting breakpoint at %s (0x%016llx)", symbol_name, address);

    debug_put_single_time_breakpoint_at_address(address);
}

void debug_remove_breakpoint_from_symbol(const char_t* symbol_name) {
    uint64_t address = debug_get_symbol_address(symbol_name);

    if(!address) {
        return;
    }

    debug_remove_single_time_breakpoint_from_address(address);
}

uint8_t debug_get_original_byte(uint64_t address) {
    const debug_breakpoint_t* breakpoint = hashmap_get(debug_location_map, (void*)address);

    if(!breakpoint) {
        return 0;
    }

    return breakpoint->original_instruction;
}

void debug_revert_original_byte_at_address(uint64_t address) {
    const debug_breakpoint_t* breakpoint = hashmap_get(debug_location_map, (void*)address);

    if(!breakpoint) {
        return;
    }

    memory_paging_toggle_attributes(address, MEMORY_PAGING_PAGE_TYPE_READONLY);
    *((uint8_t*)address) = breakpoint->original_instruction;
    memory_paging_toggle_attributes(address, MEMORY_PAGING_PAGE_TYPE_READONLY);
}
