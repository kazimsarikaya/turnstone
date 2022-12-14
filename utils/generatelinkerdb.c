/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#define RAMSIZE 0x8000000
#include "setup.h"
#include <sha2.h>
#include <strings.h>
#include <utils.h>

#define DATABASE_CLUSTER_SIZE 4096

#define DATABASE_FIRST_CLUSTER_MAGIC 0xF3E35CFB028D8B18ULL

typedef struct cluster_header_s {
    uint8_t  checksum[SHA256_OUTPUT_SIZE];
    uint64_t version;
    uint64_t old_version_address;
    uint64_t previous_cluster;
    uint64_t data_size;
} __attribute__((packed)) cluster_header_t;

typedef struct cluster_generic_s {
    cluster_header_t header;
    uint8_t          data[DATABASE_CLUSTER_SIZE - sizeof(cluster_header_t)];
} __attribute__((packed)) cluster_generic_t;

typedef struct cluster_first_s {
    cluster_header_t header;
    uint64_t         magic;
    uint64_t         cluster_count;
    uint64_t         self_address;
    uint64_t         backup_address;
    uint64_t         database_entries_address;
} __attribute__((packed)) cluster_first_t;

typedef struct database_strings_entry_s {
    uint64_t id;
    uint64_t length;
    char_t   string[];
} __attribute__((packed)) database_strings_entry_t;

typedef struct database_entry_s {
    uint64_t id;
    uint64_t name;
    uint64_t owner_id;
    uint64_t group_id;
    uint64_t tables_address;
    uint64_t strings_index_address;
}__attribute__((packed)) database_entry_t;

typedef struct database_entries_s {
    cluster_header_t header;
    database_entry_t entries[];
}__attribute__((packed)) database_entries_t;

int32_t main(int32_t argc, char** argv) {

    if(argc != 3) {
        print_error("argument cound mismatch");

        return -1;
    }

    argv++;

    char_t* db_fn = *argv;

    argv++;

    uint64_t db_size = atoi(*argv);

    if(db_size <= 0) {
        print_error("db size error");

        return -1;
    }

    uint64_t cluster_count = db_size / DATABASE_CLUSTER_SIZE;

    FILE* db_file = fopen(db_fn, "w");

    if(db_fn == NULL) {
        print_error("cannot open db file");

        return -1;
    }

    printf("db %s size %li\n", db_fn, db_size);

    fseek(db_file, db_size - 1, SEEK_SET);

    uint8_t zero = 0;
    fwrite(&zero, 1, 1, db_file);

    cluster_first_t* fc = memory_malloc(sizeof(cluster_generic_t));
    fc->header.version = 0;
    fc->header.old_version_address = 0;
    fc->header.previous_cluster = 0;
    fc->header.data_size = sizeof(cluster_first_t);
    fc->magic = DATABASE_FIRST_CLUSTER_MAGIC;
    fc->cluster_count = cluster_count;
    fc->self_address = 0;
    fc->backup_address = cluster_count - 1;

    uint8_t* hash = sha256_hash((uint8_t*)fc, sizeof(cluster_generic_t));
    memory_memcopy(hash, fc->header.checksum, SHA256_OUTPUT_SIZE);
    memory_free(hash);

    fseek(db_file, 0, SEEK_SET);
    fwrite(fc, 1, sizeof(cluster_generic_t), db_file);

    memory_memclean(fc->header.checksum, SHA256_OUTPUT_SIZE);

    fc->self_address = cluster_count - 1;
    fc->backup_address = 0;

    hash = sha256_hash((uint8_t*)fc, sizeof(cluster_generic_t));
    memory_memcopy(hash, fc->header.checksum, SHA256_OUTPUT_SIZE);
    memory_free(hash);

    fseek(db_file, (cluster_count - 1) * DATABASE_CLUSTER_SIZE, SEEK_SET);
    fwrite(fc, 1, sizeof(cluster_generic_t), db_file);

    memory_free(fc);


    fclose(db_file);


    print_success("OK");
    return 0;
}
