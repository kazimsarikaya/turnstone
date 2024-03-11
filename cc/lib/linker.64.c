/**
 * @file linker.64.c
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

MODULE("turnstone.lib.linker");

int8_t    linker_link_module(linker_context_t* ctx, linker_module_t* module);
int8_t    linker_efi_image_relocation_entry_cmp(const void* a, const void* b);
int8_t    linker_efi_image_section_header_cmp(const void* a, const void* b);
buffer_t* linker_build_relocation_table_buffer(linker_context_t* ctx);
buffer_t* linker_build_metadata_buffer(linker_context_t* ctx);

const char_t*const linker_section_type_names[LINKER_SECTION_TYPE_NR_SECTIONS] = {
    [LINKER_SECTION_TYPE_TEXT] = ".text",
    [LINKER_SECTION_TYPE_DATA] = ".data",
    [LINKER_SECTION_TYPE_DATARELOC] = ".datareloc",
    [LINKER_SECTION_TYPE_RODATA] = ".rodata",
    [LINKER_SECTION_TYPE_RODATARELOC] = ".rodatareloc",
    [LINKER_SECTION_TYPE_BSS] = ".bss",
    [LINKER_SECTION_TYPE_RELOCATION_TABLE] = ".reloc",
    [LINKER_SECTION_TYPE_GOT_RELATIVE_RELOCATION_TABLE] = ".gotrel",
    [LINKER_SECTION_TYPE_GOT] = ".got",
    [LINKER_SECTION_TYPE_STACK] = ".stack",
    [LINKER_SECTION_TYPE_HEAP] = ".heap",
};

int8_t linker_efi_image_relocation_entry_cmp(const void* a, const void* b) {
    efi_image_relocation_entry_t* entry_a = (efi_image_relocation_entry_t*)a;
    efi_image_relocation_entry_t* entry_b = (efi_image_relocation_entry_t*)b;

    if(entry_a->page_rva < entry_b->page_rva) {
        return -1;
    } else if(entry_a->page_rva > entry_b->page_rva) {
        return 1;
    }

    return 0;
}

int8_t linker_efi_image_section_header_cmp(const void* a, const void* b) {
    efi_image_section_header_t* header_a = (efi_image_section_header_t*)a;
    efi_image_section_header_t* header_b = (efi_image_section_header_t*)b;

    if(header_a->virtual_address < header_b->virtual_address) {
        return -1;
    } else if(header_a->virtual_address > header_b->virtual_address) {
        return 1;
    }

    return 0;
}

int8_t linker_destroy_context(linker_context_t* ctx) {
    hashmap_destroy(ctx->got_symbol_index_map);
    buffer_destroy(ctx->got_table_buffer);

    iterator_t* it = hashmap_iterator_create(ctx->modules);

    if(!it) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator for modules");

        return -1;
    }

    while(it->end_of_iterator(it) != 0) {
        linker_module_t* module = (linker_module_t*)it->get_item(it);

        if(!module) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get module from iterator");

            return -1;
        }

        for(uint8_t i = 0; i < LINKER_SECTION_TYPE_NR_SECTIONS; i++) {
            buffer_destroy(module->sections[i].section_data);
        }

        memory_free(module);

        it = it->next(it);
    }

    it->destroy(it);

    hashmap_destroy(ctx->modules);

    memory_free(ctx);

    return 0;
}


int8_t linker_build_symbols(linker_context_t* ctx, uint64_t module_id, uint64_t section_id, uint8_t section_type, uint64_t section_offset) {
    int8_t res = 0;

    tosdb_database_t* db_system = tosdb_database_create_or_open(ctx->tdb, "system");
    tosdb_table_t* tbl_symbols = tosdb_table_create_or_open(db_system, "symbols", 1 << 10, 512 << 10, 8);

    tosdb_record_t* s_sym_rec = tosdb_table_create_record(tbl_symbols);

    if(!s_sym_rec) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create record for searching symbols");

        return -1;
    }

    if(!s_sym_rec->set_uint64(s_sym_rec, "section_id", section_id)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot set search key for records section_id column for section id 0x%llx", section_id);
        s_sym_rec->destroy(s_sym_rec);

        return -1;
    }

    list_t* symbols = s_sym_rec->search_record(s_sym_rec);

    s_sym_rec->destroy(s_sym_rec);

    if(!symbols) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot search symbols for section id 0x%llx", section_id);

        return -1;
    }

    PRINTLOG(LINKER, LOG_DEBUG, "found %llu symbols for section id 0x%llx", list_size(symbols), section_id);

    iterator_t* it = list_iterator_create(symbols);

    if(!it) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator for symbols");

        return -1;
    }

    linker_global_offset_table_entry_t got_entry = {0};
    uint64_t symbol_id = 0;
    uint8_t symbol_type = 0;
    uint8_t symbol_scope = 0;
    uint64_t symbol_value = 0;
    uint64_t symbol_size = 0;
    char_t* symbol_name = NULL;

    while(it->end_of_iterator(it) != 0) {
        tosdb_record_t* sym_rec = (tosdb_record_t*)it->delete_item(it);

        if(!sym_rec) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get symbol record");

            goto clean_symbols_iter;
        }

        if(!sym_rec->get_uint64(sym_rec, "id", &symbol_id)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get symbol id");

            goto clean_symbols_iter;
        }

        if(!sym_rec->get_uint8(sym_rec, "type", &symbol_type)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get symbol type");

            goto clean_symbols_iter;
        }

        if(!sym_rec->get_uint8(sym_rec, "scope", &symbol_scope)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get symbol scope");

            goto clean_symbols_iter;
        }

        if(!sym_rec->get_uint64(sym_rec, "value", &symbol_value)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get symbol value");

            goto clean_symbols_iter;
        }

        if(!sym_rec->get_uint64(sym_rec, "size", &symbol_size)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get symbol size");

            goto clean_symbols_iter;
        }

        if(!sym_rec->get_string(sym_rec, "name", &symbol_name)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get symbol name");

            goto clean_symbols_iter;
        }

        PRINTLOG(LINKER, LOG_DEBUG, "found symbol %s with id 0x%llx size 0x%llx, at section 0x%llx", symbol_name, symbol_id, symbol_size, section_id);

        uint64_t got_entry_index = (uint64_t)hashmap_get(ctx->got_symbol_index_map, (void*)symbol_id);

        if(got_entry_index) {
            linker_global_offset_table_entry_t* existing_got_entry = (linker_global_offset_table_entry_t*)buffer_get_view_at_position(ctx->got_table_buffer, got_entry_index * sizeof(linker_global_offset_table_entry_t), sizeof(linker_global_offset_table_entry_t));

            if(!existing_got_entry) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot get existing got entry");
                memory_free(symbol_name);

                goto clean_symbols_iter;
            }

            if(existing_got_entry->symbol_id != symbol_id || existing_got_entry->module_id != module_id) {
                PRINTLOG(LINKER, LOG_ERROR, "got entry symbol/module id mismatch");
                memory_free(symbol_name);

                goto clean_symbols_iter;
            }

            existing_got_entry->resolved = true;
            existing_got_entry->symbol_type = symbol_type;
            existing_got_entry->symbol_scope = symbol_scope;
            existing_got_entry->symbol_value = symbol_value + section_offset;
            existing_got_entry->symbol_size = symbol_size;
            existing_got_entry->section_type = section_type;

        } else {
            memory_memclean(&got_entry, sizeof(linker_global_offset_table_entry_t));

            got_entry.resolved = true;
            got_entry.module_id = module_id;
            got_entry.symbol_id = symbol_id;
            got_entry.symbol_type = symbol_type;
            got_entry.symbol_scope = symbol_scope;
            got_entry.symbol_value = symbol_value + section_offset;
            got_entry.symbol_size = symbol_size;
            got_entry.section_type = section_type;

            if(ctx->symbol_table_buffer) {
                uint64_t symbol_table_index = buffer_get_length(ctx->symbol_table_buffer);

                buffer_append_bytes(ctx->symbol_table_buffer, (uint8_t*)symbol_name, strlen(symbol_name) + 1);

                got_entry.symbol_name_offset = symbol_table_index;
            }

            got_entry_index = buffer_get_length(ctx->got_table_buffer) / sizeof(linker_global_offset_table_entry_t);

            buffer_append_bytes(ctx->got_table_buffer, (uint8_t*)&got_entry, sizeof(linker_global_offset_table_entry_t));

            hashmap_put(ctx->got_symbol_index_map, (void*)symbol_id, (void*)got_entry_index);
        }

        PRINTLOG(LINKER, LOG_DEBUG, "added symbol %s with id 0x%llx, at section 0x%llx, to got table at index 0x%llx", symbol_name, symbol_id, section_id, got_entry_index);

        memory_free(symbol_name);

        sym_rec->destroy(sym_rec);

        it = it->next(it);
    }

    it->destroy(it);

    list_destroy(symbols);

    return res;

clean_symbols_iter:
    while(it->end_of_iterator(it) != 0) {
        tosdb_record_t* sym_rec = (tosdb_record_t*)it->delete_item(it);

        if(sym_rec) {
            sym_rec->destroy(sym_rec);
        }

        it = it->next(it);
    }

    it->destroy(it);

    list_destroy(symbols);

    return -1;
}

int8_t linker_build_relocations(linker_context_t* ctx, uint64_t section_id, uint8_t section_type, uint64_t section_offset, linker_section_t* section, boolean_t recursive) {
    int8_t res = 0;

    tosdb_database_t* db_system = tosdb_database_create_or_open(ctx->tdb, "system");
    tosdb_table_t* tbl_sections = tosdb_table_create_or_open(db_system, "sections", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_relocations = tosdb_table_create_or_open(db_system, "relocations", 1 << 10, 512 << 10, 8);

    tosdb_record_t* s_rel_reloc = tosdb_table_create_record(tbl_relocations);

    if(!s_rel_reloc) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create record for searching relocations");

        return -1;
    }

    if(!s_rel_reloc->set_uint64(s_rel_reloc, "section_id", section_id)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot set search key for records section_id column for section id 0x%llx", section_id);
        s_rel_reloc->destroy(s_rel_reloc);

        return -1;
    }

    PRINTLOG(LINKER, LOG_TRACE, "searching relocations for section id 0x%llx", section_id);

    list_t* relocations = s_rel_reloc->search_record(s_rel_reloc);

    s_rel_reloc->destroy(s_rel_reloc);

    if(!relocations) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot search relocations for section id 0x%llx", section_id);

        return -1;
    }

    PRINTLOG(LINKER, LOG_DEBUG, "relocations count of section 0x%llx: 0x%llx", section_id, list_size(relocations));

    iterator_t* it = list_iterator_create(relocations);

    if(!it) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator for relocations");

        return -1;
    }

    linker_relocation_entry_t relocation = {0};
    int64_t reloc_id = 0;
    int64_t symbol_section_id = 0;
    int64_t symbol_id = 0;
    int8_t reloc_type = 0;
    int64_t reloc_offset = 0;
    int64_t reloc_addend = 0;
    char_t* symbol_name = NULL;
    int64_t module_id = 0;

    if(!section->section_data) {
        section->section_data = buffer_new();
    }

    while(it->end_of_iterator(it) != 0) {
        tosdb_record_t* reloc_rec = (tosdb_record_t*)it->delete_item(it);
        boolean_t is_got_symbol = false;
        boolean_t symbol_id_missing = false;

        if(!reloc_rec) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get relocation record");

            goto clean_relocs_iter;
        }

        PRINTLOG(LINKER, LOG_TRACE, "parsing relocation record");

        if(!reloc_rec->get_int64(reloc_rec, "id", &reloc_id)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get relocation id");
            reloc_rec->destroy(reloc_rec);

            goto clean_relocs_iter;
        }

        if(!reloc_rec->get_int64(reloc_rec, "symbol_id", &symbol_id)) {
            symbol_id_missing = true;
        }

        if(!reloc_rec->get_int64(reloc_rec, "symbol_section_id", &symbol_section_id)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get relocation symbol section id for relocation id 0x%llx", reloc_id);
            reloc_rec->destroy(reloc_rec);

            goto clean_relocs_iter;
        }

        if(!reloc_rec->get_string(reloc_rec, "symbol_name", &symbol_name)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get relocation symbol name for relocation id 0x%llx", reloc_id);
            reloc_rec->destroy(reloc_rec);

            goto clean_relocs_iter;
        }

        PRINTLOG(LINKER, LOG_DEBUG, "relocation 0x%llx symbol name: %s id 0x%llx", reloc_id, symbol_name, symbol_id);


        if(strcmp(symbol_name, "_GLOBAL_OFFSET_TABLE_") == 0) {
            PRINTLOG(LINKER, LOG_TRACE, "found _GLOBAL_OFFSET_TABLE_ symbol for relocation at section 0x%llx", section_id);
            is_got_symbol = true;
            symbol_id = LINKER_GOT_SYMBOL_ID;
        }

        if(symbol_id_missing && !is_got_symbol) {
            PRINTLOG(LINKER, LOG_ERROR, "symbol id is missing for symbol %s, relocation at section 0x%llx relocation id 0x%llx", symbol_name, section_id, reloc_id);
            reloc_rec->destroy(reloc_rec);
            memory_free(symbol_name);

            goto clean_relocs_iter;
        }

        if(symbol_section_id == 0) {
            if(!is_got_symbol) {
                PRINTLOG(LINKER, LOG_ERROR, "symbol section id is missing for symbol %s(%lli), relocation at section 0x%llx relocation id 0x%llx", symbol_name, symbol_id, section_id, reloc_id);
                PRINTLOG(LINKER, LOG_ERROR, "relocation record deleted? %s", reloc_rec->is_deleted(reloc_rec) ? "yes" : "no");

                reloc_rec->destroy(reloc_rec);

                memory_free(symbol_name);

                goto clean_relocs_iter;
            } else {
                symbol_section_id = LINKER_GOT_SECTION_ID;
            }
        }

        memory_free(symbol_name);

        if(!reloc_rec->get_int8(reloc_rec, "type", &reloc_type)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get relocation type for relocation id 0x%llx", reloc_id);
            reloc_rec->destroy(reloc_rec);

            goto clean_relocs_iter;
        }

        if(!reloc_rec->get_int64(reloc_rec, "offset", &reloc_offset)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get relocation offset for relocation id 0x%llx", reloc_id);
            reloc_rec->destroy(reloc_rec);

            goto clean_relocs_iter;
        }

        if(!reloc_rec->get_int64(reloc_rec, "addend", &reloc_addend)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get relocation addend for relocation id 0x%llx", reloc_id);
            reloc_rec->destroy(reloc_rec);

            goto clean_relocs_iter;
        }

        if(!is_got_symbol) {
            tosdb_record_t* s_sec_rec = tosdb_table_create_record(tbl_sections);

            if(!s_sec_rec) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot create record for searching section");
                reloc_rec->destroy(reloc_rec);

                goto clean_relocs_iter;
            }

            if(!s_sec_rec->set_uint64(s_sec_rec, "id", symbol_section_id)) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot set search key for records id column for section id 0x%llx", symbol_section_id);
                reloc_rec->destroy(reloc_rec);
                s_sec_rec->destroy(s_sec_rec);

                goto clean_relocs_iter;
            }

            if(!s_sec_rec->get_record(s_sec_rec)) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot get section record for section id 0x%llx for relocation 0x%llx", symbol_section_id, reloc_id);
                reloc_rec->destroy(reloc_rec);
                s_sec_rec->destroy(s_sec_rec);

                goto clean_relocs_iter;
            }

            if(!s_sec_rec->get_int64(s_sec_rec, "module_id", &module_id)) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot get section module id, is deleted? %d", s_sec_rec->is_deleted(s_sec_rec));
                reloc_rec->destroy(reloc_rec);
                s_sec_rec->destroy(s_sec_rec);

                goto clean_relocs_iter;
            }

            s_sec_rec->destroy(s_sec_rec);

            PRINTLOG(LINKER, LOG_DEBUG, "relocation 0x%llx source symbol section id 0x%llx", reloc_id, symbol_section_id);
        }

        memory_memclean(&relocation, sizeof(linker_relocation_entry_t));

        relocation.symbol_id = symbol_id;
        relocation.section_type = section_type;
        relocation.relocation_type = reloc_type;
        relocation.offset = reloc_offset + section_offset;
        relocation.addend = reloc_addend;

        buffer_append_bytes(section->section_data, (uint8_t*)&relocation, sizeof(linker_relocation_entry_t));
        section->size += sizeof(linker_relocation_entry_t);

        if(!is_got_symbol) {
            PRINTLOG(LINKER, LOG_TRACE, "check if symbol 0x%llx loaded?", symbol_id);
            uint64_t got_index = (uint64_t)hashmap_get(ctx->got_symbol_index_map, (void*)symbol_id);

            if(!got_index) {
                if(recursive) {
                    PRINTLOG(LINKER, LOG_TRACE, "cannot get got index for symbol 0x%llx for module 0x%llx, recursive loading", symbol_id, module_id);

                    int8_t recursive_res = linker_build_module(ctx, module_id, recursive);

                    if( recursive_res == -1) {
                        PRINTLOG(LINKER, LOG_ERROR, "cannot build module for got symbol 0x%llx module 0x%llx", symbol_id, module_id);
                        reloc_rec->destroy(reloc_rec);

                        goto clean_relocs_iter;
                    } else if(recursive_res == -2) {
                        PRINTLOG(LINKER, LOG_TRACE, "module 0x%llx still loading", module_id);
                    } else {
                        got_index = (uint64_t)hashmap_get(ctx->got_symbol_index_map, (void*)symbol_id);

                        if(!got_index) {
                            PRINTLOG(LINKER, LOG_ERROR, "cannot get got index for symbol 0x%llx after recursive loading", symbol_id);
                            reloc_rec->destroy(reloc_rec);

                            goto clean_relocs_iter;
                        } else {
                            PRINTLOG(LINKER, LOG_TRACE, "symbol 0x%llx loaded, got index 0x%llx", symbol_id, got_index);
                        }
                    }

                } else {
                    linker_global_offset_table_entry_t got_entry = {0};
                    memory_memclean(&got_entry, sizeof(linker_global_offset_table_entry_t));

                    got_entry.module_id = module_id;
                    got_entry.symbol_id = symbol_id;

                    got_index = buffer_get_length(ctx->got_table_buffer) / sizeof(linker_global_offset_table_entry_t);

                    buffer_append_bytes(ctx->got_table_buffer, (uint8_t*)&got_entry, sizeof(linker_global_offset_table_entry_t));

                    hashmap_put(ctx->got_symbol_index_map, (void*)symbol_id, (void*)got_index);

                }
            }
        }

        reloc_rec->destroy(reloc_rec);

        it = it->next(it);
    }

    it->destroy(it);

    list_destroy(relocations);

    return res;

clean_relocs_iter:
    while(it->end_of_iterator(it) != 0) {
        tosdb_record_t* reloc_rec = (tosdb_record_t*)it->delete_item(it);

        if(reloc_rec) {
            reloc_rec->destroy(reloc_rec);
        }

        it = it->next(it);
    }

    it->destroy(it);

    list_destroy(relocations);

    return -1;
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t linker_build_module(linker_context_t* ctx, uint64_t module_id, boolean_t recursive) {
    int8_t res = 0;

    tosdb_database_t* db_system = tosdb_database_create_or_open(ctx->tdb, "system");
    tosdb_table_t* tbl_sections = tosdb_table_create_or_open(db_system, "sections", 1 << 10, 512 << 10, 8);

    linker_module_t* module = (linker_module_t*)hashmap_get(ctx->modules, (void*)module_id);

    if(!module) {
        module = memory_malloc(sizeof(linker_module_t));

        if(!module) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot allocate memory for module 0x%llx", module_id);

            return -1;
        }

        if(ctx->symbol_table_buffer) {
            tosdb_table_t* tbl_modules = tosdb_table_create_or_open(db_system, "modules", 1 << 10, 512 << 10, 8);

            tosdb_record_t* s_mod_rec = tosdb_table_create_record(tbl_modules);

            if(!s_mod_rec) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot create record for searching modules");

                return -1;
            }

            if(!s_mod_rec->set_uint64(s_mod_rec, "id", module_id)) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot set search key for records id column for module id 0x%llx", module_id);
                s_mod_rec->destroy(s_mod_rec);

                return -1;
            }

            if(!s_mod_rec->get_record(s_mod_rec)) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot get module record for module id 0x%llx", module_id);
                s_mod_rec->destroy(s_mod_rec);

                return -1;
            }

            char_t* module_name = NULL;

            if(!s_mod_rec->get_string(s_mod_rec, "name", &module_name)) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot get module name for module id 0x%llx", module_id);
                s_mod_rec->destroy(s_mod_rec);

                return -1;
            }

            s_mod_rec->destroy(s_mod_rec);

            uint64_t symbol_table_index = buffer_get_length(ctx->symbol_table_buffer);

            buffer_append_bytes(ctx->symbol_table_buffer, (uint8_t*)module_name, strlen(module_name) + 1);

            module->module_name_offset = symbol_table_index;

            memory_free(module_name);
        }

        module->id = module_id;
        hashmap_put(ctx->modules, (void*)module_id, module);
    } else {
        if(recursive) {
            return -2;
        }
    }

    tosdb_record_t* s_sec_rec = tosdb_table_create_record(tbl_sections);

    if(!s_sec_rec) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create record for searching sections");

        return -1;
    }

    if(!s_sec_rec->set_uint64(s_sec_rec, "module_id", module_id)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot set search key for records module_id column for module id 0x%llx", module_id);
        s_sec_rec->destroy(s_sec_rec);

        return -1;
    }

    list_t* sections = s_sec_rec->search_record(s_sec_rec);

    s_sec_rec->destroy(s_sec_rec);

    if(!sections) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot search sections for module id 0x%llx", module_id);

        return -1;
    }

    PRINTLOG(LINKER, LOG_DEBUG, "module 0x%llx sections count: %llu", module_id, list_size(sections));


    uint64_t section_id = 0;
    uint8_t section_type = 0;
    uint8_t* section_data = NULL;
    uint64_t section_size = 0;
    uint64_t tmp_section_size = 0;
    int64_t section_alignment = 0;
    char_t* section_name = NULL;

    uint64_t section_offset = 0;

    iterator_t* it = list_iterator_create(sections);

    while(it->end_of_iterator(it) != 0) {
        tosdb_record_t* sec_rec = (tosdb_record_t*)it->delete_item(it);

        if(!sec_rec) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get section record");

            goto clean_secs_iter;
        }

        if(!sec_rec->get_uint64(sec_rec, "id", &section_id)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get section id");
            sec_rec->destroy(sec_rec);

            goto clean_secs_iter;
        }

        if(!sec_rec->get_uint8(sec_rec, "type", &section_type)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get section type");
            sec_rec->destroy(sec_rec);

            goto clean_secs_iter;
        }

        if(!sec_rec->get_uint64(sec_rec, "size", &section_size)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get section size");
            sec_rec->destroy(sec_rec);

            goto clean_secs_iter;
        }

        if(!sec_rec->get_int64(sec_rec, "alignment", &section_alignment)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get section aligment");
            sec_rec->destroy(sec_rec);

            goto clean_secs_iter;
        }

        uint64_t padding = 0;

        if(module->sections[section_type].size % section_alignment) {
            padding = section_alignment - (module->sections[section_type].size % section_alignment);
        }

        module->sections[section_type].size += padding;

        if(section_type != LINKER_SECTION_TYPE_BSS) {
            section_data = NULL;

            if(!sec_rec->get_bytearray(sec_rec, "value", &tmp_section_size, &section_data)) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot get section data");
                sec_rec->destroy(sec_rec);

                goto clean_secs_iter;
            }

            if(!section_data) {
                PRINTLOG(LINKER, LOG_ERROR, "section data is NULL");
                sec_rec->destroy(sec_rec);

                goto clean_secs_iter;
            }

            if(tmp_section_size != section_size) {
                PRINTLOG(LINKER, LOG_ERROR, "section size mismatch");
                memory_free(section_data);
                sec_rec->destroy(sec_rec);

                goto clean_secs_iter;
            }

            if(!module->sections[section_type].section_data) {
                module->sections[section_type].section_data = buffer_new();
            }

            uint8_t zero = 0x0;

            for(uint64_t i = 0; i < padding; i++) {
                buffer_append_byte(module->sections[section_type].section_data, zero);
            }

            section_offset = buffer_get_length(module->sections[section_type].section_data);

            if(section_offset % section_alignment) {
                PRINTLOG(LINKER, LOG_ERROR, "section offset alignment mismatch");
                memory_free(section_data);
                sec_rec->destroy(sec_rec);

                goto clean_secs_iter;
            }

            if(section_offset != module->sections[section_type].size) {
                PRINTLOG(LINKER, LOG_ERROR, "section offset mismatch");
                memory_free(section_data);
                sec_rec->destroy(sec_rec);

                goto clean_secs_iter;
            }

            if(!buffer_append_bytes(module->sections[section_type].section_data, section_data, section_size)) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot append section data");
                memory_free(section_data);
                sec_rec->destroy(sec_rec);

                goto clean_secs_iter;
            }

            uint64_t section_end = buffer_get_length(module->sections[section_type].section_data);

            if(section_end != section_offset + section_size) {
                PRINTLOG(LINKER, LOG_ERROR, "section end mismatch");
                memory_free(section_data);
                sec_rec->destroy(sec_rec);

                goto clean_secs_iter;
            }

            memory_free(section_data);
        } else {
            section_offset = module->sections[section_type].size;
        }

        if(!sec_rec->get_string(sec_rec, "name", &section_name)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get section name");
            sec_rec->destroy(sec_rec);

            goto clean_secs_iter;
        }

        PRINTLOG(LINKER, LOG_DEBUG, "module id 0x%llx section id: 0x%llx, type: %u, name: %s offset 0x%llx alignment 0x%llx size 0x%llx, padding 0x%llx",
                 module_id, section_id, section_type, section_name, section_offset, section_alignment, section_size, padding);

        memory_free(section_name);

        if(linker_build_symbols(ctx, module_id, section_id, section_type, section_offset) != 0) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot build symbols for section id 0x%llx", section_id);
            sec_rec->destroy(sec_rec);

            goto clean_secs_iter;
        }

        if(linker_build_relocations(ctx,  section_id, section_type, section_offset, &module->sections[LINKER_SECTION_TYPE_RELOCATION_TABLE], recursive) != 0) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot build relocations for section id 0x%llx", section_id);
            sec_rec->destroy(sec_rec);

            goto clean_secs_iter;
        }

        module->sections[section_type].size += section_size;

        sec_rec->destroy(sec_rec);

        it = it->next(it);
    }

    it->destroy(it);

    list_destroy(sections);

    PRINTLOG(LINKER, LOG_DEBUG, "module id 0x%llx built", module_id);

    return res;

clean_secs_iter:
    while(it->end_of_iterator(it) != 0) {
        tosdb_record_t* sec_rec = (tosdb_record_t*)it->delete_item(it);

        if(sec_rec) {
            sec_rec->destroy(sec_rec);
        }

        it = it->next(it);
    }

    it->destroy(it);

    list_destroy(sections);

    return -1;
}
#pragma GCC diagnostic pop

int8_t linker_calculate_program_size(linker_context_t* ctx) {
    if(!ctx) {
        return -1;
    }

    uint64_t relocation_table_size = 0;
    uint64_t metadata_size = 0;

    iterator_t* it = hashmap_iterator_create(ctx->modules);

    if(!it) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator");

        return -1;
    }

    while(it->end_of_iterator(it) != 0) {
        linker_module_t* module = (linker_module_t*)it->get_item(it);

        metadata_size += 24; // id, physical_start, virtual_start bytes

        for(int32_t i = 0; i < LINKER_SECTION_TYPE_RELOCATION_TABLE; i++) {
            if(module->sections[i].size) {
                metadata_size += 32; // section id, physical_start, virtual_start, size bytes

                if(module->sections[i].size % 0x1000) {
                    ctx->program_size += module->sections[i].size + (0x1000 - (module->sections[i].size % 0x1000));
                } else {
                    ctx->program_size += module->sections[i].size;
                }
            }
        }

        if(module->sections[LINKER_SECTION_TYPE_RELOCATION_TABLE].size) {
            relocation_table_size += 16 +  module->sections[LINKER_SECTION_TYPE_RELOCATION_TABLE].size;
        }

        it = it->next(it);
    }

    it->destroy(it);

    if(ctx->program_size % 0x1000) {
        ctx->program_size += 0x1000 - (ctx->program_size % 0x1000);
    }

    ctx->global_offset_table_size = buffer_get_length(ctx->got_table_buffer);

    if(ctx->global_offset_table_size % 0x1000) {
        ctx->global_offset_table_size += 0x1000 - (ctx->global_offset_table_size % 0x1000);
    }

    ctx->relocation_table_size = relocation_table_size;

    if(ctx->relocation_table_size % 0x1000) {
        ctx->relocation_table_size += 0x1000 - (ctx->relocation_table_size % 0x1000);
    }

    ctx->metadata_size = metadata_size;

    if(ctx->metadata_size % 0x1000) {
        ctx->metadata_size += 0x1000 - (ctx->metadata_size % 0x1000);
    }

    if(ctx->symbol_table_buffer) {
        ctx->symbol_table_size = buffer_get_length(ctx->symbol_table_buffer);

        if(ctx->symbol_table_size % 0x1000) {
            ctx->symbol_table_size += 0x1000 - (ctx->symbol_table_size % 0x1000);
        }
    }

    PRINTLOG(LINKER, LOG_INFO, "program size 0x%llx got size 0x%llx relocation table size 0x%llx metadata size 0x%llx symbol table size 0x%llx",
             ctx->program_size, ctx->global_offset_table_size, ctx->relocation_table_size, ctx->metadata_size, ctx->symbol_table_size);

    return 0;
}

int8_t linker_bind_linear_addresses(linker_context_t* ctx) {
    if(!ctx) {
        return -1;
    }

    uint64_t offset_pyhsical = ctx->program_start_physical;
    uint64_t offset_virtual = ctx->program_start_virtual;

    iterator_t* it = hashmap_iterator_create(ctx->modules);

    if(!it) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator");

        return -1;
    }

    while(it->end_of_iterator(it) != 0) {
        linker_module_t* module = (linker_module_t*)it->get_item(it);

        module->physical_start = offset_pyhsical;
        module->virtual_start = offset_virtual;

        for(int32_t i = 0; i < LINKER_SECTION_TYPE_RELOCATION_TABLE; i++) {
            if(module->sections[i].size) {
                module->sections[i].physical_start = offset_pyhsical;
                module->sections[i].virtual_start = offset_virtual;

                offset_pyhsical += module->sections[i].size;
                offset_virtual += module->sections[i].size;

                if(offset_pyhsical % 0x1000) {
                    offset_pyhsical += 0x1000 - (offset_pyhsical % 0x1000);
                    offset_virtual += 0x1000 - (offset_virtual % 0x1000);
                }
            }
        }

        it = it->next(it);
    }

    it->destroy(it);

    ctx->got_address_physical = offset_pyhsical;
    ctx->got_address_virtual = offset_virtual;

    return 0;
}

int64_t linker_get_section_count_without_relocations(linker_context_t* ctx) {
    if(!ctx) {
        return -1;
    }

    iterator_t* it = hashmap_iterator_create(ctx->modules);

    if(!it) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator");

        return -1;
    }

    int64_t count = 0;

    while(it->end_of_iterator(it) != 0) {
        linker_module_t* module = (linker_module_t*)it->get_item(it);

        for(int32_t i = 0; i < LINKER_SECTION_TYPE_RELOCATION_TABLE; i++) {
            if(module->sections[i].size) {
                count++;
            }
        }

        it = it->next(it);
    }

    it->destroy(it);

    return count;
}

int8_t linker_bind_got_entry_values(linker_context_t* ctx) {
    if(!ctx) {
        return -1;
    }

    uint64_t got_size = buffer_get_length(ctx->got_table_buffer);

    if(got_size == 0) {
        PRINTLOG(LINKER, LOG_ERROR, "GOT table is empty");

        return -1;
    }

    uint64_t got_entry_count = got_size / sizeof(linker_global_offset_table_entry_t);

    if(got_entry_count == 0) {
        PRINTLOG(LINKER, LOG_ERROR, "GOT table is empty");

        return -1;
    }

    linker_global_offset_table_entry_t* got_entries = (linker_global_offset_table_entry_t*)buffer_get_view_at_position(ctx->got_table_buffer, 0, got_size);

    for(uint64_t i = 0; i < got_entry_count; i++) {
        if(got_entries[i].resolved) {
            linker_module_t* module = (linker_module_t*)hashmap_get(ctx->modules, (void*)got_entries[i].module_id);

            if(!module) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot get module with id 0x%llx", got_entries[i].module_id);

                return -1;
            }

            got_entries[i].entry_value = module->sections[got_entries[i].section_type].virtual_start + got_entries[i].symbol_value;
        }
    }

    uint64_t entry_point_got_index = (uint64_t)hashmap_get(ctx->got_symbol_index_map, (void*)ctx->entrypoint_symbol_id);

    if(entry_point_got_index == 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot get entry point GOT index");

        return -1;
    }

    ctx->entrypoint_address_virtual = got_entries[entry_point_got_index].entry_value;

    return 0;
}

boolean_t linker_is_all_symbols_resolved(linker_context_t* ctx) {
    if(!ctx) {
        return false;
    }

    uint64_t got_size = buffer_get_length(ctx->got_table_buffer);

    if(got_size == 0) {
        return false;
    }

    uint64_t got_entry_count = got_size / sizeof(linker_global_offset_table_entry_t);

    if(got_entry_count == 0) {
        return false;
    }

    linker_global_offset_table_entry_t* got_entries = (linker_global_offset_table_entry_t*)buffer_get_view_at_position(ctx->got_table_buffer, 0, got_size);

    uint64_t unresolved_count = 0;

    for(uint64_t i = 0; i < got_entry_count; i++) {
        if(!got_entries[i].resolved) {
            unresolved_count++;
        }
    }

    return unresolved_count == 2;
}

int8_t linker_link_module(linker_context_t* ctx, linker_module_t* module) {
    if(!ctx || !module) {
        PRINTLOG(LINKER, LOG_ERROR, "invalid context or module");

        return -1;
    }

    uint64_t reloc_size = buffer_get_length(module->sections[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_data);

    if(reloc_size == 0) {
        return 0;
    }

    uint64_t reloc_entry_count = reloc_size / sizeof(linker_relocation_entry_t);

    uint64_t got_size = buffer_get_length(ctx->got_table_buffer);

    if(got_size == 0) {
        PRINTLOG(LINKER, LOG_ERROR, "GOT table is empty");

        return -1;
    }

    linker_relocation_entry_t* reloc_entries = (linker_relocation_entry_t*)buffer_get_view_at_position(module->sections[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_data, 0, reloc_size);

    linker_global_offset_table_entry_t* got_entries = (linker_global_offset_table_entry_t*)buffer_get_view_at_position(ctx->got_table_buffer, 0, got_size);


    for(uint64_t reloc_id = 0; reloc_id < reloc_entry_count; reloc_id++) {
        uint64_t got_idx = (uint64_t)hashmap_get(ctx->got_symbol_index_map, (void*)reloc_entries[reloc_id].symbol_id);

        if(got_idx == 0) {
            if(reloc_entries[reloc_id].symbol_id != 1) {
                PRINTLOG(LINKER, LOG_ERROR, "invalid GOT index symbol id 0x%llx got index 0x%llx", reloc_entries[reloc_id].symbol_id, got_idx);

                return -1;
            } else {
                if(reloc_entries[reloc_id].relocation_type != LINKER_RELOCATION_TYPE_64_GOTPC64) {
                    PRINTLOG(LINKER, LOG_ERROR, "invalid relocation for got itself. relocation type 0x%x", reloc_entries[reloc_id].relocation_type);

                    return -1;
                }
            }
        }


        uint8_t* section_data = (uint8_t*)buffer_get_view_at_position(module->sections[reloc_entries[reloc_id].section_type].section_data, 0, module->sections[reloc_entries[reloc_id].section_type].size);


        if(reloc_entries[reloc_id].relocation_type == LINKER_RELOCATION_TYPE_64_32) {
            uint32_t* value = (uint32_t*)(section_data + reloc_entries[reloc_id].offset);
            *value = (uint32_t)(got_entries[got_idx].entry_value + reloc_entries[reloc_id].addend);
        } else if(reloc_entries[reloc_id].relocation_type == LINKER_RELOCATION_TYPE_64_32S) {
            int32_t* value = (int32_t*)(section_data + reloc_entries[reloc_id].offset);
            *value = (int32_t)got_entries[got_idx].entry_value + reloc_entries[reloc_id].addend;
        } else if(reloc_entries[reloc_id].relocation_type == LINKER_RELOCATION_TYPE_64_64) {
            uint64_t* value = (uint64_t*)(section_data + reloc_entries[reloc_id].offset);
            *value = got_entries[got_idx].entry_value + reloc_entries[reloc_id].addend;
        } else if(reloc_entries[reloc_id].relocation_type == LINKER_RELOCATION_TYPE_64_PC32) {
            uint32_t* value = (uint32_t*)(section_data + reloc_entries[reloc_id].offset);
            *value = (uint32_t)got_entries[got_idx].entry_value + reloc_entries[reloc_id].addend - (uint32_t)(module->sections[reloc_entries[reloc_id].section_type].virtual_start + reloc_entries[reloc_id].offset);
        } else if(reloc_entries[reloc_id].relocation_type == LINKER_RELOCATION_TYPE_64_PC64) {
            uint64_t* value = (uint64_t*)(section_data + reloc_entries[reloc_id].offset);
            *value = got_entries[got_idx].entry_value + reloc_entries[reloc_id].addend - (module->sections[reloc_entries[reloc_id].section_type].virtual_start + reloc_entries[reloc_id].offset);
        } else if(reloc_entries[reloc_id].relocation_type == LINKER_RELOCATION_TYPE_64_GOT64) {
            uint64_t* value = (uint64_t*)(section_data + reloc_entries[reloc_id].offset);
            *value = got_idx * sizeof(linker_global_offset_table_entry_t) + reloc_entries[reloc_id].addend;
        } else if(reloc_entries[reloc_id].relocation_type == LINKER_RELOCATION_TYPE_64_GOTOFF64) {
            uint64_t* value = (uint64_t*)(section_data + reloc_entries[reloc_id].offset);
            *value = got_entries[got_idx].entry_value + reloc_entries[reloc_id].addend - ctx->got_address_virtual;
        } else if(reloc_entries[reloc_id].relocation_type == LINKER_RELOCATION_TYPE_64_GOTPC64) {
            uint64_t* value = (uint64_t*)(section_data + reloc_entries[reloc_id].offset);
            *value = ctx->got_address_virtual + reloc_entries[reloc_id].addend - (module->sections[reloc_entries[reloc_id].section_type].virtual_start + reloc_entries[reloc_id].offset);
        } else {
            PRINTLOG(LINKER, LOG_ERROR, "invalid relocation type");

            return -1;
        }
    }

    return 0;
}


int8_t linker_link_program(linker_context_t* ctx) {
    if(!ctx) {
        PRINTLOG(LINKER, LOG_ERROR, "invalid context");

        return -1;
    }

    iterator_t* it = hashmap_iterator_create(ctx->modules);

    if(!it) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator");

        return -1;
    }

    while(it->end_of_iterator(it) != 0) {
        linker_module_t* module = (linker_module_t*)it->get_item(it);

        if(linker_link_module(ctx, module) < 0) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot link module");
            it->destroy(it);

            return -1;
        }

        it = it->next(it);
    }

    it->destroy(it);


    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
buffer_t* linker_build_efi_image_relocations(linker_context_t* ctx) {
    if(!ctx) {
        PRINTLOG(LINKER, LOG_ERROR, "invalid context");

        return NULL;
    }

    list_t* relocations_list = list_create_sortedlist(linker_efi_image_relocation_entry_cmp);

    if(!relocations_list) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create list");

        return NULL;
    }

    iterator_t* it = hashmap_iterator_create(ctx->modules);

    if(!it) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator");

        return NULL;
    }

    while(it->end_of_iterator(it) != 0) {
        linker_module_t* module = (linker_module_t*)it->get_item(it);

        if(module->sections[LINKER_SECTION_TYPE_RELOCATION_TABLE].size == 0) {
            it = it->next(it);

            continue;
        }

        uint64_t reloc_entries_size = module->sections[LINKER_SECTION_TYPE_RELOCATION_TABLE].size;
        uint64_t reloc_entries_count = reloc_entries_size / sizeof(linker_relocation_entry_t);

        linker_relocation_entry_t* reloc_entries = (linker_relocation_entry_t*)buffer_get_view_at_position(module->sections[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_data, 0, reloc_entries_size);

        efi_image_relocation_entry_t* efi_reloc_entry = NULL;
        uint64_t efi_reloc_entry_count = 0;

        for(uint64_t i = 0; i < reloc_entries_count; i++) {
            if(reloc_entries[i].relocation_type == LINKER_RELOCATION_TYPE_64_32 ||
               reloc_entries[i].relocation_type == LINKER_RELOCATION_TYPE_64_32S ||
               reloc_entries[i].relocation_type == LINKER_RELOCATION_TYPE_64_64) {

                uint64_t reloc_offset = module->sections[reloc_entries[i].section_type].virtual_start + reloc_entries[i].offset;
                uint64_t er_page = reloc_offset & ~(0x1000 - 1);
                uint64_t er_offset = reloc_offset & (0x1000 - 1);

                if(!efi_reloc_entry) {
                    efi_reloc_entry = memory_malloc(sizeof(efi_image_relocation_entry_t) + sizeof(uint16_t) * EFI_IMAGE_MAX_RELOCATION_ENTRIES);

                    if(!efi_reloc_entry) {
                        PRINTLOG(LINKER, LOG_ERROR, "cannot allocate memory");

                        goto error;
                    }

                    efi_reloc_entry->page_rva = er_page;
                    efi_reloc_entry_count = 0;
                } else {
                    if(efi_reloc_entry->page_rva != er_page) {
                        efi_reloc_entry->block_size = sizeof(efi_image_relocation_entry_t) + efi_reloc_entry_count * sizeof(uint16_t);
                        list_sortedlist_insert(relocations_list, efi_reloc_entry);

                        efi_reloc_entry = memory_malloc(sizeof(efi_image_relocation_entry_t) + sizeof(uint16_t) * EFI_IMAGE_MAX_RELOCATION_ENTRIES);

                        if(!efi_reloc_entry) {
                            PRINTLOG(LINKER, LOG_ERROR, "cannot allocate memory");

                            goto error;
                        }

                        efi_reloc_entry->page_rva = er_page;
                        efi_reloc_entry_count = 0;
                    }
                }

                efi_reloc_entry->entries[efi_reloc_entry_count].offset = er_offset;

                if(reloc_entries[i].relocation_type == LINKER_RELOCATION_TYPE_64_32 || reloc_entries[i].relocation_type == LINKER_RELOCATION_TYPE_64_32S) {
                    efi_reloc_entry->entries[efi_reloc_entry_count].type = EFI_IMAGE_REL_BASED_HIGHLOW;
                } else if(reloc_entries[i].relocation_type == LINKER_RELOCATION_TYPE_64_64) {
                    efi_reloc_entry->entries[efi_reloc_entry_count].type = EFI_IMAGE_REL_BASED_DIR64;
                }

                efi_reloc_entry_count++;
            }
        }

        if(efi_reloc_entry_count && efi_reloc_entry) {
            efi_reloc_entry->block_size = sizeof(efi_image_relocation_entry_t) + efi_reloc_entry_count * sizeof(uint16_t);
            list_sortedlist_insert(relocations_list, efi_reloc_entry);
        }

        it = it->next(it);
    }

    it->destroy(it);


    buffer_t* relocations_buffer = buffer_new();

    if(!relocations_buffer) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create buffer");

        goto error;
    }


    it = list_iterator_create(relocations_list);

    while(it->end_of_iterator(it) != 0) {
        efi_image_relocation_entry_t* efi_reloc_entry = (efi_image_relocation_entry_t*)it->get_item(it);

        PRINTLOG(LINKER, LOG_DEBUG, "relocation entry: page_rva: 0x%x, block_size: 0x%x", efi_reloc_entry->page_rva, efi_reloc_entry->block_size);

        if(!buffer_append_bytes(relocations_buffer, (uint8_t*)efi_reloc_entry, efi_reloc_entry->block_size)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot append to buffer");

            goto error;
        }

        it = it->next(it);
    }

    it->destroy(it);

    list_destroy_with_data(relocations_list);

    return relocations_buffer;

error:
    list_destroy_with_data(relocations_list);
    it->destroy(it);

    return NULL;
}

buffer_t* linker_build_efi_image_section_headers_without_relocations(linker_context_t* ctx) {
    if(!ctx) {
        PRINTLOG(LINKER, LOG_ERROR, "invalid context");

        return NULL;
    }

    list_t* sections_list = list_create_sortedlist(linker_efi_image_section_header_cmp);

    if(!sections_list) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create list");

        return NULL;
    }

    iterator_t* it = hashmap_iterator_create(ctx->modules);

    if(!it) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator");

        return NULL;
    }

    while(it->end_of_iterator(it) != 0) {
        linker_module_t* module = (linker_module_t*)it->get_item(it);

        for(uint64_t i = 0; i < LINKER_SECTION_TYPE_RELOCATION_TABLE; i++) {
            if(module->sections[i].size == 0) {
                continue;
            }

            efi_image_section_header_t* efi_section_header = memory_malloc(sizeof(efi_image_section_header_t));

            if(!efi_section_header) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot allocate memory");

                goto error;
            }

            uint64_t section_size = module->sections[i].size;

            if(section_size % 0x1000 != 0) {
                section_size += 0x1000 - (section_size % 0x1000);
            }

            efi_section_header->virtual_size = section_size;
            efi_section_header->virtual_address = module->sections[i].virtual_start;
            efi_section_header->size_of_raw_data = section_size;
            efi_section_header->pointer_to_raw_data = module->sections[i].physical_start;

            ctx->size_of_sections[i] += section_size;


            if(i == LINKER_SECTION_TYPE_TEXT) {
                strcpy(".text", efi_section_header->name);
                efi_section_header->characteristics =  EFI_IMAGE_SECTION_FLAGS_TEXT;
            } else if(i == LINKER_SECTION_TYPE_DATA || i == LINKER_SECTION_TYPE_DATARELOC) {
                strcpy(".data", efi_section_header->name);
                efi_section_header->characteristics =  EFI_IMAGE_SECTION_FLAGS_DATA;
            } else if(i == LINKER_SECTION_TYPE_RODATA || i == LINKER_SECTION_TYPE_RODATARELOC) {
                strcpy(".rdata", efi_section_header->name);
                efi_section_header->characteristics =  EFI_IMAGE_SECTION_FLAGS_RODATA;
            } else if(i == LINKER_SECTION_TYPE_BSS) {
                strcpy(".bss", efi_section_header->name);
                efi_section_header->characteristics = EFI_IMAGE_SECTION_FLAGS_BSS;
            }

            list_sortedlist_insert(sections_list, efi_section_header);

        }

        it = it->next(it);
    }

    it->destroy(it);


    buffer_t* sections_buffer = buffer_new();

    if(!sections_buffer) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create buffer");

        goto error;
    }


    it = list_iterator_create(sections_list);

    while(it->end_of_iterator(it) != 0) {
        efi_image_section_header_t* efi_reloc_entry = (efi_image_section_header_t*)it->get_item(it);

        if(!buffer_append_bytes(sections_buffer, (uint8_t*)efi_reloc_entry, sizeof(efi_image_section_header_t))) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot append to buffer");

            goto error;
        }

        it = it->next(it);
    }

    it->destroy(it);

    list_destroy_with_data(sections_list);

    return sections_buffer;

error:
    list_destroy_with_data(sections_list);
    it->destroy(it);

    return NULL;
}
#pragma GCC diagnostic pop

buffer_t*  linker_build_efi(linker_context_t* ctx) {
    if(!ctx) {
        PRINTLOG(LINKER, LOG_ERROR, "invalid context");

        return NULL;
    }

    buffer_t* program_buffer = NULL;

    uint64_t section_count = linker_get_section_count_without_relocations(ctx) + 1;

    PRINTLOG(LINKER, LOG_INFO, "section count: 0x%llx", section_count);

    buffer_t* section_headers_buffer = linker_build_efi_image_section_headers_without_relocations(ctx);

    if(!section_headers_buffer) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot build section headers");

        goto error;
    }

    uint64_t section_headers_size = buffer_get_length(section_headers_buffer);
    uint64_t section_headers_size_with_relocations = section_headers_size + sizeof(efi_image_section_header_t);

    PRINTLOG(LINKER, LOG_INFO, "section headers size: 0x%llx", section_headers_size);

    buffer_t* relocation_buffer = linker_build_efi_image_relocations(ctx);

    if(!relocation_buffer) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot build relocations");

        goto error_destroy_from_section_headers_buffer;
    }

    uint64_t relocation_size = buffer_get_length(relocation_buffer);

    uint64_t padding_after_relocations = 0;

    if(relocation_size % 0x20 != 0) {
        padding_after_relocations = 0x20 - (relocation_size % 0x20);
    }

    efi_image_section_header_t reloc_section = {
        .name = ".reloc",
        .virtual_size = relocation_size,
        .virtual_address = ctx->program_size + ctx->program_start_virtual,
        .size_of_raw_data = relocation_size,
        .pointer_to_raw_data = ctx->program_size + ctx->program_start_physical,
        .characteristics = EFI_IMAGE_SECTION_FLAGS_RELOC,
    };


    uint8_t dos_stub[EFI_IMAGE_DOSSTUB_LENGTH];
    memory_memclean(dos_stub, EFI_IMAGE_DOSSTUB_LENGTH);

    *(uint16_t*)&dos_stub[0] = EFI_IMAGE_DOSSTUB_HEADER_MAGIC;
    *(uint32_t*)&dos_stub[EFI_IMAGE_DOSSTUB_EFI_IMAGE_OFFSET_LOCATION] = EFI_IMAGE_DOSSTUB_LENGTH;


    efi_image_header_t efi_image_hdr = {
        .magic = EFI_IMAGE_HEADER_MAGIC,
        .machine = EFI_IMAGE_MACHINE_AMD64,
        .number_of_sections = section_count,
        .size_of_optional_header = sizeof(efi_image_optional_header_t),
        .characteristics = EFI_IMAGE_CHARACTERISTISCS,
    };

    uint64_t size_of_headers = sizeof(efi_image_header_t) + efi_image_hdr.size_of_optional_header + section_headers_size_with_relocations + EFI_IMAGE_DOSSTUB_LENGTH;

    if(size_of_headers % 0x20) {
        size_of_headers += 0x20 - (size_of_headers % 0x20);
    }

    PRINTLOG(LINKER, LOG_INFO, "size of headers: 0x%llx", size_of_headers);

    efi_image_optional_header_t efi_image_opt_hdr = {
        .magic = EFI_IMAGE_OPTIONAL_HEADER_MAGIC,
        .address_of_entrypoint = ctx->entrypoint_address_virtual,
        .base_of_code = 0x1000,
        .section_alignment = 0x1000,
        .file_alignment = 0x20,
        .subsystem = EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION,
        .number_of_rva_nd_sizes = 16,
        .base_relocation_table.virtual_address = reloc_section.virtual_address,
        .base_relocation_table.size = reloc_section.size_of_raw_data,
        .size_of_code = ctx->size_of_sections[LINKER_SECTION_TYPE_TEXT],
        .size_of_initialized_data = ctx->size_of_sections[LINKER_SECTION_TYPE_DATA] +
                                    ctx->size_of_sections[LINKER_SECTION_TYPE_DATARELOC] +
                                    ctx->size_of_sections[LINKER_SECTION_TYPE_RODATA] +
                                    ctx->size_of_sections[LINKER_SECTION_TYPE_RODATARELOC],
        .size_of_uninitialized_data = ctx->size_of_sections[LINKER_SECTION_TYPE_BSS],
        .size_of_headers = size_of_headers,
        .size_of_image = ctx->program_size + relocation_size + ctx->program_start_physical + padding_after_relocations,
    };


    uint8_t* program_data = memory_malloc(ctx->program_size);

    if(!program_data) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot allocate memory");

        goto error_destroy_from_relocation_buffer;
    }

    if(linker_dump_program_to_array(ctx, LINKER_PROGRAM_DUMP_TYPE_CODE, program_data)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot dump program to array");

        goto error_destroy_from_program_data;
    }

    program_buffer =  buffer_new();

    if(!program_buffer) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create buffer");

        goto error_destroy_from_program_data;
    }

    if(!buffer_append_bytes(program_buffer, dos_stub, EFI_IMAGE_DOSSTUB_LENGTH)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot append to buffer");

        goto error_destroy_from_program_buffer;
    }

    if(!buffer_append_bytes(program_buffer, (uint8_t*)&efi_image_hdr, sizeof(efi_image_header_t))) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot append to buffer");

        goto error_destroy_from_program_buffer;
    }

    if(!buffer_append_bytes(program_buffer, (uint8_t*)&efi_image_opt_hdr, sizeof(efi_image_optional_header_t))) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot append to buffer");

        goto error_destroy_from_program_buffer;
    }

    if(!buffer_append_bytes(program_buffer, buffer_get_view_at_position(section_headers_buffer, 0, section_headers_size), section_headers_size)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot append to buffer");

        goto error_destroy_from_program_buffer;
    }

    buffer_destroy(section_headers_buffer);
    section_headers_buffer = NULL;

    if(!buffer_append_bytes(program_buffer, (uint8_t*)&reloc_section, sizeof(efi_image_section_header_t))) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot append to buffer");

        goto error_destroy_from_program_buffer;
    }

    uint64_t tmp_buf_len = buffer_get_length(program_buffer);

    PRINTLOG(LINKER, LOG_INFO, "program data unaligned start: 0x%llx should start 0x%llx", tmp_buf_len, ctx->program_start_physical);

    if(tmp_buf_len > ctx->program_start_physical) {
        PRINTLOG(LINKER, LOG_ERROR, "program header size is too big");

        goto error_destroy_from_program_buffer;
    }

    if(tmp_buf_len < ctx->program_start_physical) {
        uint64_t padding_size = ctx->program_start_physical - tmp_buf_len;

        int8_t zero = 0;

        for(uint64_t i = 0; i < padding_size; i++) {
            if(!buffer_append_bytes(program_buffer, (uint8_t*)&zero, 1)) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot append to buffer");

                goto error_destroy_from_program_buffer;
            }
        }
    }

    tmp_buf_len = buffer_get_length(program_buffer);

    PRINTLOG(LINKER, LOG_INFO, "program data starts at: 0x%llx", tmp_buf_len);

    if(!buffer_append_bytes(program_buffer, program_data, ctx->program_size)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot append to buffer");

        goto error_destroy_from_program_buffer;
    }

    memory_free(program_data);
    program_data = NULL;

    if(!buffer_append_bytes(program_buffer, buffer_get_view_at_position(relocation_buffer, 0, relocation_size), relocation_size)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot append to buffer");

        goto error_destroy_from_program_buffer;
    }

    buffer_destroy(relocation_buffer);
    relocation_buffer = NULL;

    tmp_buf_len = buffer_get_length(program_buffer);

    if(tmp_buf_len % 0x20) {
        uint64_t padding_size = 0x20 - (tmp_buf_len % 0x20);

        int8_t zero = 0;

        for(uint64_t i = 0; i < padding_size; i++) {
            if(!buffer_append_bytes(program_buffer, (uint8_t*)&zero, 1)) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot append to buffer");

                goto error_destroy_from_program_buffer;
            }
        }
    }

    return program_buffer;


error_destroy_from_program_buffer:
    if(program_buffer) {
        buffer_destroy(program_buffer);
    }

error_destroy_from_program_data:
    if(program_data) {
        memory_free(program_data);
    }

error_destroy_from_relocation_buffer:
    if(relocation_buffer) {
        buffer_destroy(relocation_buffer);
    }

error_destroy_from_section_headers_buffer:
    if(section_headers_buffer) {
        buffer_destroy(section_headers_buffer);
    }
error:
    return NULL;
}

const uint8_t linker_program_header_trampoline_code[] = {
    0x48, 0x8b, 0x57, 0x48, // mov 0x48(%rdi),%rdx
    0x48, 0x8b, 0x42, 0x40, // mov 0x40(%rdx),%rax
    0x48, 0x03, 0x42, 0x48, // add 0x48(%rdx),%rax
    0x48, 0x83, 0xe8, 0x10, // sub $0x10,%rax
    0x48, 0x89, 0xc4, // mov %rax,%rsp
    0x48, 0x31, 0xed, // xor %rbp,%rbp
    0x48, 0x8b, 0x82, 0xf0, 0x00, 0x00, 0x00, // mov 0xf0(%rdx),%rax
    0x48, 0x8b, 0x00, // mov (%rax),%rax
    0x0f, 0x22, 0xd8, // mov %rax,%cr3
    0x48, 0x8b, 0x42, 0x38, // mov 0x38(%rdx),%rax
    0xff, 0xd0, // call *%rax
};

int8_t linker_dump_program_to_array(linker_context_t* ctx, linker_program_dump_type_t dump_type, uint8_t* array) {
    if(!ctx || !array) {
        PRINTLOG(LINKER, LOG_ERROR, "invalid context or array");

        return -1;
    }

    if(dump_type == LINKER_PROGRAM_DUMP_TYPE_NONE) {
        return 0;
    }

    if(dump_type & LINKER_PROGRAM_DUMP_TYPE_BUILD_PAGE_TABLE) {
        if(!(dump_type & LINKER_PROGRAM_DUMP_TYPE_HEADER)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot build page table without header");

            return -1;
        }
    }

#ifndef ___TESTMODE
    memory_page_table_context_t* page_table_ctx = NULL;
#endif

    uint64_t program_target_offset = 0;

    if(dump_type & LINKER_PROGRAM_DUMP_TYPE_HEADER) {
        program_header_t* program_header = (program_header_t*)array;

        program_header->jmp_code = 0xe9;
        program_header->trampoline_address_pc_relative = offsetof_field(program_header_t, trampoline_code) - 5;
        memory_memcopy(linker_program_header_trampoline_code, program_header->trampoline_code, sizeof(linker_program_header_trampoline_code));

        strcpy(TOS_EXECUTABLE_OR_LIBRARY_MAGIC, (char_t*)program_header->magic);

        program_header->header_physical_address = ctx->program_start_physical - 0x1000;
        program_header->header_virtual_address = ctx->program_start_virtual - 0x1000;
        program_header->program_offset = 0x1000;
        program_header->total_size += 0x1000 + ctx->program_size;
        program_header->program_size = ctx->program_size;
        program_header->program_entry = ctx->entrypoint_address_virtual;

        program_target_offset += 0x1000;

        if(dump_type & LINKER_PROGRAM_DUMP_TYPE_BUILD_PAGE_TABLE) {
#ifndef ___TESTMODE
            PRINTLOG(LINKER, LOG_TRACE, "building page table");

            page_table_ctx = memory_paging_build_empty_table(ctx->page_table_helper_frames);

            if(!page_table_ctx) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot build page table");

                return -1;
            }

            PRINTLOG(LINKER, LOG_TRACE, "page table built");

            program_header->page_table_context_address = (uint64_t)page_table_ctx;

            frame_t frame = {
                .frame_address = program_header->header_physical_address,
                .frame_count = 1,
            };

            if(memory_paging_add_va_for_frame_ext(page_table_ctx,
                                                  program_header->header_virtual_address,
                                                  &frame,
                                                  MEMORY_PAGING_PAGE_TYPE_READONLY) != 0) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot add header page to page table");

                return -1;
            }

            if(memory_paging_add_va_for_frame_ext(page_table_ctx,
                                                  program_header->header_physical_address,
                                                  &frame,
                                                  MEMORY_PAGING_PAGE_TYPE_READONLY) != 0) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot add header page to page table");

                return -1;
            }

            PRINTLOG(LINKER, LOG_TRACE, "program header added to page table");
#else
            PRINTLOG(LINKER, LOG_ERROR, "page table not supported on host");

            return -1;
#endif
        }

    }


    if(dump_type & LINKER_PROGRAM_DUMP_TYPE_CODE) {

        iterator_t* it = hashmap_iterator_create(ctx->modules);

        if(!it) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator");

            return -1;
        }

        while(it->end_of_iterator(it) != 0) {
            linker_module_t* module = (linker_module_t*)it->get_item(it);

            for(uint64_t i = 0; i < LINKER_SECTION_TYPE_RELOCATION_TABLE; i++) {
                if(module->sections[i].size == 0) {
                    continue;
                }

                uint64_t section_data_size = buffer_get_length(module->sections[i].section_data);

                uint8_t* section_data = buffer_get_view_at_position(module->sections[i].section_data, 0, section_data_size);

                PRINTLOG(LINKER, LOG_DEBUG, "copying module id 0x%llx section type %lli to 0x%llx with size 0x%llx", module->id, i, module->sections[i].physical_start - ctx->program_start_physical, section_data_size);
                memory_memcopy(section_data, array + program_target_offset + module->sections[i].physical_start - ctx->program_start_physical, section_data_size);

#ifndef ___TESTMODE
                if(dump_type & LINKER_PROGRAM_DUMP_TYPE_BUILD_PAGE_TABLE) {
                    uint64_t section_size = module->sections[i].size;

                    if(section_size % FRAME_SIZE != 0) {
                        section_size += FRAME_SIZE - (section_size % FRAME_SIZE);
                    }


                    frame_t frame = {
                        .frame_address = module->sections[i].physical_start,
                        .frame_count = section_size / FRAME_SIZE,
                    };

                    memory_paging_page_type_t page_type = MEMORY_PAGING_PAGE_TYPE_UNKNOWN;

                    if(i == LINKER_SECTION_TYPE_TEXT) {
                        page_type |= MEMORY_PAGING_PAGE_TYPE_READONLY;
                    } else {
                        page_type |= MEMORY_PAGING_PAGE_TYPE_NOEXEC;
                    }

                    if(i == LINKER_SECTION_TYPE_RODATARELOC || i == LINKER_SECTION_TYPE_RODATA) {
                        page_type |= MEMORY_PAGING_PAGE_TYPE_READONLY;
                    }

                    if(memory_paging_add_va_for_frame_ext(page_table_ctx,
                                                          module->sections[i].virtual_start,
                                                          &frame,
                                                          page_type) != 0) {
                        PRINTLOG(LINKER, LOG_ERROR, "cannot add section to page table");

                        return -1;
                    }

                    PRINTLOG(LINKER, LOG_TRACE, "section added to page table");

                }
#endif
            }

            it = it->next(it);
        }

        it->destroy(it);

        program_target_offset += ctx->program_size;
    }


    if(dump_type & LINKER_PROGRAM_DUMP_TYPE_GOT) {

        uint64_t got_size = buffer_get_length(ctx->got_table_buffer);

        uint8_t* got = buffer_get_view_at_position(ctx->got_table_buffer, 0, got_size);

        PRINTLOG(LINKER, LOG_DEBUG, "copying got to 0x%llx with size 0x%llx", program_target_offset, got_size);
        memory_memcopy(got, array + program_target_offset, got_size);

        if(dump_type & LINKER_PROGRAM_DUMP_TYPE_HEADER) {
            program_header_t* program_header = (program_header_t*)array;

            program_header->got_offset = program_target_offset;
            program_header->got_size = ctx->global_offset_table_size;
            program_header->got_virtual_address = program_header->header_virtual_address + program_target_offset;
            program_header->got_physical_address = program_header->header_physical_address + program_target_offset;

            program_header->total_size += ctx->global_offset_table_size;

#ifndef ___TESTMODE
            if(dump_type & LINKER_PROGRAM_DUMP_TYPE_BUILD_PAGE_TABLE) {
                frame_t frame = {
                    .frame_address = program_header->got_physical_address,
                    .frame_count = program_header->got_size / FRAME_SIZE,
                };

                if(memory_paging_add_va_for_frame_ext(page_table_ctx,
                                                      program_header->got_virtual_address,
                                                      &frame,
                                                      MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
                    PRINTLOG(LINKER, LOG_ERROR, "cannot add got to page table");

                    return -1;
                }

                PRINTLOG(LINKER, LOG_INFO, "got added to page table at 0x%llx", program_header->got_virtual_address);

            }
#endif
        }

        program_target_offset += ctx->global_offset_table_size;
    }

    if(dump_type & LINKER_PROGRAM_DUMP_TYPE_RELOCATIONS) {
        buffer_t* relocs_buf = linker_build_relocation_table_buffer(ctx);

        uint64_t relocs_size = buffer_get_length(relocs_buf);

        uint8_t* relocs = buffer_get_view_at_position(relocs_buf, 0, relocs_size);

        PRINTLOG(LINKER, LOG_DEBUG, "copying relocations to 0x%llx with size 0x%llx", program_target_offset, relocs_size);
        memory_memcopy(relocs, array + program_target_offset, relocs_size);

        buffer_destroy(relocs_buf);

        if(dump_type & LINKER_PROGRAM_DUMP_TYPE_HEADER) {
            program_header_t* program_header = (program_header_t*)array;

            program_header->relocation_table_offset = program_target_offset;
            program_header->relocation_table_size = ctx->relocation_table_size;
            program_header->relocation_table_virtual_address = program_header->header_virtual_address + program_target_offset;
            program_header->relocation_table_physical_address = program_header->header_physical_address + program_target_offset;

            program_header->total_size += ctx->relocation_table_size;

#ifndef ___TESTMODE
            if(dump_type & LINKER_PROGRAM_DUMP_TYPE_BUILD_PAGE_TABLE) {
                frame_t frame = {
                    .frame_address = program_header->relocation_table_physical_address,
                    .frame_count = program_header->relocation_table_size / FRAME_SIZE,
                };

                if(memory_paging_add_va_for_frame_ext(page_table_ctx,
                                                      program_header->relocation_table_virtual_address,
                                                      &frame,
                                                      MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
                    PRINTLOG(LINKER, LOG_ERROR, "cannot add relocation table to page table");

                    return -1;
                }

                PRINTLOG(LINKER, LOG_INFO, "relocation table added to page table at 0x%llx", program_header->relocation_table_virtual_address);

            }
#endif
        }

        program_target_offset += ctx->relocation_table_size;
    }

    if(dump_type & LINKER_PROGRAM_DUMP_TYPE_METADATA) {
        buffer_t* metadata_buf = linker_build_metadata_buffer(ctx);

        uint64_t metadata_size = buffer_get_length(metadata_buf);

        uint8_t* metadata = buffer_get_view_at_position(metadata_buf, 0, metadata_size);

        PRINTLOG(LINKER, LOG_DEBUG, "copying metadata to 0x%llx with size 0x%llx", program_target_offset, metadata_size);
        memory_memcopy(metadata, array + program_target_offset, metadata_size);

        buffer_destroy(metadata_buf);

        if(dump_type & LINKER_PROGRAM_DUMP_TYPE_HEADER) {
            program_header_t* program_header = (program_header_t*)array;

            program_header->metadata_offset = program_target_offset;
            program_header->metadata_size = ctx->metadata_size;
            program_header->metadata_virtual_address = program_header->header_virtual_address + program_target_offset;
            program_header->metadata_physical_address = program_header->header_physical_address + program_target_offset;

            program_header->total_size += ctx->metadata_size;

#ifndef ___TESTMODE
            if(dump_type & LINKER_PROGRAM_DUMP_TYPE_BUILD_PAGE_TABLE) {
                frame_t frame = {
                    .frame_address = program_header->metadata_physical_address,
                    .frame_count = program_header->metadata_size / FRAME_SIZE,
                };

                if(memory_paging_add_va_for_frame_ext(page_table_ctx,
                                                      program_header->metadata_virtual_address,
                                                      &frame,
                                                      MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
                    PRINTLOG(LINKER, LOG_ERROR, "cannot add metadata to page table");

                    return -1;
                }

                PRINTLOG(LINKER, LOG_INFO, "metadata added to page table at 0x%llx", program_header->metadata_virtual_address);

            }
#endif
        }

        program_target_offset += ctx->metadata_size;
    }

    if(ctx->symbol_table_buffer && (dump_type & LINKER_PROGRAM_DUMP_TYPE_SYMBOLS)) {
        buffer_t* symbol_table_buf = ctx->symbol_table_buffer;
        uint64_t symbol_table_size = buffer_get_length(symbol_table_buf);

        uint8_t* symbol_table = buffer_get_view_at_position(symbol_table_buf, 0, symbol_table_size);

        PRINTLOG(LINKER, LOG_DEBUG, "copying symbol table to 0x%llx with size 0x%llx", program_target_offset, symbol_table_size);
        memory_memcopy(symbol_table, array + program_target_offset, symbol_table_size);

        buffer_destroy(symbol_table_buf);

        if(dump_type & LINKER_PROGRAM_DUMP_TYPE_HEADER) {
            program_header_t* program_header = (program_header_t*)array;

            program_header->symbol_table_offset = program_target_offset;
            program_header->symbol_table_size = ctx->symbol_table_size;
            program_header->symbol_table_virtual_address = program_header->header_virtual_address + program_target_offset;
            program_header->symbol_table_physical_address = program_header->header_physical_address + program_target_offset;

            program_header->total_size += ctx->symbol_table_size;

#ifndef ___TESTMODE
            if(dump_type & LINKER_PROGRAM_DUMP_TYPE_BUILD_PAGE_TABLE) {
                frame_t frame = {
                    .frame_address = program_header->symbol_table_physical_address,
                    .frame_count = program_header->symbol_table_size / FRAME_SIZE,
                };

                if(memory_paging_add_va_for_frame_ext(page_table_ctx,
                                                      program_header->symbol_table_virtual_address,
                                                      &frame,
                                                      MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
                    PRINTLOG(LINKER, LOG_ERROR, "cannot add symbol table to page table");

                    return -1;
                }

                PRINTLOG(LINKER, LOG_INFO, "symbol table added to page table at 0x%llx", program_header->symbol_table_virtual_address);

            }
#endif
        }

        program_target_offset += ctx->symbol_table_size;
    }

#ifndef ___TESTMODE
    if(dump_type & LINKER_PROGRAM_DUMP_TYPE_BUILD_PAGE_TABLE) {
        program_header_t* program_header = (program_header_t*)array;

        if(program_header->program_heap_size > 0) {
            frame_t frame = {
                .frame_address = program_header->program_heap_physical_address,
                .frame_count = program_header->program_heap_size / FRAME_SIZE,
            };

            if(memory_paging_add_va_for_frame_ext(page_table_ctx,
                                                  program_header->program_heap_virtual_address,
                                                  &frame,
                                                  MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot add heap to page table");

                return -1;
            }

            PRINTLOG(LINKER, LOG_TRACE, "heap added to page table");

        }

        if(program_header->program_stack_size > 0) {
            frame_t frame = {
                .frame_address = program_header->program_stack_physical_address,
                .frame_count = program_header->program_stack_size / FRAME_SIZE,
            };

            if(memory_paging_add_va_for_frame_ext(page_table_ctx,
                                                  program_header->program_stack_virtual_address,
                                                  &frame,
                                                  MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot add stack to page table");

                return -1;
            }

            PRINTLOG(LINKER, LOG_TRACE, "stack added to page table");

        }
    }
#endif

    return 0;
}

buffer_t* linker_build_relocation_table_buffer(linker_context_t* ctx) {
    if(!ctx) {
        PRINTLOG(LINKER, LOG_ERROR, "invalid context");

        return NULL;
    }

    buffer_t* relocation_buffer = buffer_new_with_capacity(NULL, ctx->relocation_table_size);

    if(!relocation_buffer) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create buffer");

        return NULL;
    }


    iterator_t* it = hashmap_iterator_create(ctx->modules);

    if(!it) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator");

        goto error_destroy_buffer;
    }

    while(it->end_of_iterator(it) != 0) {
        linker_module_t* module = (linker_module_t*)it->get_item(it);


        if(module->sections[LINKER_SECTION_TYPE_RELOCATION_TABLE].size == 0) {
            it = it->next(it);

            continue;
        }

        if(!buffer_append_bytes(relocation_buffer, (uint8_t*)&module->id, sizeof(uint64_t))) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot append module id to buffer");
            it->destroy(it);

            goto error_destroy_buffer;
        }

        if(!buffer_append_bytes(relocation_buffer, (uint8_t*)&module->sections[LINKER_SECTION_TYPE_RELOCATION_TABLE].size, sizeof(uint64_t))) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot append relocation table size to buffer");
            it->destroy(it);

            goto error_destroy_buffer;
        }

        if(!buffer_append_buffer(relocation_buffer, module->sections[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_data)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot append relocation table to buffer");
            it->destroy(it);

            goto error_destroy_buffer;
        }

        it = it->next(it);
    }


    it->destroy(it);

    return relocation_buffer;

error_destroy_buffer:
    buffer_destroy(relocation_buffer);

    return NULL;
}

static int8_t linker_build_metadata_buffer_null_terminator(buffer_t* metadata_buffer, uint64_t count) {
    if(!metadata_buffer) {
        PRINTLOG(LINKER, LOG_ERROR, "invalid buffer");

        return -1;
    }

    uint64_t null_terminator = 0;

    for(uint64_t i = 0; i < count; i++) {
        if(!buffer_append_bytes(metadata_buffer, (uint8_t*)&null_terminator, sizeof(uint64_t))) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot append null terminator to buffer");

            return -1;
        }
    }

    return 0;
}

buffer_t* linker_build_metadata_buffer(linker_context_t* ctx) {
    if(!ctx) {
        PRINTLOG(LINKER, LOG_ERROR, "invalid context");

        return NULL;
    }

    buffer_t* metadata_buffer = buffer_new_with_capacity(NULL, ctx->metadata_size);

    if(!metadata_buffer) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create buffer");

        return NULL;
    }


    iterator_t* it = hashmap_iterator_create(ctx->modules);

    if(!it) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator");

        goto error_destroy_buffer;
    }

    while(it->end_of_iterator(it) != 0) {
        linker_module_t* module = (linker_module_t*)it->get_item(it);

        if(!buffer_append_bytes(metadata_buffer, (uint8_t*)&module->id, sizeof(uint64_t))) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot append module id to buffer");
            it->destroy(it);

            goto error_destroy_buffer;
        }

        if(!buffer_append_bytes(metadata_buffer, (uint8_t*)&module->module_name_offset, sizeof(uint64_t))) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot append module size to buffer");
            it->destroy(it);

            goto error_destroy_buffer;
        }

        if(!buffer_append_bytes(metadata_buffer, (uint8_t*)&module->physical_start, sizeof(uint64_t))) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot append physical start to buffer");
            it->destroy(it);

            goto error_destroy_buffer;
        }

        if(!buffer_append_bytes(metadata_buffer, (uint8_t*)&module->virtual_start, sizeof(uint64_t))) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot append virtual start to buffer");
            it->destroy(it);

            goto error_destroy_buffer;
        }

        for(uint64_t i = 0; i < LINKER_SECTION_TYPE_RELOCATION_TABLE; i++) {
            if(module->sections[i].size == 0) {
                continue;
            }

            if(!buffer_append_bytes(metadata_buffer, (uint8_t*)&i, sizeof(uint64_t))) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot append section type to buffer");
                it->destroy(it);

                goto error_destroy_buffer;
            }

            if(!buffer_append_bytes(metadata_buffer, (uint8_t*)&module->sections[i].physical_start, sizeof(uint64_t))) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot append section physical start to buffer");
                it->destroy(it);

                goto error_destroy_buffer;
            }

            if(!buffer_append_bytes(metadata_buffer, (uint8_t*)&module->sections[i].virtual_start, sizeof(uint64_t))) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot append section virtual start to buffer");
                it->destroy(it);

                goto error_destroy_buffer;
            }

            if(!buffer_append_bytes(metadata_buffer, (uint8_t*)&module->sections[i].size, sizeof(uint64_t))) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot append section size to buffer");
                it->destroy(it);

                goto error_destroy_buffer;
            }
        }

        if(linker_build_metadata_buffer_null_terminator(metadata_buffer, 4) != 0) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot append null terminator to buffer");
            it->destroy(it);

            goto error_destroy_buffer;
        }


        it = it->next(it);
    }


    it->destroy(it);

    if(linker_build_metadata_buffer_null_terminator(metadata_buffer, 4) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot append null terminator to buffer");

        goto error_destroy_buffer;
    }

    return metadata_buffer;

error_destroy_buffer:
    buffer_destroy(metadata_buffer);

    return NULL;
}
