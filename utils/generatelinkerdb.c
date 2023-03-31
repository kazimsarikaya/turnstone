/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#define RAMSIZE 0x8000000
#include "setup.h"
#include <utils.h>
#include <buffer.h>
#include <data.h>
#include <sha2.h>

typedef union tosdb_cluster_header_t {
    struct tosdb_cluster_header_fields_t {
        uint8_t  hash[SHA256_OUTPUT_SIZE];     ///< 32 byte
        uint32_t version_minor; ///< 4byte
        uint32_t version_major; ///< 4 byte
        uint64_t database_id; ///< 8 byte
        uint64_t table_id; ///< 8 byte 

    }__attribute__((packed)) fields;
    uint8_t header_bytes[256];
}__attribute__((packed)) tosdb_cluster_header_t;

typedef struct tosdb_supercluster_data_t {
    char_t   signature[16];  ///< 16 byte
    uint64_t disk_size;  ///< 8 byte
    uint64_t cluser_size;  ///< 8 byte
}__attribute__((packed)) tosdb_supercluster_data_t;


typedef struct tosdb_cluster_t {
    tosdb_cluster_header_t header;
    uint8_t                data[4096 - 256];
}__attribute__((packed)) tosdb_cluster_t;

int32_t main(int32_t argc, char_t** argv) {
    if(argc != 2) {
        print_error("incorrect parameter.");
        printf("usage: %s <dbfile>\n\n", argv[0]);

        return -1;
    }

    char_t* dbfile = argv[1];

    FILE * fp_db;

    fp_db = fopen(dbfile, "w");
    uint8_t data = 0;
    fseek(fp_db, (1 << 30) - 1, SEEK_SET);
    fwrite(&data, 1, 1, fp_db);
    fclose(fp_db);

    fp_db = fopen(dbfile, "r+");


    fclose(fp_db);

    return 0;
}
