/**
 * @file tosdb_internal.h
 * @brief tosdb backend interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___TOSDB_TOSDB_BACKEND_H
#define ___TOSDB_TOSDB_BACKEND_H 0

#include <types.h>
#include <tosdb/tosdb.h>

typedef enum tosdb_backend_type_t {
    TOSDB_BACKEND_TYPE_NONE,
    TOSDB_BACKEND_TYPE_MEMORY,
    TOSDB_BACKEND_TYPE_DISK,
}tosdb_backend_type_t;

typedef uint8_t   * (*tosdb_backend_read_f)(tosdb_backend_t* backend, uint64_t position, uint64_t size);
typedef uint64_t  (*tosdb_backend_write_f)(tosdb_backend_t* backend, uint64_t position, uint64_t size, uint8_t* data);
typedef boolean_t (*tosdb_backend_flush_f)(tosdb_backend_t* backend);

struct tosdb_backend_t {
    void*                 context;
    tosdb_backend_type_t  type;
    uint64_t              capacity;
    tosdb_backend_read_f  read;
    tosdb_backend_write_f write;
    tosdb_backend_flush_f flush;
};

boolean_t tosdb_backend_memory_close(tosdb_backend_t* backend);
boolean_t tosdb_backend_disk_close(tosdb_backend_t* backend);

typedef struct tosdb_superblock_t tosdb_superblock_t;

tosdb_superblock_t* tosdb_backend_repair(tosdb_backend_t* backend);
tosdb_superblock_t* tosdb_backend_format(tosdb_backend_t* backend, compression_type_t compression_type_if_not_exists);


#endif
