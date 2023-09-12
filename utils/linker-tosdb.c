/**
 * @file linker-tosdb.c
 * @brief Linker from TOSDB tables
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE (256 << 20)

#include "setup.h"
#include <tosdb/tosdb.h>
#include <strings.h>
#include <hashmap.h>
#include <buffer.h>
#include <linker.h>
#include <linkedlist.h>

//utils programs need dep headers for linking
#include <utils.h>
#include <set.h>
#include <cache.h>
#include <xxhash.h>
#include <rbtree.h>
#include <bloomfilter.h>
#include <binarysearch.h>
#include <bplustree.h>
#include <zpack.h>
#include <math.h>
// end of dep headers


#define LINKERDB_CAP (32 << 20)

#define LINKER_GOT_SYMBOL_ID  0x1
#define LINKER_GOT_SECTION_ID 0x1

typedef struct linkerdb_t {
    uint64_t         capacity;
    FILE*            db_file;
    int32_t          fd;
    uint8_t*         mmap_res;
    buffer_t         backend_buffer;
    tosdb_backend_t* backend;
    tosdb_t*         tdb;
} linkerdb_t;

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
    uint64_t    program_start;
    uint64_t    entrypoint_symbol_id;
    hashmap_t*  modules;
    buffer_t    got_table_buffer;
    hashmap_t*  got_symbol_index_map;
    linkerdb_t* ldb;
} linker_context_t;


int32_t     main(int32_t argc, char_t** args);
linkerdb_t* linkerdb_open(const char_t* file);
boolean_t   linkerdb_close(linkerdb_t* ldb);
int8_t      linker_destroy_context(linker_context_t* ctx);
int8_t      linker_build_module(linker_context_t* ctx, uint64_t module_id, boolean_t recursive);
int8_t      linker_build_symbols(linker_context_t* ctx, uint64_t module_id, uint64_t section_id, uint8_t section_type, uint64_t section_offset);
int8_t      linker_build_relocations(linker_context_t* ctx, uint64_t section_id, uint64_t section_offset, linker_section_t* section, boolean_t recursive);


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

    tosdb_database_t* db_system = tosdb_database_create_or_open(ctx->ldb->tdb, "system");
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

    linkedlist_t* symbols = s_sym_rec->search_record(s_sym_rec);

    s_sym_rec->destroy(s_sym_rec);

    if(!symbols) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot search symbols for section id 0x%llx", section_id);

        return -1;
    }

    PRINTLOG(LINKER, LOG_DEBUG, "found %llu symbols for section id 0x%llx", linkedlist_size(symbols), section_id);

    iterator_t* it = linkedlist_iterator_create(symbols);

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

        PRINTLOG(LINKER, LOG_DEBUG, "found symbol %s with id 0x%llx, at section 0x%llx", symbol_name, symbol_id, section_id);

        memory_free(symbol_name);

        memory_memclean(&got_entry, sizeof(linker_global_offset_table_entry_t));

        got_entry.module_id = module_id;
        got_entry.symbol_id = symbol_id;
        got_entry.symbol_type = symbol_type;
        got_entry.symbol_scope = symbol_scope;
        got_entry.symbol_value = symbol_value + section_offset;
        got_entry.symbol_size = symbol_size;
        got_entry.section_type = section_type;

        uint64_t got_entry_index = buffer_get_length(ctx->got_table_buffer) / sizeof(linker_global_offset_table_entry_t);

        buffer_append_bytes(ctx->got_table_buffer, (uint8_t*)&got_entry, sizeof(linker_global_offset_table_entry_t));

        hashmap_put(ctx->got_symbol_index_map, (void*)symbol_id, (void*)got_entry_index);

        PRINTLOG(LINKER, LOG_DEBUG, "added symbol %s with id 0x%llx, at section 0x%llx, to got table at index 0x%llx", symbol_name, symbol_id, section_id, got_entry_index);

        sym_rec->destroy(sym_rec);

        it = it->next(it);
    }

    it->destroy(it);

    linkedlist_destroy(symbols);

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

    linkedlist_destroy(symbols);

    return -1;
}

int8_t linker_build_relocations(linker_context_t* ctx, uint64_t section_id, uint64_t section_offset, linker_section_t* section, boolean_t recursive) {
    int8_t res = 0;

    tosdb_database_t* db_system = tosdb_database_create_or_open(ctx->ldb->tdb, "system");
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

    linkedlist_t* relocations = s_rel_reloc->search_record(s_rel_reloc);

    s_rel_reloc->destroy(s_rel_reloc);

    if(!relocations) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot search relocations for section id 0x%llx", section_id);

        return -1;
    }

    PRINTLOG(LINKER, LOG_DEBUG, "relocations count of section 0x%llx: 0x%llx", section_id, linkedlist_size(relocations));

    iterator_t* it = linkedlist_iterator_create(relocations);

    if(!it) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator for relocations");

        return -1;
    }

    linker_direct_relocation_t relocation = {0};
    int64_t reloc_id = 0;
    int64_t symbol_section_id = 0;
    int64_t symbol_id = 0;
    int8_t reloc_type = 0;
    int64_t reloc_offset = 0;
    int64_t reloc_addend = 0;
    uint8_t section_type = 0;
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

        memory_free(symbol_name);

        if(symbol_section_id == 0) {
            if(!is_got_symbol) {
                PRINTLOG(LINKER, LOG_ERROR, "symbol section id is missing for symbol %s, relocation at section 0x%llx relocation id 0x%llx", symbol_name, section_id, reloc_id);
                reloc_rec->destroy(reloc_rec);

                goto clean_relocs_iter;
            } else {
                symbol_section_id = LINKER_GOT_SECTION_ID;
            }
        }

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

            if(!s_sec_rec->get_uint8(s_sec_rec, "type", &section_type)) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot get section type");
                reloc_rec->destroy(reloc_rec);
                s_sec_rec->destroy(s_sec_rec);

                goto clean_relocs_iter;
            }

            if(!s_sec_rec->get_int64(s_sec_rec, "module_id", &module_id)) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot get section module id");
                reloc_rec->destroy(reloc_rec);
                s_sec_rec->destroy(s_sec_rec);

                goto clean_relocs_iter;
            }

            s_sec_rec->destroy(s_sec_rec);

            PRINTLOG(LINKER, LOG_DEBUG, "relocation 0x%llx source symbol section id 0x%llx type 0x%x", reloc_id, symbol_section_id, section_type);
        }

        memory_memclean(&relocation, sizeof(linker_direct_relocation_t));

        relocation.symbol_id = symbol_id;
        relocation.section_type = section_type;
        relocation.relocation_type = reloc_type;
        relocation.offset = reloc_offset + section_offset;
        relocation.addend = reloc_addend;

        buffer_append_bytes(section->section_data, (uint8_t*)&relocation, sizeof(linker_direct_relocation_t));
        section->size += sizeof(linker_direct_relocation_t);

        if(recursive && !is_got_symbol) {
            PRINTLOG(LINKER, LOG_TRACE, "check if symbol 0x%llx loaded?", symbol_id);
            uint64_t got_index = (uint64_t)hashmap_get(ctx->got_symbol_index_map, (void*)symbol_id);

            if(!got_index) {
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
                PRINTLOG(LINKER, LOG_TRACE, "symbol 0x%llx already loaded, got index 0x%llx", symbol_id, got_index);
            }
        }

        reloc_rec->destroy(reloc_rec);

        it = it->next(it);
    }

    it->destroy(it);

    linkedlist_destroy(relocations);

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

    linkedlist_destroy(relocations);

    return -1;
}


int8_t linker_build_module(linker_context_t* ctx, uint64_t module_id, boolean_t recursive) {
    int8_t res = 0;

    linker_module_t* module = (linker_module_t*)hashmap_get(ctx->modules, (void*)module_id);

    if(!module) {
        module = memory_malloc(sizeof(linker_module_t));
        hashmap_put(ctx->modules, (void*)module_id, module);
    } else {
        if(recursive) {
            return -2;
        }
    }

    tosdb_database_t* db_system = tosdb_database_create_or_open(ctx->ldb->tdb, "system");
    tosdb_table_t* tbl_sections = tosdb_table_create_or_open(db_system, "sections", 1 << 10, 512 << 10, 8);


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

    linkedlist_t* sections = s_sec_rec->search_record(s_sec_rec);

    s_sec_rec->destroy(s_sec_rec);

    if(!sections) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot search sections for module id 0x%llx", module_id);

        return -1;
    }

    PRINTLOG(LINKER, LOG_DEBUG, "module 0x%llx sections count: %llu", module_id, linkedlist_size(sections));


    uint64_t section_id = 0;
    uint8_t section_type = 0;
    uint8_t* section_data = NULL;
    uint64_t section_size = 0;
    uint64_t tmp_section_size = 0;
    char_t* section_name = NULL;

    uint64_t section_offset = 0;

    iterator_t* it = linkedlist_iterator_create(sections);

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


        if(section_type != LINKER_SECTION_TYPE_BSS) {
            if(!sec_rec->get_bytearray(sec_rec, "value", &tmp_section_size, &section_data)) {
                PRINTLOG(LINKER, LOG_ERROR, "cannot get section data");
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

            section_offset = buffer_get_length(module->sections[section_type].section_data);
            buffer_append_bytes(module->sections[section_type].section_data, section_data, section_size);

            memory_free(section_data);
        } else {
            section_offset = module->sections[section_type].size;
        }

        if(!sec_rec->get_string(sec_rec, "name", &section_name)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot get section name");
            sec_rec->destroy(sec_rec);

            goto clean_secs_iter;
        }

        PRINTLOG(LINKER, LOG_DEBUG, "module id 0x%llx section id: 0x%llx, type: %u, name: %s", module_id, section_id, section_type, section_name);

        memory_free(section_name);

        if(linker_build_symbols(ctx, module_id, section_id, section_type, section_offset) != 0) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot build symbols for section id 0x%llx", section_id);
            sec_rec->destroy(sec_rec);

            goto clean_secs_iter;
        }

        if(linker_build_relocations(ctx,  section_id, section_offset, &module->sections[LINKER_SECTION_TYPE_RELOCATION_TABLE], recursive) != 0) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot build relocations for section id 0x%llx", section_id);
            sec_rec->destroy(sec_rec);

            goto clean_secs_iter;
        }

        module->sections[section_type].size += section_size;

        sec_rec->destroy(sec_rec);

        it = it->next(it);
    }

    it->destroy(it);

    linkedlist_destroy(sections);

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

    linkedlist_destroy(sections);

    return -1;
}


linkerdb_t* linkerdb_open(const char_t* file) {
    FILE* fp = fopen(file, "r+");

    if(!fp) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot open db file");

        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    uint64_t capacity = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    int32_t fd = fileno(fp);

    uint8_t* mmap_res = mmap(NULL, capacity, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_SYNC, fd, 0);

    if(mmap_res == (void*)-1) {
        fclose(fp);
        PRINTLOG(LINKER, LOG_ERROR, "cannot mmap db file");

        return NULL;
    }

    buffer_t buf = buffer_encapsulate(mmap_res, capacity);

    if(!buf) {
        munmap(mmap_res, capacity);
        fclose(fp);
        PRINTLOG(LINKER, LOG_ERROR, "cannot create buffer");

        return NULL;
    }

    buffer_set_readonly(buf, false);

    tosdb_backend_t* bend = tosdb_backend_memory_from_buffer(buf);

    if(!bend) {
        buffer_set_readonly(buf, true);
        buffer_destroy(buf);
        munmap(mmap_res, capacity);
        fclose(fp);
        PRINTLOG(LINKER, LOG_ERROR, "cannot create backend");

        return NULL;
    }

    tosdb_t* tdb = tosdb_new(bend);

    if(!tdb) {
        buffer_set_readonly(buf, true);
        tosdb_backend_close(bend);
        munmap(mmap_res, capacity);
        fclose(fp);
        PRINTLOG(LINKER, LOG_ERROR, "cannot create backend");

        return NULL;
    }

    tosdb_cache_config_t cc = {0};
    cc.bloomfilter_size = 2 << 20;
    cc.index_data_size = 4 << 20;
    cc.secondary_index_data_size = 4 << 20;
    cc.valuelog_size = 16 << 20;

    if(!tosdb_cache_config_set(tdb, &cc)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot set cache");
        buffer_set_readonly(buf, true);
        tosdb_backend_close(bend);
        munmap(mmap_res, capacity);
        fclose(fp);
        PRINTLOG(LINKER, LOG_ERROR, "cannot create backend");

        return NULL;
    }


    linkerdb_t* ldb = memory_malloc(sizeof(linkerdb_t));

    if(!ldb) {
        tosdb_close(tdb);
        tosdb_free(tdb);
        buffer_set_readonly(buf, true);
        tosdb_backend_close(bend);
        munmap(mmap_res, capacity);
        fclose(fp);
        PRINTLOG(LINKER, LOG_ERROR, "cannot create backend");

        return NULL;
    }

    ldb->backend = bend;
    ldb->backend_buffer = buf;
    ldb->capacity = capacity;
    ldb->db_file = fp;
    ldb->fd = fd;
    ldb->mmap_res = mmap_res;
    ldb->tdb = tdb;

    return ldb;
}

boolean_t linkerdb_close(linkerdb_t* ldb) {
    if(!ldb) {
        return false;
    }

    if(!tosdb_close(ldb->tdb)) {
        return false;
    }

    if(!tosdb_free(ldb->tdb)) {
        return false;
    }

    buffer_set_readonly(ldb->backend_buffer, true);

    if(!tosdb_backend_close(ldb->backend)) {
        return false;
    }

    int32_t mss = msync(ldb->mmap_res, ldb->capacity, MS_SYNC);
    if(mss) {
        PRINTLOG(LINKER, LOG_ERROR, "msync res %i", mss);
        PRINTLOG(LINKER, LOG_ERROR, "cannot sync contents");
    }

    munmap(ldb->mmap_res, ldb->capacity);

    fclose(ldb->db_file);

    memory_free(ldb);

    return true;
}


int32_t main(int32_t argc, char_t** argv) {

    if(argc <= 1) {
        PRINTLOG(LINKER, LOG_ERROR, "not enough params");
        return -1;
    }

    logging_module_levels[TOSDB] = LOG_INFO;
    logging_module_levels[LINKER] = LOG_DEBUG;

    argc--;
    argv++;

    char_t* db_file = NULL;
    char_t* entrypoint_symbol = NULL;
    uint64_t program_start = 0;

    while(argc > 0) {
        if(strstarts(*argv, "-") != 0) {
            PRINTLOG(LINKER, LOG_ERROR, "argument error");
            PRINTLOG(LINKER, LOG_ERROR, "LINKERDB FAILED");

            return -1;
        }

        if(strcmp(*argv, "-s") == 0 || strcmp(*argv, "--start") == 0) {
            argc--;
            argv++;

            if(argc) {
                program_start = atoi(*argv);

                argc--;
                argv++;
                continue;
            } else {
                PRINTLOG(LINKER, LOG_ERROR, "argument error");
                PRINTLOG(LINKER, LOG_ERROR, "LINKERDB FAILED");

                return -1;
            }
        }

        if(strcmp(*argv, "-e") == 0 || strcmp(*argv, "--entrypoint") == 0) {
            argc--;
            argv++;

            if(argc) {
                entrypoint_symbol = *argv;

                argc--;
                argv++;
                continue;
            } else {
                PRINTLOG(LINKER, LOG_ERROR, "argument error");
                PRINTLOG(LINKER, LOG_ERROR, "LINKERDB FAILED");

                return -1;
            }
        }

        if(strcmp(*argv, "-db") == 0 || strcmp(*argv, "--db-file") == 0) {
            argc--;
            argv++;

            if(argc) {
                db_file = *argv;

                argc--;
                argv++;
                continue;
            } else {
                PRINTLOG(LINKER, LOG_ERROR, "argument error");
                PRINTLOG(LINKER, LOG_ERROR, "LINKERDB FAILED");

                return -1;
            }
        }

    }


    if(db_file == NULL) {
        PRINTLOG(LINKER, LOG_ERROR, "no db file specified");
        PRINTLOG(LINKER, LOG_ERROR, "LINKERDB FAILED");

        return -1;
    }

    if(entrypoint_symbol == NULL) {
        PRINTLOG(LINKER, LOG_ERROR, "no entrypoint specified");
        PRINTLOG(LINKER, LOG_ERROR, "LINKERDB FAILED");

        return -1;
    }

    int32_t exit_code = 0;

    linkerdb_t* ldb = linkerdb_open(db_file);

    if(!ldb) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot open linkerdb");

        exit_code = -1;
        goto exit;
    }

    tosdb_database_t* db_system = tosdb_database_create_or_open(ldb->tdb, "system");

    if(!db_system) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot open system database");

        exit_code = -1;
        goto exit;
    }

    tosdb_table_t* tbl_sections = tosdb_table_create_or_open(db_system, "sections", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_modules = tosdb_table_create_or_open(db_system, "modules", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_symbols = tosdb_table_create_or_open(db_system, "symbols", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_relocations = tosdb_table_create_or_open(db_system, "relocations", 1 << 10, 512 << 10, 8);


    if(!tbl_sections || !tbl_modules || !tbl_symbols || !tbl_relocations) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot open system tables");

        exit_code = -1;
        goto exit;
    }

    tosdb_record_t* s_sym_rec = tosdb_table_create_record(tbl_symbols);

    if(!s_sym_rec) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create record for search");

        exit_code = -1;
        goto exit;
    }

    s_sym_rec->set_string(s_sym_rec, "name", entrypoint_symbol);

    linkedlist_t found_symbols = s_sym_rec->search_record(s_sym_rec);

    if(!found_symbols) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot search for entrypoint symbol");

        s_sym_rec->destroy(s_sym_rec);

        exit_code = -1;
        goto exit;
    }

    s_sym_rec->destroy(s_sym_rec);

    if(linkedlist_size(found_symbols) == 0) {
        PRINTLOG(LINKER, LOG_ERROR, "entrypoint symbol not found");

        s_sym_rec->destroy(s_sym_rec);

        exit_code = -1;
        goto exit;
    }

    s_sym_rec = (tosdb_record_t*)linkedlist_queue_pop(found_symbols);
    linkedlist_destroy(found_symbols);

    if(!s_sym_rec) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot get record for entrypoint symbol");

        exit_code = -1;
        goto exit;
    }

    uint64_t sec_id = 0;

    if(!s_sym_rec->get_int64(s_sym_rec, "section_id", (int64_t*)&sec_id)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot get section id for entrypoint symbol");

        s_sym_rec->destroy(s_sym_rec);

        exit_code = -1;
        goto exit;
    }

    uint64_t sym_id = 0;

    if(!s_sym_rec->get_int64(s_sym_rec, "id", (int64_t*)&sym_id)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot get symbol id for entrypoint symbol");

        s_sym_rec->destroy(s_sym_rec);

        exit_code = -1;
        goto exit;
    }

    s_sym_rec->destroy(s_sym_rec);


    PRINTLOG(LINKER, LOG_INFO, "entrypoint symbol %s id 0x%llx section id: 0x%llx", entrypoint_symbol, sym_id, sec_id);


    tosdb_record_t* s_sec_rec = tosdb_table_create_record(tbl_sections);

    if(!s_sec_rec) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create record for search");

        exit_code = -1;
        goto exit;
    }

    s_sec_rec->set_int64(s_sec_rec, "id", sec_id);

    if(!s_sec_rec->get_record(s_sec_rec)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot get record for search");

        exit_code = -1;
        goto exit;
    }

    uint64_t mod_id = 0;

    if(!s_sec_rec->get_int64(s_sec_rec, "module_id", (int64_t*)&mod_id)) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot get module id for entrypoint symbol");

        s_sec_rec->destroy(s_sec_rec);

        exit_code = -1;
        goto exit;
    }

    s_sec_rec->destroy(s_sec_rec);

    PRINTLOG(LINKER, LOG_INFO, "module id: 0x%llx\n", mod_id);

    linker_context_t* ctx = memory_malloc(sizeof(linker_context_t));

    if(!ctx) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot allocate linker context");

        exit_code = -1;
        goto exit;
    }

    ctx->entrypoint_symbol_id = sym_id;
    ctx->program_start = program_start;
    ctx->ldb = ldb;
    ctx->modules = hashmap_integer(16);
    ctx->got_table_buffer = buffer_new();
    ctx->got_symbol_index_map = hashmap_integer(1024);

    linker_global_offset_table_entry_t empty_got_entry = {0};

    buffer_append_bytes(ctx->got_table_buffer, (uint8_t*)&empty_got_entry, sizeof(linker_global_offset_table_entry_t)); // null entry
    buffer_append_bytes(ctx->got_table_buffer, (uint8_t*)&empty_got_entry, sizeof(linker_global_offset_table_entry_t)); // got itself

    if(linker_build_module(ctx, mod_id, true) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot build module");
        linker_destroy_context(ctx);

        exit_code = -1;
        goto exit;
    }

    PRINTLOG(LINKER, LOG_INFO, "modules built");

    linker_destroy_context(ctx);

exit:
    linkerdb_close(ldb);

    if(exit_code != 0) {
        print_error("LINKERDB FAILED");
    } else {
        print_success("LINKERDB OK");
    }

    return exit_code;
}
