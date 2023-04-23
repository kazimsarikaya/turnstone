/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___DISK_H
#define ___DISK_H 0

#include <types.h>
#include <iterator.h>

typedef void * disk_context_t;
typedef struct disk_partition_context_t {
    void*    internal_context;
    uint64_t start_lba;
    uint64_t end_lba;
}disk_partition_context_t;

typedef struct disk_t              disk_t;
typedef struct disk_partition_t    disk_partition_t;
typedef struct disk_or_partition_t disk_or_partition_t;

typedef uint64_t (*disk_or_partition_get_size_f)(const disk_or_partition_t* dp);
typedef uint64_t (*disk_or_partition_get_block_size_f)(const disk_or_partition_t* dp);
typedef int8_t   (*disk_or_partition_write_f)(const disk_or_partition_t* dp, uint64_t lba, uint64_t count, uint8_t* data);
typedef int8_t   (*disk_or_partition_read_f)(const disk_or_partition_t* dp, uint64_t lba, uint64_t count, uint8_t** data);
typedef int8_t   (*disk_or_partition_flush_f)(const disk_or_partition_t* dp);
typedef int8_t   (*disk_or_partition_close_f)(const disk_or_partition_t* dp);

struct disk_or_partition_t {
    disk_context_t                     context;
    disk_or_partition_get_size_f       get_size;
    disk_or_partition_get_block_size_f get_block_size;
    disk_or_partition_write_f          write;
    disk_or_partition_read_f           read;
    disk_or_partition_flush_f          flush;
    disk_or_partition_close_f          close;
};

struct disk_partition_t {
    disk_or_partition_t             partition;
    const disk_partition_context_t* (*get_context)(const disk_partition_t* p);
    const disk_t*                   (*get_disk)(const disk_partition_t* p);
};

struct disk_t {
    disk_or_partition_t       disk;
    uint8_t                   (* add_partition)(const disk_t* d, disk_partition_context_t* part_ctx);
    int8_t                    (* del_partition)(const disk_t* d, uint8_t partno);
    disk_partition_context_t* (* get_partition_context)(const disk_t* d, uint8_t partno);
    iterator_t*               (* get_partition_contexts)(const disk_t* d);
    disk_partition_t*         (* get_partition)(const disk_t* d, uint8_t partno);
};

#endif
