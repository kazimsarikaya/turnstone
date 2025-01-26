/**
 * @file efi_disk_impl.c
 * @brief efi disk implementation.
 * @details uses efi block io protocol to implement disk methods.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <setup.h>

/*! module name */
MODULE("turnstone.efi");

/**
 * @struct disk_context_t
 * @brief efi disk implementation context.
 * @details this structure contains context for efi disk implementation.
 */
typedef struct disk_context_t {
    efi_block_io_t* bio; ///< efi block io protocol.
    uint64_t        disk_size; ///< disk size.
    uint64_t        block_size; ///< block size.
} disk_context_t; ///< typedef for disk_context_t.

/**
 * @brief returns heap.
 * @param[in] d disk or partition.
 * @return heap.
 */
memory_heap_t* efi_disk_impl_get_heap(const disk_or_partition_t* d);

/**
 * @brief returns disk size.
 * @param[in] d disk or partition.
 * @return disk size.
 */
uint64_t efi_disk_impl_get_disk_size(const disk_or_partition_t* d);

/**
 * @brief returns block size.
 * @param[in] d disk or partition.
 * @return block size.
 */
uint64_t efi_disk_impl_get_block_size(const disk_or_partition_t* d);

/**
 * @brief writes data to disk.
 * @param[in] d disk or partition.
 * @param[in] lba logical block address.
 * @param[in] count number of blocks to write.
 * @param[in] data data to write.
 * @return 0 on success.
 */
int8_t efi_disk_impl_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data);

/**
 * @brief reads data from disk.
 * @param[in] d disk or partition.
 * @param[in] lba logical block address.
 * @param[in] count number of blocks to read.
 * @param[out] data data to read.
 * @return 0 on success.
 */
int8_t efi_disk_impl_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data);

/**
 * @brief closes and frees disk.
 * @param[in] d disk or partition.
 * @return 0 on success.
 */
int8_t efi_disk_impl_close(const disk_or_partition_t* d);

/**
 * @brief flushes disk.
 * @param[in] d disk or partition.
 * @return 0 on success.
 */
int8_t efi_disk_impl_flush(const disk_or_partition_t* d);

memory_heap_t* efi_disk_impl_get_heap(const disk_or_partition_t* d) {
    UNUSED(d);
    return memory_get_heap(NULL);
}

uint64_t efi_disk_impl_get_disk_size(const disk_or_partition_t* d){
    disk_context_t* ctx = (disk_context_t*)d->context;
    return ctx->disk_size;
}

uint64_t efi_disk_impl_get_block_size(const disk_or_partition_t* d){
    disk_context_t* ctx = (disk_context_t*)d->context;
    return ctx->block_size;
}

int8_t efi_disk_impl_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data) {
    disk_context_t* ctx = (disk_context_t*)d->context;

    ctx->bio->write(ctx->bio, ctx->bio->media->media_id, lba, count * ctx->block_size, data);
    ctx->bio->flush(ctx->bio);

    return 0;
}

int8_t efi_disk_impl_flush(const disk_or_partition_t* d) {
    disk_context_t* ctx = (disk_context_t*)d->context;

    ctx->bio->flush(ctx->bio);

    return 0;
}

int8_t efi_disk_impl_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data){
    disk_context_t* ctx = (disk_context_t*)d->context;

    *data = memory_malloc(count * ctx->block_size);

    efi_status_t res = ctx->bio->read(ctx->bio, ctx->bio->media->media_id, lba, count * ctx->block_size, *data);

    return res == EFI_SUCCESS?0:-1;
}

int8_t efi_disk_impl_close(const disk_or_partition_t* d) {
    disk_context_t* ctx = (disk_context_t*)d->context;

    memory_free(ctx);

    memory_free((void*)d);

    return 0;
}

disk_t* efi_disk_impl_open(efi_block_io_t* bio) {

    disk_context_t* ctx = memory_malloc(sizeof(disk_context_t));

    if(ctx == NULL) {
        return NULL;
    }

    ctx->bio = bio;
    ctx->disk_size = (bio->media->last_block + 1) * bio->media->block_size;
    ctx->block_size = bio->media->block_size;

    disk_t* d = memory_malloc(sizeof(disk_t));

    if(d == NULL) {
        memory_free(ctx);

        return NULL;
    }

    d->disk.context = ctx;
    d->disk.get_heap = efi_disk_impl_get_heap;
    d->disk.get_size = efi_disk_impl_get_disk_size;
    d->disk.get_block_size = efi_disk_impl_get_block_size;
    d->disk.write = efi_disk_impl_write;
    d->disk.read = efi_disk_impl_read;
    d->disk.close = efi_disk_impl_close;
    d->disk.flush = efi_disk_impl_flush;

    return d;
}
