/**
 * @file tosdb.64.c
 * @brief tosdb interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <buffer.h>
#include <cpu/sync.h>
#include <video.h>
#include <strings.h>
#include <xxhash.h>

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

tosdb_superblock_t* tosdb_backend_repair(tosdb_backend_t* backend) {
    UNUSED(backend);

    NOTIMPLEMENTEDLOG(TOSDB);

    return NULL;
}

boolean_t tosdb_write_and_flush_superblock(tosdb_backend_t* backend, tosdb_superblock_t* sb) {
    if(!backend || !sb) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend or super block is null");

        return false;
    }

    uint64_t csum = xxhash64_hash(sb, sizeof(tosdb_superblock_t));

    sb->checksum = csum;

    future_t fut = backend->write(backend, 0, sizeof(tosdb_superblock_t), (uint8_t*)sb);

    if(!fut) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot initiate write main super block");

        return false;
    }

    uint64_t w_cnt = (uint64_t)future_get_data_and_destroy(fut);

    if(w_cnt != sizeof(tosdb_superblock_t)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot write main super block");

        return false;
    }

    fut = backend->write(backend, backend->capacity - sizeof(tosdb_superblock_t), sizeof(tosdb_superblock_t), (uint8_t*)sb);

    if(!fut) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot initiate write backup super block");

        return false;
    }

    w_cnt = (uint64_t)future_get_data_and_destroy(fut);

    if(w_cnt != sizeof(tosdb_superblock_t)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot write backup super block");

        return false;
    }

    fut = backend->flush(backend);

    if(!fut) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot initiate flush");

        return NULL;
    }

    boolean_t f_res = (uint64_t)future_get_data_and_destroy(fut);

    if(!f_res) {
        PRINTLOG(TOSDB, LOG_ERROR, "flush failed");
    }

    return true;
}

tosdb_superblock_t* tosdb_backend_format(tosdb_backend_t* backend) {

    tosdb_superblock_t* sb = memory_malloc(sizeof(tosdb_superblock_t));

    if(!sb) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create super block struct");

        return NULL;
    }

    strcpy(TOSDB_SUPERBLOCK_SIGNATURE, sb->signature);
    sb->block_type = TOSDB_BLOCK_TYPE_SUPERBLOCK;
    sb->block_size = sizeof(tosdb_superblock_t);
    sb->version_major = TOSDB_VERSION_MAJOR;
    sb->version_minor = TOSDB_VERSION_MINOR;
    sb->page_size = TOSDB_PAGE_SIZE;

    if(!tosdb_write_and_flush_superblock(backend, sb)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot write and flush super block");
        memory_free(sb);

        return NULL;
    }

    return sb;
}

tosdb_t* tosdb_new(tosdb_backend_t* backend) {
    if(!backend) {
        PRINTLOG(TOSDB, LOG_ERROR, "backend is null");

        return NULL;
    }

    future_t fut;

    fut = backend->read(backend, 0, sizeof(tosdb_superblock_t));

    if(!fut) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read main super block");
    }

    tosdb_superblock_t* sb = future_get_data_and_destroy(fut);

    if(!sb) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read superblock");

        return NULL;
    }

    boolean_t need_format = false;
    boolean_t need_repair = false;

    if(strcmp(sb->signature, TOSDB_SUPERBLOCK_SIGNATURE) != 0) {
        PRINTLOG(TOSDB, LOG_WARNING, "main super block signature mismatch");

        memory_free(sb);

        fut = backend->read(backend, backend->capacity - sizeof(tosdb_superblock_t), sizeof(tosdb_superblock_t));

        if(!fut) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read backup super block");

            return NULL;
        }

        sb = future_get_data_and_destroy(fut);


        if(!sb) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read backup superblock");

            return NULL;
        }

        if(strcmp(sb->signature, TOSDB_SUPERBLOCK_SIGNATURE) != 0) {
            need_format = true;
        } else {
            need_repair = true;
        }
    }

    if(need_format || need_repair) {
        memory_free(sb);
    }

    if(need_repair) {
        PRINTLOG(TOSDB, LOG_WARNING, "backend needs reparing");

        sb = tosdb_backend_repair(backend);

        if(!sb) {
            return NULL;
        }
    }

    if(need_format) {
        PRINTLOG(TOSDB, LOG_WARNING, "backend needs format");

        sb = tosdb_backend_format(backend);

        if(!sb) {
            return NULL;
        }
    }

    tosdb_t* res = memory_malloc(sizeof(tosdb_t));

    if(!res) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create tosdb struct");
        memory_free(sb);

        return NULL;
    }

    res->backend = backend;
    res->superblock = sb;

    return res;
}

boolean_t tosdb_close(tosdb_t* tdb) {
    if(!tdb) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb is null");

        return false;
    }

    if(!tdb->backend) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb backend is null");

        return false;
    }

    if(!tdb->superblock) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb superblock is null");

        return false;
    }

    if(!tosdb_write_and_flush_superblock(tdb->backend, tdb->superblock)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot write and flush super block");

        return false;
    }

    memory_free(tdb->superblock);
    memory_free(tdb);

    return true;
}
