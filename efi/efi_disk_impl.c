/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <setup.h>


typedef struct {
	efi_block_io_t* bio;
	uint64_t disk_size;
	uint64_t block_size;
} efi_disk_impl_context_t;

uint64_t efi_disk_impl_get_disk_size(disk_t* d){
	efi_disk_impl_context_t* ctx = (efi_disk_impl_context_t*)d->disk_context;
	return ctx->disk_size;
}

int8_t efi_disk_impl_write(disk_t* d, uint64_t lba, uint64_t count, uint8_t* data) {
	efi_disk_impl_context_t* ctx = (efi_disk_impl_context_t*)d->disk_context;

	ctx->bio->write(ctx->bio, ctx->bio->media->media_id, lba, count * ctx->block_size, data);
	ctx->bio->flush(ctx->bio);

	return 0;
}

int8_t efi_disk_impl_read(disk_t* d, uint64_t lba, uint64_t count, uint8_t** data){
	efi_disk_impl_context_t* ctx = (efi_disk_impl_context_t*)d->disk_context;

	*data = memory_malloc(count * ctx->block_size);

	efi_status_t res = ctx->bio->read(ctx->bio, ctx->bio->media->media_id, lba, count * ctx->block_size, *data);

	return res == EFI_SUCCESS?0:-1;
}

int8_t efi_disk_impl_close(disk_t* d) {
	efi_disk_impl_context_t* ctx = (efi_disk_impl_context_t*)d->disk_context;

	memory_free(ctx);

	memory_free(d);

	return 0;
}

disk_t* efi_disk_impl_open(efi_block_io_t* bio) {

	efi_disk_impl_context_t* ctx = memory_malloc(sizeof(efi_disk_impl_context_t));

	ctx->bio = bio;
	ctx->disk_size = (bio->media->last_block + 1) * bio->media->block_size;
	ctx->block_size = bio->media->block_size;

	disk_t* d = memory_malloc(sizeof(disk_t));

	d->disk_context = ctx;
	d->get_disk_size = efi_disk_impl_get_disk_size;
	d->write = efi_disk_impl_write;
	d->read = efi_disk_impl_read;
	d->close = efi_disk_impl_close;

	return d;
}
