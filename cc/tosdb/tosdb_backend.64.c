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
#include <logging.h>
#include <logging.h>
#include <strings.h>
#include <xxhash.h>

MODULE("turnstone.kernel.db");

boolean_t tosdb_write_and_flush_superblock(tosdb_backend_t* backend, tosdb_superblock_t* sb) {
    if(!backend || !sb) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend or super block is null");

        return false;
    }

    sb->header.checksum = 0;
    uint64_t csum = xxhash64_hash(sb, sb->header.block_size);

    sb->header.checksum = csum;
    PRINTLOG(TOSDB, LOG_DEBUG, "super block checksum 0x%llx", csum);

    uint64_t w_cnt =  backend->write(backend, 0, sizeof(tosdb_superblock_t), (uint8_t*)sb);

    if(w_cnt != sizeof(tosdb_superblock_t)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot write main super block wcnt %lli", w_cnt);

        return false;
    }

    w_cnt = backend->write(backend, backend->capacity - sb->header.block_size, sb->header.block_size, (uint8_t*)sb);


    if(w_cnt != sizeof(tosdb_superblock_t)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot write backup super block");

        return false;
    }

    boolean_t f_res = backend->flush(backend);

    if(!f_res) {
        PRINTLOG(TOSDB, LOG_ERROR, "flush failed");
    }

    return true;
}

tosdb_superblock_t* tosdb_backend_repair(tosdb_backend_t* backend) {
    UNUSED(backend);

    NOTIMPLEMENTEDLOG(TOSDB);

    return NULL;
}

tosdb_superblock_t* tosdb_backend_format(tosdb_backend_t* backend, compression_type_t compression_type_if_not_exists) {

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
    sb->compression_type = compression_type_if_not_exists;

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
    } else if(backend->type == TOSDB_BACKEND_TYPE_DISK) {
        return tosdb_backend_disk_close(backend);
    }

    PRINTLOG(TOSDB, LOG_ERROR, "not implemented backend");


    return false;
}
