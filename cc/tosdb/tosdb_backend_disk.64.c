/**
 * @file tosdb_backend_disk.64.c
 * @brief tosdb memory backend implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <tosdb/tosdb_backend.h>
#include <logging.h>
#include <logging.h>

MODULE("turnstone.kernel.db");


typedef struct tosdb_backend_disk_ctx_t {
    disk_or_partition_t* dp;
} tosdb_backend_disk_ctx_t;

uint8_t*  tosdb_backend_disk_read(tosdb_backend_t* backend, uint64_t position, uint64_t size);
uint64_t  tosdb_backend_disk_write(tosdb_backend_t* backend, uint64_t position, uint64_t size, uint8_t* data);
boolean_t tosdb_backend_disk_flush(tosdb_backend_t* backend);


tosdb_backend_t* tosdb_backend_disk_new(disk_or_partition_t* dp) {
    if(!dp) {
        return NULL;
    }

    memory_heap_t* heap = dp->get_heap(dp);

    tosdb_backend_disk_ctx_t* ctx = memory_malloc_ext(heap, sizeof(tosdb_backend_disk_ctx_t), 0);

    if(!ctx) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create backend context");

        return NULL;
    }

    ctx->dp = dp;

    tosdb_backend_t* backend = memory_malloc(sizeof(tosdb_backend_t));

    if(!backend) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create backend");
        memory_free_ext(heap, ctx);

        return NULL;
    }

    backend->heap = heap;
    backend->context = ctx;
    backend->capacity = dp->get_size(dp);
    backend->type = TOSDB_BACKEND_TYPE_DISK;
    backend->read = tosdb_backend_disk_read;
    backend->write = tosdb_backend_disk_write;
    backend->flush = tosdb_backend_disk_flush;

    PRINTLOG(TOSDB, LOG_DEBUG, "created backend with capacity 0x%llx (0x%llx) block size 0x%llx",
             backend->capacity, backend->capacity / dp->get_block_size(dp), dp->get_block_size(dp));

    return backend;
}

uint8_t*  tosdb_backend_disk_read(tosdb_backend_t* backend, uint64_t position, uint64_t size) {
    if(!backend) {
        return NULL;
    }

    tosdb_backend_disk_ctx_t* d_ctx = backend->context;

    uint8_t* res = NULL;
    uint64_t bs = d_ctx->dp->get_block_size(d_ctx->dp);

    PRINTLOG(TOSDB, LOG_TRACE, "read from disk position 0x%llx (0x%llx) size 0x%llx (0x%llx)",
             position, position / bs, size, size / bs);

    int64_t read_res = d_ctx->dp->read(d_ctx->dp, position / bs, size / bs, &res);

    return read_res == 0?res:NULL;
}

uint64_t  tosdb_backend_disk_write(tosdb_backend_t* backend, uint64_t position, uint64_t size, uint8_t* data) {
    if(!backend) {
        return NULL;
    }

    tosdb_backend_disk_ctx_t* d_ctx = backend->context;
    uint64_t bs = d_ctx->dp->get_block_size(d_ctx->dp);

    uint64_t res = d_ctx->dp->write(d_ctx->dp, position / bs, size / bs, data);

    return res == 0?size:0;
}

boolean_t tosdb_backend_disk_flush(tosdb_backend_t* backend) {
    if(!backend) {
        return false;
    }

    tosdb_backend_disk_ctx_t* d_ctx = backend->context;

    return d_ctx->dp->flush(d_ctx->dp) == 0?true:false;
}

boolean_t tosdb_backend_disk_close(tosdb_backend_t* backend) {
    boolean_t res = true;

    if(!tosdb_backend_disk_flush(backend)) {
        res = false;
    }

    memory_free_ext(backend->heap, backend->context);
    memory_free_ext(backend->heap, backend);

    return res;
}
