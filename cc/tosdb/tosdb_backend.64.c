/**
 * @file tosdb_backend.64.c
 * @brief tosdb backend interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_backend.h>
#include <tosdb/tosdb_internal.h>
#include <video.h>
#include <logging.h>
#include <strings.h>

tosdb_superblock_t* tosdb_backend_repair(tosdb_backend_t* backend) {
    UNUSED(backend);

    NOTIMPLEMENTEDLOG(TOSDB);

    return NULL;
}

tosdb_superblock_t* tosdb_backend_format(tosdb_backend_t* backend) {

    tosdb_superblock_t* sb = memory_malloc(sizeof(tosdb_superblock_t));

    if(!sb) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create super block struct");

        return NULL;
    }

    strcpy(TOSDB_SUPERBLOCK_SIGNATURE, sb->header.signature);
    sb->header.block_type = TOSDB_BLOCK_TYPE_SUPERBLOCK;
    sb->header.block_size = sizeof(tosdb_superblock_t);
    sb->header.version_major = TOSDB_VERSION_MAJOR;
    sb->header.version_minor = TOSDB_VERSION_MINOR;
    sb->capacity = backend->capacity;
    sb->page_size = TOSDB_PAGE_SIZE;
    sb->free_next_location = sizeof(tosdb_superblock_t);
    sb->database_next_id = 1;

    if(!tosdb_write_and_flush_superblock(backend, sb)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot write and flush super block");
        memory_free(sb);

        return NULL;
    }

    return sb;
}

boolean_t tosdb_backend_close(tosdb_backend_t* backend) {
    if(!backend) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend is null");

        return false;
    }

    if(backend->type == TOSDB_BACKEND_TYPE_MEMORY) {
        return tosdb_backend_memory_close(backend);
    }

    PRINTLOG(TOSDB, LOG_ERROR, "not implemented backend");


    return false;
}
