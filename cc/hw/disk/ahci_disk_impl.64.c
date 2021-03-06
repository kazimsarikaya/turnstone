/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <disk.h>
#include <driver/ahci.h>
#include <utils.h>
#include <linkedlist.h>

typedef struct {
	ahci_sata_disk_t* sata_disk;
	uint64_t block_size;
} ahci_disk_impl_context_t;

uint64_t ahci_disk_impl_get_disk_size(disk_t* d){
	ahci_disk_impl_context_t* ctx = (ahci_disk_impl_context_t*)d->disk_context;
	return ctx->sata_disk->lba_count * ctx->block_size;
}

int8_t ahci_disk_impl_write(disk_t* d, uint64_t lba, uint64_t count, uint8_t* data) {
	ahci_disk_impl_context_t* ctx = (ahci_disk_impl_context_t*)d->disk_context;

	if(data == NULL) {
		return -1;
	}

	uint64_t buffer_len = count * ctx->block_size;
	uint64_t offset = 0;
	uint16_t max_len = -1;
	future_t fut = NULL;

	linkedlist_t futs = linkedlist_create_list_with_heap(NULL);

	while(offset < buffer_len) {
		uint16_t iter_write_size = MIN(buffer_len, max_len);

		do  {
			fut = ahci_write(ctx->sata_disk->disk_id, lba, iter_write_size, data + offset);
		}while(fut == NULL);

		linkedlist_list_insert(futs, fut);


		lba += iter_write_size / ctx->block_size;
		offset += iter_write_size;
	}

	iterator_t* iter = linkedlist_iterator_create(futs);

	while(iter->end_of_iterator(iter) != 0) {
		fut = iter->get_item(iter);

		future_get_data_and_destroy(fut);

		iter = iter->next(iter);
	}

	linkedlist_destroy(futs);

	return 0;
}

int8_t ahci_disk_impl_read(disk_t* d, uint64_t lba, uint64_t count, uint8_t** data){
	ahci_disk_impl_context_t* ctx = (ahci_disk_impl_context_t*)d->disk_context;

	uint64_t buffer_len = count * ctx->block_size;
	*data = memory_malloc(buffer_len);

	if(*data == NULL) {
		return -1;
	}

	uint8_t* read_buf = *data;
	uint64_t offset = 0;
	uint16_t max_len = -1;
	future_t fut = NULL;

	linkedlist_t futs = linkedlist_create_list_with_heap(NULL);

	while(offset < buffer_len) {
		uint16_t iter_read_size = MIN(buffer_len, max_len);

		do  {
			fut = ahci_read(ctx->sata_disk->disk_id, lba, iter_read_size, read_buf + offset);
		}while(fut == NULL);

		linkedlist_list_insert(futs, fut);

		lba += iter_read_size / ctx->block_size;
		offset += iter_read_size;
	}

	iterator_t* iter = linkedlist_iterator_create(futs);

	while(iter->end_of_iterator(iter) != 0) {
		fut = iter->get_item(iter);

		future_get_data_and_destroy(fut);

		iter = iter->next(iter);
	}

	linkedlist_destroy(futs);

	return 0;
}

int8_t ahci_disk_impl_flush(disk_t* d) {
	ahci_disk_impl_context_t* ctx = (ahci_disk_impl_context_t*)d->disk_context;

	future_t f_fut = ahci_flush(ctx->sata_disk->disk_id);

	future_get_data_and_destroy(f_fut);

	return 0;
}

int8_t ahci_disk_impl_close(disk_t* d) {
	ahci_disk_impl_context_t* ctx = (ahci_disk_impl_context_t*)d->disk_context;

	d->flush(d);

	memory_free(ctx);

	memory_free(d);

	return 0;
}

disk_t* ahci_disk_impl_open(ahci_sata_disk_t* sata_disk) {

	if(sata_disk == NULL) {
		return NULL;
	}

	if(sata_disk->inserted == 0) {
		return NULL;
	}

	ahci_disk_impl_context_t* ctx = memory_malloc(sizeof(ahci_disk_impl_context_t));

	ctx->sata_disk = sata_disk;
	ctx->block_size = 512;

	disk_t* d = memory_malloc(sizeof(disk_t));

	d->disk_context = ctx;
	d->get_disk_size = ahci_disk_impl_get_disk_size;
	d->write = ahci_disk_impl_write;
	d->read = ahci_disk_impl_read;
	d->flush = ahci_disk_impl_flush;
	d->close = ahci_disk_impl_close;

	return d;
}
