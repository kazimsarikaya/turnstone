/**
 * @file tosdb_backend_memory.64.c
 * @brief tosdb memory backend implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <tosdb/tosdb_backend.h>
#include <future.h>
#include <buffer.h>
#include <cpu/sync.h>
#include <video.h>
#include <strings.h>
#include <xxhash.h>


typedef struct tosdb_backend_memory_ctx_t {
    buffer_t buffer;
} tosdb_backend_memory_ctx_t;

uint8_t*  tosdb_backend_memory_read(tosdb_backend_t* backend, uint64_t position, uint64_t size);
uint64_t  tosdb_backend_memory_write(tosdb_backend_t* backend, uint64_t position, uint64_t size, uint8_t* data);
boolean_t tosdb_backend_memory_flush(tosdb_backend_t* backend);

uint8_t* tosdb_backend_memory_read(tosdb_backend_t* backend, uint64_t position, uint64_t size) {
    if(!backend) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend is null");

        return NULL;
    }

    if(backend->type != TOSDB_BACKEND_TYPE_MEMORY) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend is not memory");

        return NULL;
    }

    tosdb_backend_memory_ctx_t* ctx = backend->context;

    if(!ctx) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend context is null");

        return NULL;
    }

    if(!ctx->buffer) {
        PRINTLOG(TOSDB, LOG_ERROR, "memory buffer is null");

        return NULL;
    }

    buffer_seek(ctx->buffer, position, BUFFER_SEEK_DIRECTION_START);


    uint8_t* data = buffer_get_bytes(ctx->buffer, size);

    return data;
}

uint64_t tosdb_backend_memory_write(tosdb_backend_t* backend, uint64_t position, uint64_t size, uint8_t* data) {
    if(!backend) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend is null");

        return NULL;
    }

    if(backend->type != TOSDB_BACKEND_TYPE_MEMORY) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend is not memory");

        return NULL;
    }

    tosdb_backend_memory_ctx_t* ctx = backend->context;

    if(!ctx) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend context is null");

        return NULL;
    }

    if(!ctx->buffer) {
        PRINTLOG(TOSDB, LOG_ERROR, "memory buffer is null");

        return NULL;
    }

    buffer_seek(ctx->buffer, position, BUFFER_SEEK_DIRECTION_START);

    if(!buffer_append_bytes(ctx->buffer, data, size)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot write to memory backend");

        return NULL;
    }

    return size;
}

boolean_t tosdb_backend_memory_flush(tosdb_backend_t* backend) {
    UNUSED(backend);

    return true;
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

    buffer_seek(ctx->buffer, capacity - 1, BUFFER_SEEK_DIRECTION_START);
    buffer_append_byte(ctx->buffer, 0);

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

tosdb_backend_t* tosdb_backend_memory_from_buffer(buffer_t buffer) {
    if(!buffer) {
        PRINTLOG(TOSDB, LOG_ERROR, "buffer is null");

        return NULL;
    }

    uint64_t capacity = buffer_get_capacity(buffer);

    if(capacity == 0) {
        PRINTLOG(TOSDB, LOG_ERROR, "zero capacity");

        return NULL;
    }

    tosdb_backend_memory_ctx_t* ctx = memory_malloc(sizeof(tosdb_backend_memory_ctx_t));

    if(!ctx) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create backend context");

        return NULL;
    }


    ctx->buffer = buffer;

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

boolean_t tosdb_backend_memory_close(tosdb_backend_t* backend) {
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
