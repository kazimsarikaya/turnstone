/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <disk.h>
#include <driver/ahci.h>
#include <utils.h>
#include <linkedlist.h>

MODULE("turnstone.kernel.hw.disk.ahci");

typedef struct ahci_disk_impl_context_t {
    ahci_sata_disk_t* sata_disk;
    uint64_t          block_size;
} ahci_disk_impl_context_t;

uint64_t ahci_disk_impl_get_size(const disk_or_partition_t* d);
uint64_t ahci_disk_impl_get_block_size(const disk_or_partition_t* d);
int8_t   ahci_disk_impl_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data);
int8_t   ahci_disk_impl_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data);
int8_t   ahci_disk_impl_flush(const disk_or_partition_t* d);
int8_t   ahci_disk_impl_close(const disk_or_partition_t* d);


uint64_t ahci_disk_impl_get_size(const disk_or_partition_t* d){
    ahci_disk_impl_context_t* ctx = (ahci_disk_impl_context_t*)d->context;
    return ctx->sata_disk->lba_count * ctx->block_size;
}

uint64_t ahci_disk_impl_get_block_size(const disk_or_partition_t* d){
    ahci_disk_impl_context_t* ctx = (ahci_disk_impl_context_t*)d->context;
    return ctx->block_size;
}

int8_t ahci_disk_impl_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data) {
    ahci_disk_impl_context_t* ctx = (ahci_disk_impl_context_t*)d->context;

    if(data == NULL) {
        return -1;
    }

    uint64_t buffer_len = count * ctx->block_size;
    uint64_t offset = 0;
    uint32_t max_len = 65536 * ctx->block_size;
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
        fut = (future_t*)iter->get_item(iter);

        future_get_data_and_destroy(fut);

        iter = iter->next(iter);
    }

    linkedlist_destroy(futs);

    return 0;
}

int8_t ahci_disk_impl_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data){
    ahci_disk_impl_context_t* ctx = (ahci_disk_impl_context_t*)d->context;

    uint64_t buffer_len = count * ctx->block_size;
    *data = memory_malloc(buffer_len);

    if(*data == NULL) {
        return -1;
    }

    uint8_t* read_buf = *data;
    uint64_t offset = 0;
    uint32_t max_len = 65536 * ctx->block_size;
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
        fut = (future_t*)iter->get_item(iter);

        future_get_data_and_destroy(fut);

        iter = iter->next(iter);
    }

    linkedlist_destroy(futs);

    return 0;
}

int8_t ahci_disk_impl_flush(const disk_or_partition_t* d) {
    ahci_disk_impl_context_t* ctx = (ahci_disk_impl_context_t*)d->context;

    future_t f_fut = ahci_flush(ctx->sata_disk->disk_id);

    future_get_data_and_destroy(f_fut);

    return 0;
}

int8_t ahci_disk_impl_close(const disk_or_partition_t* d) {
    ahci_disk_impl_context_t* ctx = (ahci_disk_impl_context_t*)d->context;

    d->flush(d);

    memory_free(ctx);

    memory_free((void*)d);

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

    if(ctx == NULL) {
        return NULL;
    }

    ctx->sata_disk = sata_disk;
    ctx->block_size = sata_disk->logical_sector_size;

    disk_t* d = memory_malloc(sizeof(disk_t));

    if(d == NULL) {
        memory_free(ctx);

        return NULL;
    }

    d->disk.context = ctx;
    d->disk.get_size = ahci_disk_impl_get_size;
    d->disk.get_block_size = ahci_disk_impl_get_block_size;
    d->disk.write = ahci_disk_impl_write;
    d->disk.read = ahci_disk_impl_read;
    d->disk.flush = ahci_disk_impl_flush;
    d->disk.close = ahci_disk_impl_close;

    return d;
}
