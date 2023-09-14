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

typedef struct linkerdb_t {
    uint64_t         capacity;
    FILE*            db_file;
    int32_t          fd;
    uint8_t*         mmap_res;
    buffer_t         backend_buffer;
    tosdb_backend_t* backend;
    tosdb_t*         tdb;
} linkerdb_t;


int32_t     main(int32_t argc, char_t** args);
linkerdb_t* linkerdb_open(const char_t* file);
boolean_t   linkerdb_close(linkerdb_t* ldb);
int8_t      linker_print_context(linker_context_t* ctx);

int8_t linker_print_context(linker_context_t* ctx) {
    if(!ctx) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot print linker context, context is null");

        return -1;
    }

    printf("program start physical 0x%llx virtual 0x%llx\n", ctx->program_start_physical, ctx->program_start_virtual);
    printf("entrypoint symbol id: 0x%llx\n\n", ctx->entrypoint_symbol_id);
    printf("modules count: %llu\n", hashmap_size(ctx->modules));
    printf("modules:\n\n");

    iterator_t* it = hashmap_iterator_create(ctx->modules);

    if(!it) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot create iterator for modules");

        return -1;
    }

    while(it->end_of_iterator(it) != 0) {
        const linker_module_t* module = it->get_item(it);

        printf("module id: 0x%llx physical start: 0x%llx virtual start: 0x%llx\n", module->id, module->physical_start, module->virtual_start);
        printf("sections:\n\n");

        printf("section type  virtual start physical start       size\n");
        printf("------------ -------------- -------------- ----------\n");

        for(uint64_t i = 0; i < LINKER_SECTION_TYPE_NR_SECTIONS; i++) {
            if(module->sections[i].size == 0) {
                continue;
            }

            printf("% 12lli 0x%012llx 0x%012llx 0x%08llx\n",
                   i, module->sections[i].virtual_start,
                   module->sections[i].physical_start, module->sections[i].size);
        }

        printf("\nrelocations:\n");

        uint64_t relocation_size = buffer_get_length(module->sections[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_data);
        uint64_t relocation_count = relocation_size / sizeof(linker_relocation_entry_t);
        linker_relocation_entry_t* relocations = (linker_relocation_entry_t*)buffer_get_view_at_position(module->sections[LINKER_SECTION_TYPE_RELOCATION_TABLE].section_data, 0, relocation_size);

        printf("section type relocation type      symbol id         offset         addend\n");
        printf("------------ --------------- -------------- -------------- --------------\n");

        for(uint64_t i = 0; i < relocation_count; i++) {
            printf("%012i %015i 0x%012llx 0x%012llx 0x%012llx\n",
                   relocations[i].section_type, relocations[i].relocation_type,
                   relocations[i].symbol_id, relocations[i].offset,
                   relocations[i].addend);
        }

        printf("\n\n");

        it = it->next(it);
    }

    it->destroy(it);

    uint64_t got_entry_count = hashmap_size(ctx->got_symbol_index_map) + 2;
    printf("GOT table entry count: %llu\n", got_entry_count);
    printf("GOT table:\n\n");
    uint64_t got_table_size = buffer_get_length(ctx->got_table_buffer);
    linker_global_offset_table_entry_t* got_table = (linker_global_offset_table_entry_t*)buffer_get_view_at_position(ctx->got_table_buffer, 0, got_table_size);

    uint64_t unresolved_symbol_count = 0;

    printf("     module id      symbol id section type    entry value resolved symbol value symbol type symbol scope symbol size\n");
    printf("-------------- -------------- ------------ -------------- -------- ------------ ----------- ------------ -----------\n");
    for(uint64_t i = 0; i < got_entry_count; i++) {
        printf("0x%012llx 0x%012llx            %i 0x%012llx        %i 0x%010llx % 11i % 12i 0x%09llx\n",
               got_table[i].module_id, got_table[i].symbol_id,
               got_table[i].section_type, got_table[i].entry_value,
               got_table[i].resolved, got_table[i].symbol_value,
               got_table[i].symbol_type, got_table[i].symbol_scope,
               got_table[i].symbol_size);

        if(got_table[i].resolved == 0) {
            unresolved_symbol_count++;
        }
    }

    printf("\n\n");

    if(unresolved_symbol_count != 2) {
        print_error("unresolved symbol count is not 2 (got itself and null symbol): %llu", unresolved_symbol_count);
    } else {
        print_success("all symbols resolved");
    }

    printf("\n\n");

    return 0;
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

    argc--;
    argv++;

    char_t* db_file = NULL;
    char_t* entrypoint_symbol = NULL;
    uint64_t program_start_physical = 0;
    uint64_t program_start_virtual = 0;
    boolean_t recursive = false;
    boolean_t for_efi = false;
    boolean_t print_context = false;
    char_t* output_file = NULL;

    while(argc > 0) {
        if(strstarts(*argv, "-") != 0) {
            PRINTLOG(LINKER, LOG_ERROR, "argument error");
            PRINTLOG(LINKER, LOG_ERROR, "LINKERDB FAILED");

            return -1;
        } else if(strcmp(*argv, "-psp") == 0 || strcmp(*argv, "--program-start-physical") == 0) {
            argc--;
            argv++;

            if(argc) {
                program_start_physical = atoi(*argv);

                argc--;
                argv++;
                continue;
            } else {
                PRINTLOG(LINKER, LOG_ERROR, "argument error");
                PRINTLOG(LINKER, LOG_ERROR, "LINKERDB FAILED");

                return -1;
            }
        } else if(strcmp(*argv, "-psv") == 0 || strcmp(*argv, "--program-start-virtual") == 0) {
            argc--;
            argv++;

            if(argc) {
                program_start_virtual = atoi(*argv);

                argc--;
                argv++;
                continue;
            } else {
                PRINTLOG(LINKER, LOG_ERROR, "argument error");
                PRINTLOG(LINKER, LOG_ERROR, "LINKERDB FAILED");

                return -1;
            }
        } else if(strcmp(*argv, "-r") == 0 || strcmp(*argv, "--recursive") == 0) {
            argc--;
            argv++;

            recursive = true;
        } else if(strcmp(*argv, "-d") == 0) {
            argc--;
            argv++;

            logging_module_levels[LINKER] = LOG_DEBUG;
        } else if(strcmp(*argv, "-v") == 0) {
            argc--;
            argv++;

            logging_module_levels[LINKER] = LOG_VERBOSE;
        } else if(strcmp(*argv, "--for-efi") == 0) {
            argc--;
            argv++;

            for_efi = true;
        } else if(strcmp(*argv, "--print") == 0) {
            argc--;
            argv++;

            print_context = true;
        } else if(strcmp(*argv, "-e") == 0 || strcmp(*argv, "--entrypoint") == 0) {
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
        } else if(strcmp(*argv, "-db") == 0 || strcmp(*argv, "--db-file") == 0) {
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
        } else if(strcmp(*argv, "-o") == 0 || strcmp(*argv, "--output-file") == 0) {
            argc--;
            argv++;

            if(argc) {
                output_file = *argv;

                argc--;
                argv++;
                continue;
            } else {
                PRINTLOG(LINKER, LOG_ERROR, "argument error");
                PRINTLOG(LINKER, LOG_ERROR, "LINKERDB FAILED");

                return -1;
            }
        } else {
            PRINTLOG(LINKER, LOG_ERROR, "argument error");
            PRINTLOG(LINKER, LOG_ERROR, "LINKERDB FAILED");

            return -1;
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

    if(for_efi && output_file == NULL) {
        PRINTLOG(LINKER, LOG_ERROR, "no output file specified");
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
        linkedlist_destroy(found_symbols);

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

    PRINTLOG(LINKER, LOG_INFO, "module id: 0x%llx", mod_id);

    linker_context_t* ctx = memory_malloc(sizeof(linker_context_t));

    if(!ctx) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot allocate linker context");

        exit_code = -1;
        goto exit;
    }

    ctx->entrypoint_symbol_id = sym_id;
    ctx->program_start_physical = program_start_physical;
    ctx->program_start_virtual = program_start_virtual;
    ctx->tdb = ldb->tdb;
    ctx->modules = hashmap_integer(16);
    ctx->got_table_buffer = buffer_new();
    ctx->got_symbol_index_map = hashmap_integer(1024);

    linker_global_offset_table_entry_t empty_got_entry = {0};

    buffer_append_bytes(ctx->got_table_buffer, (uint8_t*)&empty_got_entry, sizeof(linker_global_offset_table_entry_t)); // null entry
    buffer_append_bytes(ctx->got_table_buffer, (uint8_t*)&empty_got_entry, sizeof(linker_global_offset_table_entry_t)); // got itself

    if(linker_build_module(ctx, mod_id, recursive) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot build module");

        exit_code = -1;
        goto exit_with_destroy_context;
    }

    PRINTLOG(LINKER, LOG_INFO, "modules built");


    if(linker_bind_addresses(ctx) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot bind addresses");

        exit_code = -1;
        goto exit_with_destroy_context;
    }


    if(linker_bind_got_entry_values(ctx) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot bind got entry values");

        exit_code = -1;
        goto exit_with_destroy_context;
    }

    if(linker_link_program(ctx) != 0) {
        PRINTLOG(LINKER, LOG_ERROR, "cannot link program");

        exit_code = -1;
        goto exit_with_destroy_context;
    }

    if(for_efi) {
        buffer_t efi_program = linker_build_efi(ctx);

        if(!efi_program) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot build efi program");

            exit_code = -1;
            goto exit_with_destroy_context;
        }

        FILE* out = fopen(output_file, "w");

        if(!out) {
            PRINTLOG(LINKER, LOG_ERROR, "cannot open output file %s", output_file);
            buffer_destroy(efi_program);

            exit_code = -1;
            goto exit_with_destroy_context;
        }

        uint64_t efi_program_size = buffer_get_length(efi_program);

        uint8_t* efi_program_data = buffer_get_view_at_position(efi_program, 0, efi_program_size);

        fwrite(efi_program_data, 1, efi_program_size, out);

        fclose(out);

        buffer_destroy(efi_program);
    }


    if(print_context) {
        linker_print_context(ctx);
    }

exit_with_destroy_context:
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
