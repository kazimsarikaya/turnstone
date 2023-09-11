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

//utils programs need dep headers for linking
#include <utils.h>
#include <set.h>
#include <hashmap.h>
#include <buffer.h>
#include <linkedlist.h>
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


linkerdb_t* linkerdb_open(const char_t* file) {
    FILE* fp = fopen(file, "r+");

    if(!fp) {
        print_error("cannot open db file");

        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    uint64_t capacity = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    int32_t fd = fileno(fp);

    uint8_t* mmap_res = mmap(NULL, capacity, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_SYNC, fd, 0);

    if(mmap_res == (void*)-1) {
        fclose(fp);
        print_error("cannot mmap db file");

        return NULL;
    }

    buffer_t buf = buffer_encapsulate(mmap_res, capacity);

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

    tosdb_t* tdb = tosdb_new(bend);

    if(!tdb) {
        buffer_set_readonly(buf, true);
        tosdb_backend_close(bend);
        munmap(mmap_res, capacity);
        fclose(fp);
        print_error("cannot create backend");

        return NULL;
    }

    tosdb_cache_config_t cc = {0};
    cc.bloomfilter_size = 2 << 20;
    cc.index_data_size = 4 << 20;
    cc.secondary_index_data_size = 4 << 20;
    cc.valuelog_size = 16 << 20;

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
        printf("msync res %i\n", mss);
        print_error("cannot sync contents");
    }

    munmap(ldb->mmap_res, ldb->capacity);

    fclose(ldb->db_file);

    memory_free(ldb);

    return true;
}


int32_t main(int32_t argc, char_t** argv) {

    if(argc <= 1) {
        print_error("not enough params");
        return -1;
    }

    argc--;
    argv++;

    char_t* db_file = NULL;
    char_t* entrypoint_symbol = NULL;
    uint64_t program_start = 0;

    while(argc > 0) {
        if(strstarts(*argv, "-") != 0) {
            print_error("argument error");
            print_error("LINKERDB FAILED");

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
                print_error("argument error");
                print_error("LINKERDB FAILED");

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
                print_error("argument error");
                print_error("LINKERDB FAILED");

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
                print_error("argument error");
                print_error("LINKERDB FAILED");

                return -1;
            }
        }

    }


    if(db_file == NULL) {
        print_error("no db file specified");
        print_error("LINKERDB FAILED");

        return -1;
    }

    if(entrypoint_symbol == NULL) {
        print_error("no entrypoint specified");
        print_error("LINKERDB FAILED");

        return -1;
    }

    int32_t exit_code = 0;

    linkerdb_t* ldb = linkerdb_open(db_file);

    if(!ldb) {
        print_error("cannot open linkerdb");
        print_error("LINKERDB FAILED");

        exit_code = -1;
        goto exit;
    }

    tosdb_database_t* db_system = tosdb_database_create_or_open(ldb->tdb, "system");

    if(!db_system) {
        print_error("cannot open system database");
        print_error("LINKERDB FAILED");

        exit_code = -1;
        goto exit;
    }

    tosdb_table_t* tbl_sections = tosdb_table_create_or_open(db_system, "sections", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_modules = tosdb_table_create_or_open(db_system, "modules", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_symbols = tosdb_table_create_or_open(db_system, "symbols", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_relocations = tosdb_table_create_or_open(db_system, "relocations", 1 << 10, 512 << 10, 8);


    if(!tbl_sections || !tbl_modules || !tbl_symbols || !tbl_relocations) {
        print_error("cannot open system tables");
        print_error("LINKERDB FAILED");

        exit_code = -1;
        goto exit;
    }

    tosdb_record_t* s_sym_rec = tosdb_table_create_record(tbl_symbols);

    if(!s_sym_rec) {
        print_error("cannot create record for search");
        print_error("LINKERDB FAILED");

        exit_code = -1;
        goto exit;
    }

    s_sym_rec->set_string(s_sym_rec, "name", entrypoint_symbol);

    if(!s_sym_rec->get_record(s_sym_rec)) {
        print_error("cannot get record for search");
        print_error("LINKERDB FAILED");

        exit_code = -1;
        goto exit;
    }

    uint64_t sec_id = 0;

    if(!s_sym_rec->get_int64(s_sym_rec, "section_id", (int64_t*)&sec_id)) {
        print_error("cannot get section id for entrypoint symbol");
        print_error("LINKERDB FAILED");

        s_sym_rec->destroy(s_sym_rec);

        exit_code = -1;
        goto exit;
    }

    uint64_t sym_id = 0;

    if(!s_sym_rec->get_int64(s_sym_rec, "id", (int64_t*)&sym_id)) {
        print_error("cannot get symbol id for entrypoint symbol");
        print_error("LINKERDB FAILED");

        s_sym_rec->destroy(s_sym_rec);

        exit_code = -1;
        goto exit;
    }

    s_sym_rec->destroy(s_sym_rec);


    printf("entrypoint symbol %s id 0x%llx section id: 0x%llx\n", entrypoint_symbol, sym_id, sec_id);


    tosdb_record_t* s_sec_rec = tosdb_table_create_record(tbl_sections);

    if(!s_sec_rec) {
        print_error("cannot create record for search");
        print_error("LINKERDB FAILED");

        exit_code = -1;
        goto exit;
    }

    s_sec_rec->set_int64(s_sec_rec, "id", sec_id);

    if(!s_sec_rec->get_record(s_sec_rec)) {
        print_error("cannot get record for search");
        print_error("LINKERDB FAILED");

        exit_code = -1;
        goto exit;
    }

    uint64_t mod_id = 0;

    if(!s_sec_rec->get_int64(s_sec_rec, "module_id", (int64_t*)&mod_id)) {
        print_error("cannot get module id for entrypoint symbol");
        print_error("LINKERDB FAILED");

        s_sec_rec->destroy(s_sec_rec);

        exit_code = -1;
        goto exit;
    }

    s_sec_rec->destroy(s_sec_rec);

    printf("module id: 0x%llx\n", mod_id);


exit:
    linkerdb_close(ldb);
    UNUSED(program_start);

    return exit_code;
}
