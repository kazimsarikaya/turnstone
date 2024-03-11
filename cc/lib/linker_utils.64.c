/**
 * @file linker_utils.64.c
 * @brief Linker implementation for both efi and turnstone executables.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <linker.h>
#include <cpu.h>
#include <memory.h>
#include <memory/frame.h>
#include <memory/paging.h>
#include <systeminfo.h>
#include <logging.h>
#include <strings.h>
#include <efi.h>
#include <list.h>
#include <hashmap.h>

MODULE("turnstone.lib.linker");


typedef struct linker_module_at_memory_t {
    uint64_t id;
    uint64_t module_name_offset;
    uint64_t physical_start;
    uint64_t virtual_start;
} linker_module_at_memory_t;

typedef struct linker_section_at_memory_t {
    uint64_t section_type;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t size;
} linker_section_at_memory_t;

typedef union linker_metadata_at_memory_t {
    linker_module_at_memory_t  module;
    linker_section_at_memory_t section;
} linker_metadata_at_memory_t;

hashmap_t* linker_modules_at_memory = NULL;

void linker_build_modules_at_memory(void) {
    if(linker_modules_at_memory) {
        return;
    }

    linker_modules_at_memory = hashmap_integer(128);

    if(!linker_modules_at_memory) {
        return;
    }

    program_header_t* program_header = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;

    linker_metadata_at_memory_t* module_or_section = (linker_metadata_at_memory_t *)program_header->metadata_virtual_address;

    while (module_or_section->module.id != 0) {
        hashmap_put(linker_modules_at_memory, (void*)module_or_section->module.id, module_or_section);
        module_or_section++;

        while(module_or_section->section.size){
            module_or_section++;
        }

        module_or_section++;
    }
}

void linker_print_modules_at_memory(void) {
    if(!linker_modules_at_memory) {
        PRINTLOG(LINKER, LOG_ERROR, "Linker modules at memory not initialized\n");
        return;
    }

    program_header_t* program_header = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;
    const char_t* symbol_names = (const char_t*)program_header->symbol_table_virtual_address;

    iterator_t* iter = hashmap_iterator_create(linker_modules_at_memory);

    if(!iter) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator\n");
        return;
    }

    while(iter->end_of_iterator(iter) != 0){
        const linker_metadata_at_memory_t* module_or_section = iter->get_item(iter);

        if(!module_or_section) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get item\n");
            iter->destroy(iter);
            return;
        }

        printf("Module: 0x%llx ", module_or_section->module.id);

        if(symbol_names) {
            printf("%s\n", symbol_names + module_or_section->module.module_name_offset);
        } else {
            printf("\n");
        }

        printf("\tVirtual start: 0x%llx\n", module_or_section->module.virtual_start);
        printf("\tPhysical start: 0x%llx\n", module_or_section->module.physical_start);

        iter = iter->next(iter);
    }

    iter->destroy(iter);
}

void linker_print_module_info_at_memory(uint64_t module_id) {
    if(!linker_modules_at_memory) {
        PRINTLOG(LINKER, LOG_ERROR, "Linker modules at memory not initialized\n");
        return;
    }

    program_header_t* program_header = (program_header_t*)SYSTEM_INFO->program_header_virtual_start;
    const char_t* symbol_names = (const char_t*)program_header->symbol_table_virtual_address;

    const linker_metadata_at_memory_t* module_or_section = hashmap_get(linker_modules_at_memory, (void*)module_id);

    if(!module_or_section) {
        PRINTLOG(LINKER, LOG_ERROR, "Module not found\n");
        return;
    }

    printf("Module: 0x%llx ", module_or_section->module.id);

    if(symbol_names) {
        printf("%s\n", symbol_names + module_or_section->module.module_name_offset);
    } else {
        printf("\n");
    }

    printf("\tVirtual start: 0x%llx\n", module_or_section->module.virtual_start);
    printf("\tPhysical start: 0x%llx\n", module_or_section->module.physical_start);

    module_or_section++;

    printf("\tSection locations:\n");

    while(module_or_section->section.size){
        printf("\t\tVirtual start: 0x%llx ", module_or_section->section.virtual_start);
        printf("Physical start: 0x%llx ", module_or_section->section.physical_start);
        printf("Size: 0x%llx ", module_or_section->section.size);
        printf("Type: 0x%llx %s\n",
               module_or_section->section.section_type,
               linker_section_type_names[module_or_section->section.section_type]);

        module_or_section++;
    }

}
