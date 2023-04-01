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
#include <video.h>

#define TOSDB_PAGE_SIZE 4096

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

    PRINTLOG(TOSDB, LOG_ERROR, "not implemented");

    return future_create(NULL);
}

future_t tosdb_backend_memory_write(tosdb_backend_t* backend, uint64_t position, uint64_t size, uint8_t* data) {
    UNUSED(backend);
    UNUSED(position);
    UNUSED(size);
    UNUSED(data);

    PRINTLOG(TOSDB, LOG_ERROR, "not implemented");

    return future_create(NULL);
}

future_t tosdb_backend_memory_flush(tosdb_backend_t* backend) {
    UNUSED(backend);

    PRINTLOG(TOSDB, LOG_ERROR, "not implemented");

    return future_create(NULL);
}

tosdb_backend_t* tosdb_backend_memory_new(uint64_t capacity) {
    if(capacity == 0) {
        PRINTLOG(TOSDB, LOG_ERROR, "zero capacity");

        return NULL;
    }

    tosdb_backend_memory_ctx_t* ctx = memory_malloc(sizeof(tosdb_backend_memory_ctx_t));

    if(!ctx) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create backend context");

        return NULL;
    }

    ctx->buffer = buffer_new_with_capacity(NULL, capacity);

    if(!ctx->buffer) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create backend buffer");
        memory_free(ctx);

        return NULL;
    }

    tosdb_backend_t* backend = memory_malloc(sizeof(tosdb_backend_t));

    if(!backend) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create backend");
        buffer_destroy(ctx->buffer);
        memory_free(ctx);

        return NULL;
    }

    backend->context = ctx;
    backend->type = TOSDB_BACKEND_TYPE_MEMORY;
    backend->capacity = capacity;
    backend->read = tosdb_backend_memory_read;
    backend->write = tosdb_backend_memory_write;
    backend->flush = tosdb_backend_memory_flush;

    return backend;
}

uint8_t* tosdb_backend_memory_get_contents(tosdb_backend_t* backend) {
    if(!backend) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend is null");

        return NULL;
    }

    if(backend->type != TOSDB_BACKEND_TYPE_MEMORY) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend type is not memory");

        return NULL;
    }

    tosdb_backend_memory_ctx_t* ctx = backend->context;

    if(!ctx) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend context is null");

        return NULL;
    }

    if(!ctx->buffer) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend buffer is null");

        return NULL;
    }

    return buffer_get_all_bytes(ctx->buffer, NULL);
}


boolean_t tosdb_backend_close(tosdb_backend_t* backend) {
    if(!backend) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend is null");

        return false;
    }

    if(backend->type == TOSDB_BACKEND_TYPE_MEMORY) {

        tosdb_backend_memory_ctx_t* ctx = backend->context;

        if(!ctx) {
            PRINTLOG(TOSDB, LOG_ERROR, "backend context is null");

            return false;
        }

        buffer_destroy(ctx->buffer);
        memory_free(ctx);
        memory_free(backend);

        return true;

    }

    PRINTLOG(TOSDB, LOG_ERROR, "not implemented backend");


    return false;
}


