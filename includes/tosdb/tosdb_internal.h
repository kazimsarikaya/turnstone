/**
 * @file tosdb_internal.h
 * @brief tosdb internal interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___TOSDB_TOSDB_INTERNAL_H
#define ___TOSDB_TOSDB_INTERNAL_H 0

#include <tosdb/tosdb.h>
#include <future.h>


#define TOSDB_PAGE_SIZE 4096
#define TOSDB_SUPERBLOCK_SIGNATURE "TURNSTONE OS DB\0"
#define TOSDB_VERSION_MAJOR 0
#define TOSDB_VERSION_MINOR 1



typedef enum tosdb_backend_type_t {
    TOSDB_BACKEND_TYPE_NONE,
    TOSDB_BACKEND_TYPE_MEMORY,
}tosdb_backend_type_t;

typedef future_t (*tosdb_backend_read_f)(tosdb_backend_t* backend, uint64_t position, uint64_t size);
typedef future_t (*tosdb_backend_write_f)(tosdb_backend_t* backend, uint64_t position, uint64_t size, uint8_t* data);
typedef future_t (*tosdb_backend_flush_f)(tosdb_backend_t* backend);

struct tosdb_backend_t {
    void*                 context;
    tosdb_backend_type_t  type;
    uint64_t              capacity;
    tosdb_backend_read_f  read;
    tosdb_backend_write_f write;
    tosdb_backend_flush_f flush;
};

typedef enum tosdb_block_type_t {
    TOSDB_BLOCK_TYPE_NONE,
    TOSDB_BLOCK_TYPE_SUPERBLOCK,
} tosdb_block_type_t;

typedef struct tosdb_superblock_t {
    char_t             signature[16];
    uint64_t           checksum;
    tosdb_block_type_t block_type;
    uint64_t           block_size;
    uint32_t           version_major;
    uint32_t           version_minor;
    uint32_t           page_size;
    char_t             reservedN[TOSDB_PAGE_SIZE - 48];
}__attribute__((packed)) tosdb_superblock_t;

_Static_assert(sizeof(tosdb_superblock_t) == TOSDB_PAGE_SIZE, "super block size mismatch");

struct tosdb_t {
    tosdb_backend_t*    backend;
    tosdb_superblock_t* superblock;
};

boolean_t tosdb_backend_memory_close(tosdb_backend_t* backend);

tosdb_superblock_t* tosdb_backend_repair(tosdb_backend_t* backend);
tosdb_superblock_t* tosdb_backend_format(tosdb_backend_t* backend);
boolean_t           tosdb_write_and_flush_superblock(tosdb_backend_t* backend, tosdb_superblock_t* sb);


#endif
