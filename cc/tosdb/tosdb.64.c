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

typedef future_t* (*tosdb_backend_read_f)(tosdb_backend_t* backend, uint64_t position, uint64_t page_count);
typedef future_t* (*tosdb_backend_write_f)(tosdb_backend_t* backend, uint64_t position, uint64_t size, uint8_t* data);
typedef future_t* (*tosdb_backend_flush_f)(tosdb_backend_t* backend);

struct tosdb_backend_t {
    void*                 context;
    tosdb_backend_read_f  read;
    tosdb_backend_write_f write;
    tosdb_backend_flush_f flush;
};


typedef struct tosdb_backend_memory_ctx_t {
    buffer_t buffer;
} tosdb_backend_memory_ctx_t;

future_t* tosdb_backend_memory_read(tosdb_backend_t* backend, uint64_t position, uint64_t page_count);
future_t* tosdb_backend_memory_write(tosdb_backend_t* backend, uint64_t position, uint64_t size, uint8_t* data);
future_t* tosdb_backend_memory_flush(tosdb_backend_t* backend);
