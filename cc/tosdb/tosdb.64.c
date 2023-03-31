/**
 * @file tosdb.64.c
 * @brief tosdb interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <future.h>
#include <buffer.h>
#include <cpu/sync.h>

typedef future_t (*tosdb_backend_read_f)(tosdb_backend_t* backend, uint64_t position, uint64_t size);
typedef future_t (*tosdb_backend_write_f)(tosdb_backend_t* backend, uint64_t position, uint64_t size, uint8_t* data);
typedef future_t (*tosdb_backend_flush_f)(tosdb_backend_t* backend);

struct tosdb_backend_t {
    void*                 context;
    tosdb_backend_read_f  read;
    tosdb_backend_write_f write;
    tosdb_backend_flush_f flush;
};


typedef struct tosdb_backend_memory_ctx_t {
    buffer_t buffer;
} tosdb_backend_memory_ctx_t;

future_t tosdb_backend_memory_read(tosdb_backend_t* backend, uint64_t position, uint64_t size);
future_t tosdb_backend_memory_write(tosdb_backend_t* backend, uint64_t position, uint64_t size, uint8_t* data);
future_t tosdb_backend_memory_flush(tosdb_backend_t* backend);

future_t tosdb_backend_memory_read(tosdb_backend_t* backend, uint64_t position, uint64_t size) {
    UNUSED(backend);
    UNUSED(position);
    UNUSED(size);

    return future_create(NULL);
}

future_t tosdb_backend_memory_write(tosdb_backend_t* backend, uint64_t position, uint64_t size, uint8_t* data) {
    UNUSED(backend);
    UNUSED(position);
    UNUSED(size);
    UNUSED(data);

    return future_create(NULL);
}

future_t tosdb_backend_memory_flush(tosdb_backend_t* backend) {
    UNUSED(backend);

    return future_create(NULL);
}

tosdb_backend_t* tosdb_backend_memory_new(void) {
    tosdb_backend_memory_ctx_t* ctx = memory_malloc(sizeof(tosdb_backend_memory_ctx_t));

    if(!ctx) {
        return NULL;
    }

    ctx->buffer = buffer_new();

    if(!ctx->buffer) {
        memory_free(ctx);

        return NULL;
    }

    tosdb_backend_t* backend = memory_malloc(sizeof(tosdb_backend_t));

    if(!backend) {
        buffer_destroy(ctx->buffer);
        memory_free(ctx);

        return NULL;
    }

    backend->context = ctx;
    backend->read = tosdb_backend_memory_read;
    backend->write = tosdb_backend_memory_write;
    backend->flush = tosdb_backend_memory_flush;

    return backend;
}

boolean_t tosdb_backend_close(tosdb_backend_t* backend) {
    if(!backend) {
        return false;
    }

    tosdb_backend_memory_ctx_t* ctx = backend->context;

    if(!ctx) {
        return false;
    }

    buffer_destroy(ctx->buffer);
    memory_free(ctx);
    memory_free(backend);

    return true;
}


