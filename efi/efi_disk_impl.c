/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <setup.h>


typedef struct efi_disk_impl_context_t {
    efi_block_io_t* bio;
    uint64_t        disk_size;
    uint64_t        block_size;
} efi_disk_impl_context_t;

uint64_t efi_disk_impl_get_disk_size(const disk_or_partition_t* d);
int8_t   efi_disk_impl_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data);
int8_t   efi_disk_impl_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data);
int8_t   efi_disk_impl_close(const disk_or_partition_t* d);


uint64_t efi_disk_impl_get_disk_size(const disk_or_partition_t* d){
    efi_disk_impl_context_t* ctx = (efi_disk_impl_context_t*)d->context;
    return ctx->disk_size;
}

int8_t efi_disk_impl_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data) {
    efi_disk_impl_context_t* ctx = (efi_disk_impl_context_t*)d->context;

    ctx->bio->write(ctx->bio, ctx->bio->media->media_id, lba, count * ctx->block_size, data);
    ctx->bio->flush(ctx->bio);

    return 0;
}

int8_t efi_disk_impl_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data){
    efi_disk_impl_context_t* ctx = (efi_disk_impl_context_t*)d->context;

    *data = memory_malloc(count * ctx->block_size);

    efi_status_t res = ctx->bio->read(ctx->bio, ctx->bio->media->media_id, lba, count * ctx->block_size, *data);

    return res == EFI_SUCCESS?0:-1;
}

int8_t efi_disk_impl_close(const disk_or_partition_t* d) {
    efi_disk_impl_context_t* ctx = (efi_disk_impl_context_t*)d->context;

    memory_free(ctx);

    memory_free((void*)d);

    return 0;
}

disk_t* efi_disk_impl_open(efi_block_io_t* bio) {

    efi_disk_impl_context_t* ctx = memory_malloc(sizeof(efi_disk_impl_context_t));

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
    d->disk.get_size = efi_disk_impl_get_disk_size;
    d->disk.write = efi_disk_impl_write;
    d->disk.read = efi_disk_impl_read;
    d->disk.close = efi_disk_impl_close;

    return d;
}
