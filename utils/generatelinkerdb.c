/**
 * @file generatelinkerdb.c
 * @brief Generate linker database.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE (512 << 20)
#include "setup.h"
#include <utils.h>
#include <buffer.h>
#include <data.h>
#include <sha2.h>
#include <bplustree.h>
#include <rbtree.h>
#include <map.h>
#include <xxhash.h>
#include <tosdb/tosdb.h>
#include <strings.h>
#include <bloomfilter.h>
#include <math.h>
#include <compression.h>
#include <deflate.h>
#include <zpack.h>
#include <binarysearch.h>
#include <tokenizer.h>
#include <set.h>
#include "elf64.h"
#include <linker.h>
#include <time.h>
#include <cache.h>
#include <crc.h>
#include <quicksort.h>

#define LINKERDB_CAP (32 << 20)

typedef struct linkerdb_t {
    uint64_t         capacity;
    FILE*            db_file;
    int32_t          fd;
    uint8_t*         mmap_res;
    buffer_t*        backend_buffer;
    tosdb_backend_t* backend;
    tosdb_t*         tdb;
    boolean_t        new_file;
} linkerdb_t;

typedef struct linkerdb_stats_t {
    uint64_t sec_count;
    uint64_t sym_count;
    uint64_t reloc_count;
    uint64_t nm_count;
    uint64_t implementation_count;
} linkerdb_stats_t;

int32_t     main(int32_t argc, char_t** argv);
linkerdb_t* linkerdb_open(const char_t* file, uint64_t capacity);
boolean_t   linkerdb_close(linkerdb_t* ldb);
boolean_t   linkerdb_gen_config(linkerdb_t* ldb, const char_t* entry_point, const uint64_t stack_size, const uint64_t program_base);
boolean_t   linkerdb_create_tables(linkerdb_t* ldb);
boolean_t   linkerdb_parse_object_file(linkerdb_t*       ldb,
                                       const char_t*     filename,
                                       linkerdb_stats_t* is);
boolean_t linkerdb_fix_reloc_symbol_section_ids(linkerdb_t* ldb);


linkerdb_t* linkerdb_open(const char_t* file, uint64_t capacity) {
    FILE* fp = fopen(file, "r+");
    boolean_t new_file = false;

    if(!fp) {
        PRINTLOG(TOSDB, LOG_WARNING, "cannot open db file for append try to create new one");

        fp = fopen(file, "w+");

        if(!fp) {
            print_error("cannot open db file");

            return NULL;
        }

        new_file = true;
    }

    fseek(fp, capacity - 1, SEEK_SET);
    int8_t zero = 0;
    fwrite(&zero, 1, 1, fp);
    fseek(fp, 0, SEEK_SET);

    int32_t fd = fileno(fp);

    uint8_t* mmap_res = mmap(NULL, capacity, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_SYNC, fd, 0);

    if(mmap_res == (void*)-1) {
        fclose(fp);
        print_error("cannot mmap db file");

        return NULL;
    }

    if(new_file) {
        memory_memclean(mmap_res, capacity);
    }

    buffer_t* buf = buffer_encapsulate(mmap_res, capacity);

    if(!buf) {
        munmap(mmap_res, capacity);
        fclose(fp);
        print_error("cannot create buffer");

        return NULL;
    }

    buffer_set_readonly(buf, false);

    tosdb_backend_t* bend = tosdb_backend_memory_from_buffer(buf);

    if(!bend) {
        buffer_set_readonly(buf, true);
        buffer_destroy(buf);
        munmap(mmap_res, capacity);
        fclose(fp);
        print_error("cannot create backend");

        return NULL;
    }

    tosdb_t* tdb = tosdb_new(bend, COMPRESSION_TYPE_DEFLATE);

    if(!tdb) {
        buffer_set_readonly(buf, true);
        tosdb_backend_close(bend);
        munmap(mmap_res, capacity);
        fclose(fp);
        print_error("cannot create backend");

        return NULL;
    }

    tosdb_cache_config_t cc = {0};
    cc.bloomfilter_size = 8 << 20;
    cc.index_data_size = 16 << 20;
    cc.secondary_index_data_size = 16 << 20;
    cc.valuelog_size = 32 << 20;

    if(!tosdb_cache_config_set(tdb, &cc)) {
        print_error("cannot set cache");
        buffer_set_readonly(buf, true);
        tosdb_backend_close(bend);
        munmap(mmap_res, capacity);
        fclose(fp);
        print_error("cannot create backend");

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
        print_error("cannot create backend");

        return NULL;
    }

    ldb->backend = bend;
    ldb->backend_buffer = buf;
    ldb->capacity = capacity;
    ldb->db_file = fp;
    ldb->fd = fd;
    ldb->mmap_res = mmap_res;
    ldb->tdb = tdb;
    ldb->new_file = new_file;

    return ldb;
}

boolean_t linkerdb_close(linkerdb_t* ldb) {
    if(!ldb) {
        return false;
    }

    if(!tosdb_close(ldb->tdb)) {
        print_error("cannot close db");
    }

    if(!tosdb_free(ldb->tdb)) {
        print_error("cannot free db");
    }

    buffer_set_readonly(ldb->backend_buffer, true);

    if(!tosdb_backend_close(ldb->backend)) {
        print_error("cannot close backend");
    }

    int32_t mss = msync(ldb->mmap_res, ldb->capacity, MS_SYNC);
    if(mss) {
        printf("msync res %i\n", mss);
        print_error("cannot sync contents");
    }

    munmap(ldb->mmap_res, ldb->capacity);

    fclose(ldb->db_file);

    memory_free(ldb);

    return true;
}

boolean_t linkerdb_gen_config(linkerdb_t* ldb, const char_t* entry_point, const uint64_t stack_size, const uint64_t program_base) {
    tosdb_t* tdb = ldb->tdb;

    tosdb_database_t* db = tosdb_database_create_or_open(tdb, "system");

    if(!db) {
        return false;
    }

    tosdb_table_t* tbl_config = tosdb_table_create_or_open(db, "config", 1 << 10, 512 << 10, 8);

    if(!tbl_config) {
        return false;
    }

    tosdb_table_column_add(tbl_config, "name", DATA_TYPE_STRING);
    tosdb_table_column_add(tbl_config, "value", DATA_TYPE_INT8_ARRAY);
    tosdb_table_index_create(tbl_config, "name", TOSDB_INDEX_PRIMARY);

    tosdb_record_t* rec = tosdb_table_create_record(tbl_config);
    rec->set_string(rec, "name", "entry_point");
    rec->set_data(rec, "value", DATA_TYPE_INT8_ARRAY, strlen(entry_point), entry_point);
    rec->upsert_record(rec);
    rec->destroy(rec);

    rec = tosdb_table_create_record(tbl_config);
    rec->set_string(rec, "name", "stack_size");
    rec->set_data(rec, "value", DATA_TYPE_INT8_ARRAY, sizeof(uint64_t), &stack_size);
    rec->upsert_record(rec);
    rec->destroy(rec);

    rec = tosdb_table_create_record(tbl_config);
    rec->set_string(rec, "name", "program_base");
    rec->set_data(rec, "value", DATA_TYPE_INT8_ARRAY, sizeof(uint64_t), &program_base);
    rec->upsert_record(rec);
    rec->destroy(rec);

    return tosdb_table_close(tbl_config);
}

boolean_t linkerdb_create_tables(linkerdb_t* ldb) {
    tosdb_t* tdb = ldb->tdb;

    tosdb_database_t* db = tosdb_database_create_or_open(tdb, "system");

    if(!db) {
        return false;
    }

    tosdb_table_t* tbl_modules = tosdb_table_create_or_open(db, "modules", 1 << 10, 512 << 10, 8);

    if(!tbl_modules) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_modules, "id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_modules, "name", DATA_TYPE_STRING)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_modules, "id", TOSDB_INDEX_PRIMARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_modules, "name", TOSDB_INDEX_UNIQUE)) {
        return false;
    }

    tosdb_sequence_create_or_open(db, "modules_module_id", 1, 10);

    tosdb_table_t* tbl_implementations = tosdb_table_create_or_open(db, "implementations", 1 << 10, 512 << 10, 8);

    if(!tbl_implementations) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_implementations, "id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_implementations, "name", DATA_TYPE_STRING)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_implementations, "id", TOSDB_INDEX_PRIMARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_implementations, "name", TOSDB_INDEX_UNIQUE)) {
        return false;
    }

    tosdb_sequence_create_or_open(db, "implementations_implementation_id", 1, 10);

    tosdb_table_t* tbl_sections = tosdb_table_create_or_open(db, "sections", 1 << 10, 512 << 10, 8);

    if(!tbl_sections) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, "id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, "module_id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, "implementation_id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, "name", DATA_TYPE_STRING)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, "alignment", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, "class", DATA_TYPE_INT8)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, "size", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, "type", DATA_TYPE_INT8)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_sections, "value", DATA_TYPE_INT8_ARRAY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_sections, "id", TOSDB_INDEX_PRIMARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_sections, "implementation_id", TOSDB_INDEX_SECONDARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_sections, "name", TOSDB_INDEX_SECONDARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_sections, "module_id", TOSDB_INDEX_SECONDARY)) {
        return false;
    }

    tosdb_sequence_create_or_open(db, "sections_section_id", 1, 10);

    tosdb_table_t* tbl_symbols = tosdb_table_create_or_open(db, "symbols", 1 << 10, 512 << 10, 8);

    if(!tbl_symbols) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, "id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, "implementation_id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, "section_id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, "name", DATA_TYPE_STRING)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, "type", DATA_TYPE_INT8)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, "scope", DATA_TYPE_INT8)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, "value", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_symbols, "size", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_symbols, "id", TOSDB_INDEX_PRIMARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_symbols, "implementation_id", TOSDB_INDEX_SECONDARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_symbols, "section_id", TOSDB_INDEX_SECONDARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_symbols, "name", TOSDB_INDEX_SECONDARY)) {
        return false;
    }

    tosdb_sequence_create_or_open(db, "symbols_symbol_id", 1, 10);

    tosdb_table_t* tbl_relocations = tosdb_table_create_or_open(db, "relocations", 8 << 10, 1 << 20, 8);

    if(!tbl_relocations) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, "id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, "section_id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, "symbol_id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, "symbol_name", DATA_TYPE_STRING)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, "symbol_section_id", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, "type", DATA_TYPE_INT8)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, "offset", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_column_add(tbl_relocations, "addend", DATA_TYPE_INT64)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_relocations, "id", TOSDB_INDEX_PRIMARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_relocations, "section_id", TOSDB_INDEX_SECONDARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_relocations, "symbol_name", TOSDB_INDEX_SECONDARY)) {
        return false;
    }

    if(!tosdb_table_index_create(tbl_relocations, "symbol_section_id", TOSDB_INDEX_SECONDARY)) {
        return false;
    }

    tosdb_sequence_create_or_open(db, "relocations_relocation_id", 1, 10);

    return true;
}

static boolean_t linkerdb_clear_relocation_references_at_section(linkerdb_t* ldb, int64_t section_id) {
    tosdb_database_t* db_system = tosdb_database_create_or_open(ldb->tdb, "system");
    tosdb_table_t* tbl_relocations = tosdb_table_create_or_open(db_system, "relocations", 8 << 10, 1 << 20, 8);

    tosdb_record_t* s_reloc_rec = tosdb_table_create_record(tbl_relocations);

    if(!s_reloc_rec) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create record");

        return false;
    }

    if(!s_reloc_rec->set_int64(s_reloc_rec, "section_id", section_id)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot set 'section_id'");
        s_reloc_rec->destroy(s_reloc_rec);

        return false;
    }

    list_t* s_reloc_list = s_reloc_rec->search_record(s_reloc_rec);

    s_reloc_rec->destroy(s_reloc_rec);

    boolean_t error = false;

    if(list_size(s_reloc_list)) {
        iterator_t* it = list_iterator_create(s_reloc_list);

        if(!it) {
            list_destroy_with_data(s_reloc_list);
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create iterator");

            return false;
        }

        while(it->end_of_iterator(it) != 0) {
            tosdb_record_t* f_rec = (tosdb_record_t*)it->delete_item(it);

            int64_t f_id = 0;

            if(!f_rec->get_int64(f_rec, "id", &f_id)) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot get 'id'");
                error = true;
            } else {
                tosdb_record_t* del_rec = tosdb_table_create_record(tbl_relocations);

                PRINTLOG(TOSDB, LOG_TRACE, "relocation 0x%llx at section %llx deleting", f_id, section_id);

                if(!del_rec) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot create record");
                    error = true;
                } else if(!del_rec->set_int64(del_rec, "id", f_id)) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot set 'id'");
                    error = true;
                } else if(!del_rec->delete_record(del_rec)) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot delete record");
                    error = true;
                } else {
                    PRINTLOG(TOSDB, LOG_DEBUG, "relocation 0x%llx at section %llx deleted", f_id, section_id);
                }

                if(del_rec) {
                    del_rec->destroy(del_rec);
                }
            }

            f_rec->destroy(f_rec);

            it = it->next(it);
        }

        it->destroy(it);

    }

    list_destroy(s_reloc_list);

    s_reloc_rec = tosdb_table_create_record(tbl_relocations);

    if(!s_reloc_rec) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create record");

        return false;
    }

    if(!s_reloc_rec->set_int64(s_reloc_rec, "symbol_section_id", section_id)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot set 'symbol_section_id'");
        s_reloc_rec->destroy(s_reloc_rec);

        return false;
    }

    s_reloc_list = s_reloc_rec->search_record(s_reloc_rec);

    s_reloc_rec->destroy(s_reloc_rec);

    if(list_size(s_reloc_list)) {
        iterator_t* it = list_iterator_create(s_reloc_list);

        if(!it) {
            list_destroy_with_data(s_reloc_list);
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create iterator");

            return false;
        }

        while(it->end_of_iterator(it) != 0) {
            tosdb_record_t* f_rec = (tosdb_record_t*)it->delete_item(it);

            if(f_rec->is_deleted(f_rec)) {
                f_rec->destroy(f_rec);
                it = it->next(it);

                continue;
            }

            int64_t f_id = 0;

            if(!f_rec->get_int64(f_rec, "id", &f_id)) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot get 'id'");
                error = true;
            } else {
                PRINTLOG(TOSDB, LOG_TRACE, "relocation 0x%llx referenced by section %llx updating", f_id, section_id);

                if(!f_rec->set_int64(f_rec, "symbol_id", 0)) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot set 'symbol_id'");
                    error = true;
                } else if(!f_rec->set_int64(f_rec, "symbol_section_id", 0)) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot set 'symbol_section_id'");
                    error = true;
                } else if(!f_rec->upsert_record(f_rec)) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot update record");
                    error = true;
                } else {
                    PRINTLOG(TOSDB, LOG_DEBUG, "relocation 0x%llx updated", f_id);
                }
            }

            f_rec->destroy(f_rec);

            it = it->next(it);
        }

        it->destroy(it);

    }

    list_destroy(s_reloc_list);


    return !error;
}

static boolean_t linkerdb_clear_symbol_references(linkerdb_t* ldb, int64_t section_id) {
    tosdb_database_t* db_system = tosdb_database_create_or_open(ldb->tdb, "system");
    tosdb_table_t* tbl_symbols = tosdb_table_create_or_open(db_system, "symbols", 1 << 10, 512 << 10, 8);

    tosdb_record_t* s_sym_rec = tosdb_table_create_record(tbl_symbols);

    if(!s_sym_rec) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create record");

        return false;
    }

    if(!s_sym_rec->set_int64(s_sym_rec, "section_id", section_id)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot set 'section_id'");
        s_sym_rec->destroy(s_sym_rec);

        return false;
    }

    list_t* s_sym_list = s_sym_rec->search_record(s_sym_rec);

    s_sym_rec->destroy(s_sym_rec);

    boolean_t error = false;

    if(list_size(s_sym_list)) {
        iterator_t* it = list_iterator_create(s_sym_list);

        if(!it) {
            list_destroy_with_data(s_sym_list);
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create iterator");

            return false;
        }

        while(it->end_of_iterator(it) != 0) {
            tosdb_record_t* f_rec = (tosdb_record_t*)it->delete_item(it);

            int64_t f_id = 0;

            char_t* f_name = NULL;

            if(!f_rec->get_int64(f_rec, "id", &f_id)) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot get 'id'");
                error = true;
            } else if(!f_rec->get_string(f_rec, "name", &f_name)) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot get 'name'");
                error = true;
            } else {
                tosdb_record_t* del_rec = tosdb_table_create_record(tbl_symbols);

                PRINTLOG(TOSDB, LOG_TRACE, "symbol '%s' with id %llx deleting", f_name, f_id);

                if(!del_rec) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot create record");
                    error = true;
                } else if(!del_rec->set_int64(del_rec, "id", f_id)) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot set 'id'");
                    error = true;
                } else if(!del_rec->delete_record(del_rec)) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot delete record");
                    error = true;
                } else {
                    PRINTLOG(TOSDB, LOG_DEBUG, "symbol '%s' with id %llx deleted", f_name, f_id);
                }

                if(del_rec) {
                    del_rec->destroy(del_rec);
                }
            }

            memory_free(f_name);

            f_rec->destroy(f_rec);

            it = it->next(it);
        }

        it->destroy(it);

    }

    list_destroy(s_sym_list);

    return !error;
}

static boolean_t linkerdb_clear_implementation_references(linkerdb_t* ldb, int64_t implementation_id) {
    tosdb_database_t* db_system = tosdb_database_create_or_open(ldb->tdb, "system");
    tosdb_table_t* tbl_sections = tosdb_table_create_or_open(db_system, "sections", 1 << 10, 512 << 10, 8);

    tosdb_record_t* s_sec_rec = tosdb_table_create_record(tbl_sections);

    if(!s_sec_rec) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create record");

        return false;
    }

    if(!s_sec_rec->set_int64(s_sec_rec, "implementation_id", implementation_id)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot set 'implementation_id'");
        s_sec_rec->destroy(s_sec_rec);

        return false;
    }

    list_t* s_sec_list = s_sec_rec->search_record(s_sec_rec);

    s_sec_rec->destroy(s_sec_rec);

    boolean_t error = false;

    if(list_size(s_sec_list)) {
        iterator_t* it = list_iterator_create(s_sec_list);

        if(!it) {
            list_destroy_with_data(s_sec_list);
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create iterator");

            return false;
        }

        while(it->end_of_iterator(it) != 0) {
            tosdb_record_t* f_rec = (tosdb_record_t*)it->delete_item(it);

            int64_t f_sec_id = 0;

            if(!f_rec->get_int64(f_rec, "id", &f_sec_id)) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot get 'id'");
            } else {
                error |= !linkerdb_clear_symbol_references(ldb, f_sec_id);
                error |= !linkerdb_clear_relocation_references_at_section(ldb, f_sec_id);
            }

            f_rec->destroy(f_rec);

            tosdb_record_t* del_rec = tosdb_table_create_record(tbl_sections);

            PRINTLOG(TOSDB, LOG_TRACE, "section with id %llx deleting", f_sec_id);

            if(!del_rec) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot create record");
                error = true;
            } else if(!del_rec->set_int64(del_rec, "id", f_sec_id)) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot set 'id'");
                error = true;
            } else if(!del_rec->delete_record(del_rec)) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot delete record");
                error = true;
            } else {
                PRINTLOG(TOSDB, LOG_DEBUG, "section with id %llx deleted", f_sec_id);
            }

            if(del_rec) {
                del_rec->destroy(del_rec);
            }

            it = it->next(it);
        }

        it->destroy(it);

    }

    list_destroy(s_sec_list);

    return !error;
}


static boolean_t linkerdb_create_clean_implementation(linkerdb_t* ldb, const char_t* filename, linkerdb_stats_t* is, int64_t* implementation_id) {

    tosdb_database_t* db_system = tosdb_database_create_or_open(ldb->tdb, "system");

    tosdb_table_t* tbl_implementations = tosdb_table_create_or_open(db_system, "implementations", 1 << 10, 512 << 10, 8);

    if(!tbl_implementations) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot open table 'implementations'");

        return false;
    }

    uint64_t impl_name_end = 0;
    uint64_t impl_name_start = 0;

    for(uint64_t i = strlen(filename); i > 0; i--) {
        if(filename[i] == '.') {
            impl_name_end = i;
        }

        if(filename[i] == '/') {
            impl_name_start = i + 1;
            break;
        }
    }

    char_t* impl_name = memory_malloc(impl_name_end - impl_name_start + 1);
    memory_memcopy(filename + impl_name_start, impl_name, impl_name_end - impl_name_start);

    tosdb_record_t* rec_impl = tosdb_table_create_record(tbl_implementations);

    if(!rec_impl) {
        memory_free(impl_name);
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create record for table 'implementations'");

        return false;
    }

    if(!rec_impl->set_string(rec_impl, "name", impl_name)) {
        memory_free(impl_name);
        rec_impl->destroy(rec_impl);
        PRINTLOG(TOSDB, LOG_ERROR, "cannot set 'name' for table 'implementations'");

        return false;
    }

    memory_free(impl_name);

    if(rec_impl->get_record(rec_impl)) {
        if(!rec_impl->get_int64(rec_impl, "id", implementation_id)) {
            rec_impl->destroy(rec_impl);
            PRINTLOG(TOSDB, LOG_ERROR, "cannot get 'id' for table 'implementations'");

            return false;
        }

        if(!linkerdb_clear_implementation_references(ldb, *implementation_id)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot clean implementation");
            rec_impl->destroy(rec_impl);

            return false;
        }
    }

    tosdb_sequence_t* seq_impl = tosdb_sequence_create_or_open(db_system, "implementations_implementation_id", 1, 10);

    uint64_t impl_id = tosdb_sequence_next(seq_impl);
    *implementation_id = impl_id;

    rec_impl->set_int64(rec_impl, "id", impl_id);

    if(!rec_impl->upsert_record(rec_impl)) {
        rec_impl->destroy(rec_impl);
        PRINTLOG(TOSDB, LOG_ERROR, "cannot insert record into table 'implementations'");

        return false;
    }

    rec_impl->destroy(rec_impl);

    is->implementation_count++;

    return true;
}

boolean_t linkerdb_parse_object_file(linkerdb_t*       ldb,
                                     const char_t*     filename,
                                     linkerdb_stats_t* is){

    hashmap_t* section_ids = hashmap_integer(128);
    hashmap_t* symbol_ids = hashmap_integer(128);

    tosdb_database_t* db_system = tosdb_database_create_or_open(ldb->tdb, "system");

    int64_t implementation_id = 0;

    if(!linkerdb_create_clean_implementation(ldb, filename, is, &implementation_id)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create clean implementation");
        hashmap_destroy(section_ids);
        hashmap_destroy(symbol_ids);

        return false;
    }

    FILE* fp = fopen(filename, "r");

    if(!fp) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot open file '%s'", filename);
        hashmap_destroy(section_ids);
        hashmap_destroy(symbol_ids);

        return false;
    }

    elf_indent_t e_indent;

    fread(&e_indent, sizeof(elf_indent_t), 1, fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t e_class = e_indent.class;
    uint16_t e_shnum = 0;
    uint64_t e_shoff = 0;
    uint16_t e_shstrndx = 0;
    uint64_t e_shsize = 0;

    if(e_class == ELFCLASS32) {

        elf32_hdr_t hdr;
        fread(&hdr, sizeof(elf32_hdr_t), 1, fp);

        e_shnum = hdr.e_shnum;
        e_shoff = hdr.e_shoff;
        e_shstrndx = hdr.e_shstrndx;
        e_shsize = sizeof(elf32_shdr_t) * e_shnum;

    } else if(e_class == ELFCLASS64) {

        elf64_hdr_t hdr;
        fread(&hdr, sizeof(elf64_hdr_t), 1, fp);

        e_shnum = hdr.e_shnum;
        e_shoff = hdr.e_shoff;
        e_shstrndx = hdr.e_shstrndx;
        e_shsize = sizeof(elf64_shdr_t) * e_shnum;

    } else {
        PRINTLOG(TOSDB, LOG_ERROR, "unknown file class %i", e_indent.class );
        fclose(fp);
        hashmap_destroy(section_ids);
        hashmap_destroy(symbol_ids);

        return false;
    }

    if(e_shnum == 0) {
        PRINTLOG(TOSDB, LOG_ERROR, "no section in file %s", filename);
        fclose(fp);
        hashmap_destroy(section_ids);
        hashmap_destroy(symbol_ids);

        return false;
    }

    uint8_t* sections  = memory_malloc(e_shsize);

    if(!sections) {
        printf("cannot allocate sections for file %s", filename);
        fclose(fp);
        hashmap_destroy(section_ids);
        hashmap_destroy(symbol_ids);

        return false;
    }

    fseek(fp, e_shoff, SEEK_SET);
    fread(sections, e_shsize, 1, fp);


    uint64_t shstrtab_size = ELF_SECTION_SIZE(e_class, sections, e_shstrndx);
    uint64_t shstrtab_offset = ELF_SECTION_OFFSET(e_class, sections, e_shstrndx);

    char_t* shstrtab = memory_malloc(shstrtab_size);

    if(!shstrtab) {
        memory_free(sections);
        PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate section string table for file %s", filename);
        fclose(fp);
        hashmap_destroy(section_ids);
        hashmap_destroy(symbol_ids);

        return false;
    }

    fseek(fp, shstrtab_offset, SEEK_SET);
    fread(shstrtab, shstrtab_size, 1, fp);

    int8_t req_sec_found = 0;
    char_t* strtab = NULL;
    uint8_t* symbols = NULL;
    uint64_t symbol_count = 0;

    tosdb_table_t* tbl_sections = tosdb_table_create_or_open(db_system, "sections", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_modules = tosdb_table_create_or_open(db_system, "modules", 1 << 10, 512 << 10, 8);


    tosdb_sequence_t* seq_sections = tosdb_sequence_create_or_open(db_system, "sections_section_id", 1, 10);
    tosdb_sequence_t* seq_modules = tosdb_sequence_create_or_open(db_system, "modules_module_id", 1, 10);


    boolean_t error = false;

    int64_t module_id = 0;

    for(uint16_t sec_idx = 0; sec_idx < e_shnum; sec_idx++) {
        uint64_t sec_size = ELF_SECTION_SIZE(e_class, sections, sec_idx);
        uint64_t sec_offset = ELF_SECTION_OFFSET(e_class, sections, sec_idx);
        char_t* sec_name = shstrtab + ELF_SECTION_NAME(e_class, sections, sec_idx);

        if(strcmp(".___module___", sec_name) == 0 && sec_size) {
            char_t* module_name = memory_malloc(sec_size + 1);

            if(!module_name) {
                print_error("cannot allocate module name");
                error = true;

                break;
            }

            fseek(fp, sec_offset, SEEK_SET);
            fread(module_name, sec_size, 1, fp);

            tosdb_record_t* mn_rec = tosdb_table_create_record(tbl_modules);

            if(!mn_rec) {
                memory_free(module_name);
                print_error("cannot create module name search record");
                error = true;

                break;
            }

            mn_rec->set_string(mn_rec, "name", module_name);

            if(mn_rec->get_record(mn_rec)) {
                mn_rec->get_int64(mn_rec, "id", &module_id);
            } else {
                module_id = tosdb_sequence_next(seq_modules);

                mn_rec->set_int64(mn_rec, "id", module_id);

                if(!mn_rec->upsert_record(mn_rec)) {
                    memory_free(module_name);
                    print_error("cannot insert module into linker db");
                    error = true;

                    break;
                }

                is->nm_count++;
            }

            mn_rec->destroy(mn_rec);

            memory_free(module_name);

            break;
        }
    }

    if(!module_id) {
        print_error("module name not found");
        PRINTLOG(LINKER, LOG_ERROR, "module name not found at file %s", filename);

        error = true;
    }

    if(error) {
        goto close;
    }

    for(uint64_t sec_idx = 0; sec_idx < e_shnum; sec_idx++) {
        uint64_t sec_size = ELF_SECTION_SIZE(e_class, sections, sec_idx);
        uint64_t sec_offset = ELF_SECTION_OFFSET(e_class, sections, sec_idx);
        char_t* sec_name = shstrtab + ELF_SECTION_NAME(e_class, sections, sec_idx);

        if(strcmp(".strtab", sec_name) == 0) {
            strtab = memory_malloc(sec_size);

            if(!strtab) {
                error = true;

                break;
            }

            fseek(fp, sec_offset, SEEK_SET);
            fread(strtab, sec_size, 1, fp);
            req_sec_found++;
        }

        if(ELF_SECTION_TYPE(e_class, sections, sec_idx) == SHT_SYMTAB) {
            symbols = memory_malloc(sec_size);

            if(!symbols) {
                error = true;

                break;
            }

            fseek(fp, sec_offset, SEEK_SET);
            fread(symbols, sec_size, 1, fp);
            req_sec_found++;

            symbol_count = ELF_SYMBOL_COUNT(e_class, sec_size);
        }

        uint8_t sec_type = LINKER_SECTION_TYPE_TEXT;

        if(strstarts(sec_name, ".data.rel.ro") == 0) {
            sec_type = LINKER_SECTION_TYPE_ROREL;
        } else if(strstarts(sec_name, ".data") == 0) {
            sec_type = LINKER_SECTION_TYPE_DATA;
        } else if(strstarts(sec_name, ".rodata") == 0) {
            sec_type = LINKER_SECTION_TYPE_RODATA;
        } else if(strstarts(sec_name, ".bss") == 0) {
            sec_type = LINKER_SECTION_TYPE_BSS;
        }

        if(sec_size &&   (
               strstarts(sec_name, ".text") == 0 ||
               strstarts(sec_name, ".data") == 0 ||
               strstarts(sec_name, ".rodata") == 0 ||
               strstarts(sec_name, ".bss") == 0
               )) {
            tosdb_record_t* rec = tosdb_table_create_record(tbl_sections);

            if(!rec) {
                print_error("cannot create section record");
                error = true;

                break;
            }

            int64_t section_id = tosdb_sequence_next(seq_sections);

            hashmap_put(section_ids, (void*)sec_idx, (void*)(uint64_t)section_id);

            rec->set_int64(rec, "id", section_id);
            rec->set_int64(rec, "implementation_id", implementation_id);
            rec->set_string(rec, "name", sec_name);
            rec->set_int64(rec, "module_id", module_id);
            rec->set_int64(rec, "alignment", ELF_SECTION_ALIGN(e_class, sections, sec_idx));
            rec->set_int8(rec, "class", e_class);
            rec->set_int64(rec, "size", sec_size);
            rec->set_int8(rec, "type", sec_type);

            if(strstarts(sec_name, ".bss") != 0) {
                uint8_t* sec_data = memory_malloc(sec_size);

                if(!sec_data) {
                    rec->destroy(rec);
                    error = true;

                    break;
                }

                fseek(fp, sec_offset, SEEK_SET);
                fread(sec_data, sec_size, 1, fp);

                rec->set_data(rec, "value", DATA_TYPE_INT8_ARRAY, sec_size, sec_data);

                memory_free(sec_data);

            }

            boolean_t res = rec->upsert_record(rec);
            rec->destroy(rec);

            if(!res) {
                print_error("cannot insert section");
                error = true;

                break;
            }

            is->sec_count++;
        }
    }

    if(req_sec_found != 2) {
        print_error("required sections not found");
        error = true;
    }

    if(error) {
        goto close;
    }

    tosdb_table_t* tbl_symbols = tosdb_table_create_or_open(db_system, "symbols", 1 << 10, 512 << 10, 8);
    tosdb_sequence_t* seq_symbols = tosdb_sequence_create_or_open(db_system, "symbols_symbol_id", 1, 10);

    for(uint16_t sym_idx = 0; sym_idx < symbol_count; sym_idx++) {
        uint8_t sym_type = ELF_SYMBOL_TYPE(e_class, symbols, sym_idx);

        if(sym_type > STT_SECTION) {
            continue;
        }

        uint64_t sym_shndx = ELF_SYMBOL_SHNDX(e_class, symbols, sym_idx);

        if(sym_shndx == 0 || sym_shndx >= SHN_LOPROC) {
            continue;
        }

        char_t* sym_name = strtab + ELF_SYMBOL_NAME(e_class, symbols, sym_idx);
        uint64_t sym_sec_id = (int64_t)hashmap_get(section_ids, (void*)sym_shndx);
        uint8_t sym_scope = ELF_SYMBOL_SCOPE(e_class, symbols, sym_idx);
        uint64_t sym_value = ELF_SYMBOL_VALUE(e_class, symbols, sym_idx);
        uint64_t sym_size = ELF_SYMBOL_SIZE(e_class, symbols, sym_idx);

        if(sym_type == STT_SECTION) {
            sym_name = shstrtab + ELF_SECTION_NAME(e_class, sections, sym_shndx);
        }

        if(strcmp(sym_name, "___module___") == 0) {
            continue;
        }

        if(sym_sec_id == 0) {
            print_error("unknown symbol");
            error = true;

            break;
        }

        if(strlen(sym_name) == 0) {
            print_error("symbol name not found");
            error = true;

            break;
        }

        boolean_t free_sym_name = false;

        if(sym_scope == STB_LOCAL) {
            sym_name = strcat(shstrtab + ELF_SECTION_NAME(e_class, sections, sym_shndx), sym_name);
            free_sym_name = true;
        }

        tosdb_record_t* rec = tosdb_table_create_record(tbl_symbols);

        int64_t symbol_id = tosdb_sequence_next(seq_symbols);

        hashmap_put(symbol_ids, (void*)(uint64_t)sym_idx, (void*)(uint64_t)symbol_id);

        rec->set_int64(rec, "id", symbol_id);
        rec->set_int64(rec, "implementation_id", implementation_id);
        rec->set_int64(rec, "section_id", sym_sec_id);
        rec->set_string(rec, "name", sym_name);
        rec->set_int8(rec, "type", sym_type);
        rec->set_int8(rec, "scope", sym_scope);
        rec->set_int64(rec, "value", sym_value);
        rec->set_int64(rec, "size", sym_size);

        boolean_t res = rec->upsert_record(rec);
        rec->destroy(rec);

        if(free_sym_name) {
            memory_free(sym_name);
        }

        if(!res) {
            print_error("cannot insert symbol");
            error = true;

            break;
        }

        is->sym_count++;
    }

    if(error) {
        goto close;
    }

    tosdb_table_t* tbl_relocations = tosdb_table_create_or_open(db_system, "relocations", 8 << 10, 1 << 20, 8);
    tosdb_sequence_t* seq_relocations = tosdb_sequence_create_or_open(db_system, "relocations_relocation_id", 1, 10);

    for(uint16_t sec_idx = 0; sec_idx < e_shnum; sec_idx++) {
        uint8_t is_rela = 0;

        char_t* sec_name = shstrtab + ELF_SECTION_NAME(e_class, sections, sec_idx);

        if(strstarts(sec_name, ".rela.") == 0) {
            is_rela = 1;
        } else if(strstarts(sec_name, ".rel.") == 0) {
            is_rela = 0;
        } else {
            continue;
        }

        const char_t* tmp_section_name = sec_name + strlen(".rel") + is_rela;

        if(strcmp(tmp_section_name, ".eh_frame") == 0) {
            continue;
        }

        if(strstarts(tmp_section_name, ".data") == 0 && strstarts(tmp_section_name, ".data.rel.ro") != 0) {
            PRINTLOG(LINKER, LOG_WARNING, "relocation at data section %s at %s", tmp_section_name, filename);
        }

        int64_t reloc_sec_id = ELF_SECTION_INFO(e_class, sections, sec_idx);

        if(!reloc_sec_id) {
            print_error("cannot find section of relocations");
            error = true;

            break;
        }

        reloc_sec_id = (int64_t)hashmap_get(section_ids, (void*)(uint64_t)reloc_sec_id);

        uint64_t sec_size = ELF_SECTION_SIZE(e_class, sections, sec_idx);
        uint64_t sec_offset = ELF_SECTION_OFFSET(e_class, sections, sec_idx);

        uint8_t* relocs = memory_malloc(sec_size);

        if(!relocs) {
            print_error("cannot create reloc array");
            error = true;

            break;
        }

        fseek(fp, sec_offset, SEEK_SET);
        fread(relocs, sec_size, 1, fp);

        uint64_t reloc_count = ELF_RELOC_COUNT(e_class, is_rela, sec_size);

        for(uint64_t reloc_idx = 0; reloc_idx < reloc_count; reloc_idx++) {

            uint64_t reloc_offset = ELF_RELOC_OFFSET(e_class, is_rela, relocs, reloc_idx);
            uint32_t reloc_type = ELF_RELOC_TYPE(e_class, is_rela, relocs, reloc_idx);
            int64_t reloc_symidx = ELF_RELOC_SYMIDX(e_class, is_rela, relocs, reloc_idx);
            int64_t reloc_sym_sec_id = ELF_SYMBOL_SHNDX(e_class, symbols, reloc_symidx);


            char_t* reloc_sym_name = strtab + ELF_SYMBOL_NAME(e_class, symbols, reloc_symidx);
            uint8_t reloc_sym_type = ELF_SYMBOL_TYPE(e_class, symbols, reloc_symidx);

            if(reloc_sym_type == STT_SECTION) {
                reloc_sym_name = shstrtab + ELF_SECTION_NAME(e_class, sections, reloc_sym_sec_id);
            }

            boolean_t free_reloc_sym_name = false;

            if(reloc_sym_type == STB_LOCAL) {
                reloc_sym_name = strcat(shstrtab + ELF_SECTION_NAME(e_class, sections, reloc_sym_sec_id), reloc_sym_name);
                free_reloc_sym_name = true;
            }

            if(strlen(reloc_sym_name) == 0) {
                printf("!!! at file %s at sec %s symbol name of reloc %llx unknown", filename, sec_name, reloc_idx);
                print_error("cannot countinue");

                if(free_reloc_sym_name) {
                    memory_free(reloc_sym_name);
                }

                error = true;
                break;
            }

            int64_t reloc_sym_id = 0;

            if(reloc_sym_sec_id) {
                reloc_sym_sec_id = (int64_t)hashmap_get(section_ids, (void*)(uint64_t)reloc_sym_sec_id);
                reloc_sym_id = (int64_t)hashmap_get(symbol_ids, (void*)reloc_symidx);
            }

            if(e_class == ELFCLASS32) {
                switch (reloc_type) {
                case R_386_16:
                    reloc_type = LINKER_RELOCATION_TYPE_32_16;
                    break;
                case R_386_32:
                    reloc_type = LINKER_RELOCATION_TYPE_32_32;
                    break;
                case R_386_PC16:
                    reloc_type = LINKER_RELOCATION_TYPE_32_PC16;
                    break;
                case R_386_PC32:
                    reloc_type = LINKER_RELOCATION_TYPE_32_PC32;
                    break;
                default:
                    print_error("unknown 32 bit reloc");
                    error = true;
                    break;
                }
            } else {
                switch (reloc_type) {
                case R_X86_64_32:
                    reloc_type = LINKER_RELOCATION_TYPE_64_32;
                    break;
                case R_X86_64_32S:
                    reloc_type = LINKER_RELOCATION_TYPE_64_32S;
                    break;
                case R_X86_64_64:
                    reloc_type = LINKER_RELOCATION_TYPE_64_64;
                    break;
                case R_X86_64_PC32:
                case R_X86_64_PLT32:
                    reloc_type = LINKER_RELOCATION_TYPE_64_PC32;
                    break;
                case R_X86_64_PC64:
                    reloc_type = LINKER_RELOCATION_TYPE_64_PC64;
                    break;
                case R_X86_64_GOT64:
                    reloc_type = LINKER_RELOCATION_TYPE_64_GOT64;
                    break;
                case R_X86_64_GOTOFF64:
                    reloc_type = LINKER_RELOCATION_TYPE_64_GOTOFF64;
                    break;
                case R_X86_64_GOTPC64:
                    reloc_type = LINKER_RELOCATION_TYPE_64_GOTPC64;
                    break;
                default:
                    print_error("unknown 64 bit reloc type 0x%llx", reloc_type);
                    error = true;
                    break;
                }
            }

            if(error) {
                print_error("error at reloc type");
                break;
            }

            tosdb_record_t* rec = tosdb_table_create_record(tbl_relocations);

            if(!rec) {
                print_error("cannot create record object");
                error = true;
                break;
            }

            int64_t reloc_id = tosdb_sequence_next(seq_relocations);

            rec->set_int64(rec, "id", reloc_id);
            rec->set_int64(rec, "section_id", reloc_sec_id);
            rec->set_int64(rec, "symbol_id", reloc_sym_id);
            rec->set_string(rec, "symbol_name", reloc_sym_name);
            rec->set_int64(rec, "symbol_section_id", reloc_sym_sec_id);
            rec->set_int8(rec, "type", reloc_type);
            rec->set_int64(rec, "offset", reloc_offset);

            if(is_rela) {
                int64_t reloc_addend = ELF_RELOC_ADDEND(e_class, relocs, reloc_idx);

                rec->set_int64(rec, "addend", reloc_addend);
            }


            if(free_reloc_sym_name) {
                memory_free(reloc_sym_name);
            }

            boolean_t res = rec->upsert_record(rec);
            rec->destroy(rec);

            if(!res) {
                print_error("cannot insert reloc");
                error = true;

                break;
            }

            is->reloc_count++;
        }

        memory_free(relocs);

        if(error) {
            break;
        }
    }

close:
    memory_free(symbols);
    memory_free(strtab);
    memory_free(sections);
    memory_free(shstrtab);
    hashmap_destroy(section_ids);
    hashmap_destroy(symbol_ids);

    fclose(fp);

    return !error;
}

boolean_t linkerdb_fix_reloc_symbol_section_ids(linkerdb_t* ldb) {
    tosdb_database_t* db_system = tosdb_database_create_or_open(ldb->tdb, "system");
    tosdb_table_t* tbl_relocations = tosdb_table_create_or_open(db_system, "relocations", 8 << 10, 1 << 20, 8);
    tosdb_table_t* tbl_symbols = tosdb_table_create_or_open(db_system, "symbols", 1 << 10, 512 << 10, 8);

    tosdb_record_t* s_recs_need = tosdb_table_create_record(tbl_relocations);

    if(!s_recs_need) {
        print_error("cannot create search record");

        return false;
    }

    s_recs_need->set_int64(s_recs_need, "symbol_section_id", 0);

    list_t* res_recs = s_recs_need->search_record(s_recs_need);

    if(!res_recs) {
        s_recs_need->destroy(s_recs_need);
        print_error("search records null");

        return false;
    }

    uint64_t need_fix_count = list_size(res_recs);
    PRINTLOG(LINKER, LOG_INFO, "record probably need fix count: %lli", need_fix_count);

    iterator_t* iter = list_iterator_create(res_recs);

    if(!iter) {
        s_recs_need->destroy(s_recs_need);
        list_destroy_with_data(res_recs);
        print_error("cannot create iterator");

        return false;
    }

    boolean_t error = false;

    set_t* set_nf = set_string();

    uint64_t nf_count = 0;

    set_t* set_dup = set_string();

    uint64_t dup_count = 0;

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_record_t* reloc_rec = (tosdb_record_t*)iter->delete_item(iter);

        if(!reloc_rec) {
            error = true;
            break;
        }

        char_t* sym_name = NULL;

        reloc_rec->get_string(reloc_rec, "symbol_name", &sym_name);

        if(!sym_name || strlen(sym_name) == 0) {
            print_error("cannot get symbol name of reloc record");
            error = true;
            reloc_rec->destroy(reloc_rec);

            break;
        }

        int64_t reloc_id = 0;
        int64_t sec_id = 0;
        int64_t sym_id = 0;

        if(!reloc_rec->get_int64(reloc_rec, "id", &reloc_id)) {
            print_error("cannot get reloc id");
            error = true;
            memory_free(sym_name);
            reloc_rec->destroy(reloc_rec);

            break;
        }

        if(!reloc_rec->get_int64(reloc_rec, "symbol_id", &sym_id)) {
            print_error("cannot get symbol id");
            error = true;
            memory_free(sym_name);
            reloc_rec->destroy(reloc_rec);

            break;
        }

        if(!reloc_rec->get_int64(reloc_rec, "symbol_section_id", &sec_id)) {
            print_error("cannot get symbol section id");
            error = true;
            memory_free(sym_name);
            reloc_rec->destroy(reloc_rec);

            break;
        }

        if(sec_id != 0 && sym_id != 0) { // already fixed
            PRINTLOG(LINKER, LOG_WARNING, "reloc record already fixed: %s", sym_name);
            memory_free(sym_name);
            reloc_rec->destroy(reloc_rec);
            need_fix_count--;

            iter = iter->next(iter);

            continue;
        }


        tosdb_record_t* s_sym_rec = tosdb_table_create_record(tbl_symbols);

        if(!s_sym_rec) {
            error = true;
            memory_free(sym_name);
            reloc_rec->destroy(reloc_rec);

            break;
        }

        s_sym_rec->set_string(s_sym_rec, "name", sym_name);

        list_t* s_sym_recs = s_sym_rec->search_record(s_sym_rec);

        if(!s_sym_recs) {
            error = true;
            memory_free(sym_name);
            reloc_rec->destroy(reloc_rec);

            break;
        }

        if(list_size(s_sym_recs) == 0) {
            if(!set_append(set_nf, sym_name)) {
                memory_free(sym_name);
            }

            nf_count++;

            list_destroy(s_sym_recs);
            s_sym_rec->destroy(s_sym_rec);
            reloc_rec->destroy(reloc_rec);
            iter = iter->next(iter);

            continue;
        }

        if(list_size(s_sym_recs) > 1) {

            dup_count++;

            iterator_t* iter_dup = list_iterator_create(s_sym_recs);

            while(iter_dup->end_of_iterator(iter_dup) != 0) {
                tosdb_record_t* dup_rec = (tosdb_record_t*)iter_dup->get_item(iter_dup);

                int64_t dup_sym_id = 0;

                if(dup_rec->get_int64(dup_rec, "id", &dup_sym_id)) {
                    PRINTLOG(LINKER, LOG_WARNING, "duplicate symbol record: %s, id: %llx deleted? %i", sym_name, dup_sym_id, dup_rec->is_deleted(dup_rec));
                }

                dup_rec->destroy(dup_rec);

                iter_dup = iter_dup->next(iter_dup);
            }

            iter_dup->destroy(iter_dup);

            if(!set_append(set_dup, sym_name)) {
                memory_free(sym_name);
            }

            list_destroy(s_sym_recs);
            s_sym_rec->destroy(s_sym_rec);
            reloc_rec->destroy(reloc_rec);

            continue;
        }

        s_sym_rec->destroy(s_sym_rec);

        s_sym_rec = (tosdb_record_t*)list_delete_at_tail(s_sym_recs);

        list_destroy(s_sym_recs);

        if(!s_sym_rec->get_int64(s_sym_rec, "section_id", (int64_t*)&sec_id)) {
            s_sym_rec->destroy(s_sym_rec);
            error = true;
            reloc_rec->destroy(reloc_rec);

            break;
        }

        if(!s_sym_rec->get_int64(s_sym_rec, "id", (int64_t*)&sym_id)) {
            s_sym_rec->destroy(s_sym_rec);
            error = true;
            reloc_rec->destroy(reloc_rec);

            break;
        }

        s_sym_rec->destroy(s_sym_rec);

        if(sym_id == 0 || sec_id == 0) {
            PRINTLOG(LINKER, LOG_ERROR, "invalid symbol id or section id for symbol %s, sym_id: %llx, sec_id: %llx", sym_name, sym_id, sec_id);
            error = true;
            memory_free(sym_name);
            reloc_rec->destroy(reloc_rec);

            break;
        }

        if(!reloc_rec->set_int64(reloc_rec, "symbol_section_id", sec_id)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot set symbol section id for symbol %s", sym_name);
            error = true;
            memory_free(sym_name);
            reloc_rec->destroy(reloc_rec);

            break;
        }

        if(!reloc_rec->set_int64(reloc_rec, "symbol_id", sym_id)) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot set symbol id for symbol %s", sym_name);
            error = true;
            memory_free(sym_name);
            reloc_rec->destroy(reloc_rec);

            break;
        }

        boolean_t res = reloc_rec->upsert_record(reloc_rec);
        reloc_rec->destroy(reloc_rec);

        if(!res) {
            error = true;
            memory_free(sym_name);

            break;
        }

        PRINTLOG(LINKER, LOG_DEBUG, "relocation 0x%llx fixed for symbol %s new section id %llx symbol id %llx", reloc_id, sym_name, sec_id, sym_id);

        memory_free(sym_name);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    list_destroy(res_recs);
    s_recs_need->destroy(s_recs_need);

    int64_t fixed_count = need_fix_count - nf_count - dup_count;

    if(fixed_count) {
        PRINTLOG(LINKER, LOG_INFO, "fixed symbol count: %lli", fixed_count);
    }

    if(nf_count) {
        PRINTLOG(LINKER, LOG_WARNING, "not found total symbol count: %lli", nf_count);

        iter = set_create_iterator(set_nf);

        while(iter->end_of_iterator(iter) != 0) {
            char_t* sym_name = (char_t*)iter->get_item(iter);

            PRINTLOG(LINKER, LOG_WARNING, "cannot find symbol %s", sym_name);

            memory_free(sym_name);

            iter = iter->next(iter);
        }

        iter->destroy(iter);
    }

    set_destroy(set_nf);

    if(dup_count) {
        PRINTLOG(LINKER, LOG_WARNING, "duplicated symbol count: %lli", dup_count);

        iter = set_create_iterator(set_dup);

        while(iter->end_of_iterator(iter) != 0) {
            char_t* sym_name = (char_t*)iter->get_item(iter);

            PRINTLOG(LINKER, LOG_WARNING, "duplicated symbol %s", sym_name);

            memory_free(sym_name);

            iter = iter->next(iter);
        }

        iter->destroy(iter);
    }

    set_destroy(set_dup);

    return !error;
}

int32_t main(int32_t argc, char_t** argv) {

    if(argc <= 1) {
        print_error("not enough params");
        return -1;
    }

    argc--;
    argv++;

    char_t* output_file = NULL;
    const char_t* entry_point = "___kstart64";
    boolean_t need_free_entry_point = false;
    uint64_t stack_size = 0x10000;
    uint64_t program_base = 0x200000; // 2MB

    while(argc > 0) {
        if(strstarts(*argv, "-") != 0) {
            break;
        }


        if(strcmp(*argv, "-o") == 0) {
            argc--;
            argv++;

            if(argc) {
                output_file = *argv;

                argc--;
                argv++;
                continue;
            } else {
                print_error("argument error");
                print_error("LINKERDB FAILED");

                return -1;
            }
        }

        if(strcmp(*argv, "-e") == 0) {
            argc--;
            argv++;

            if(argc) {
                entry_point = strdup(*argv);
                need_free_entry_point = true;

                argc--;
                argv++;
                continue;
            } else {
                print_error("argument error");
                print_error("LINKERDB FAILED");

                return -1;
            }
        }

        if(strcmp(*argv, "-ss") == 0) {
            argc--;
            argv++;

            if(argc) {
                stack_size = atoi(*argv);

                argc--;
                argv++;
                continue;
            } else {
                print_error("argument error");
                print_error("LINKERDB FAILED");

                return -1;
            }
        }

        if(strcmp(*argv, "-pb") == 0) {
            argc--;
            argv++;

            if(argc) {
                program_base = atoi(*argv);

                argc--;
                argv++;
                continue;
            } else {
                print_error("argument error");
                print_error("LINKERDB FAILED");

                return -1;
            }
        }
    }

    int32_t exit_code = 0;

    linkerdb_t* ldb = linkerdb_open(output_file, LINKERDB_CAP);

    if(!ldb) {
        print_error("cannot open linkerdb");
        print_error("LINKERDB FAILED");

        exit_code = -1;
        goto close;
    }

    if(ldb->new_file && !linkerdb_gen_config(ldb, entry_point, stack_size, program_base)) {
        print_error("cannot gen config table");

        exit_code = -1;
        goto close;
    }

    if(ldb->new_file && !linkerdb_create_tables(ldb)) {
        print_error("cannot create tables");

        exit_code = -1;
        goto close;
    }

    linkerdb_stats_t is = {0};

    printf("%lli\n", time_ns(NULL));

    while(argc) {
        if(!linkerdb_parse_object_file(ldb, *argv, &is)) {
            print_error("cannot parse object file");

            exit_code = -1;
            goto close;
        }

        argc--;
        argv++;
    }

    printf("added\n\tmodules: %lli\n\timplementations: %lli\n\tsections: %lli\n\tsymbols: %lli\n\trelocations: %lli\n",
           is.nm_count, is.implementation_count, is.sec_count, is.sym_count, is.reloc_count);

    printf("%lli\n", time_ns(NULL));

    if(!linkerdb_fix_reloc_symbol_section_ids(ldb)) {
        print_error("cannot fix relocations missing symbol sections");
        exit_code = -1;

        goto close;
    }

    printf("%lli\n", time_ns(NULL));

#if 1
    tosdb_database_t* db_system = tosdb_database_create_or_open(ldb->tdb, "system");
    tosdb_table_t* tbl_relocations = tosdb_table_create_or_open(db_system, "relocations", 8 << 10, 1 << 20, 8);

    tosdb_table_close(tbl_relocations);

    tbl_relocations = tosdb_table_create_or_open(db_system, "relocations", 8 << 10, 1 << 20, 8);

    if(!tosdb_compact(ldb->tdb, TOSDB_COMPACTION_TYPE_MAJOR)) {
        print_error("cannot compact linker db");
    }
#endif

close:
    if(!linkerdb_close(ldb)) {
        print_error("cannot close linkerdb");
        print_error("LINKERDB FAILED");

        exit_code = -1;
    }

    if(need_free_entry_point) {
        memory_free((void*)entry_point);
    }

    printf("%lli\n", time_ns(NULL));

    return exit_code;

}

