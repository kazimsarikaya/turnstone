/**
 * @file ahci_disk_impl.64.c
 * @brief AHCI Disk Implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <disk.h>
#include <driver/ahci.h>
#include <utils.h>
#include <list.h>
#include <logging.h>

MODULE("turnstone.kernel.hw.disk.ahci");

typedef struct disk_context_t {
    ahci_sata_disk_t* sata_disk;
    uint64_t          block_size;
} disk_context_t;

memory_heap_t* ahci_disk_impl_get_heap(const disk_or_partition_t* d);
uint64_t       ahci_disk_impl_get_size(const disk_or_partition_t* d);
uint64_t       ahci_disk_impl_get_block_size(const disk_or_partition_t* d);
int8_t         ahci_disk_impl_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data);
int8_t         ahci_disk_impl_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data);
int8_t         ahci_disk_impl_flush(const disk_or_partition_t* d);
int8_t         ahci_disk_impl_close(const disk_or_partition_t* d);


memory_heap_t* ahci_disk_impl_get_heap(const disk_or_partition_t* d) {
    disk_context_t* ctx = (disk_context_t*)d->context;
    return ctx->sata_disk->heap;
}

uint64_t ahci_disk_impl_get_size(const disk_or_partition_t* d){
    disk_context_t* ctx = (disk_context_t*)d->context;
    return ctx->sata_disk->lba_count * ctx->block_size;
}

uint64_t ahci_disk_impl_get_block_size(const disk_or_partition_t* d){
    disk_context_t* ctx = (disk_context_t*)d->context;
    return ctx->block_size;
}

int8_t ahci_disk_impl_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data) {
    disk_context_t* ctx = (disk_context_t*)d->context;

    if(data == NULL) {
        return -1;
    }

    uint64_t buffer_len = count * ctx->block_size;
    uint64_t offset = 0;
    uint32_t max_len = 65536 * ctx->block_size;
    future_t* fut = NULL;

    list_t* futs = list_create_list_with_heap(ctx->sata_disk->heap);

    while(offset < buffer_len) {
        uint32_t iter_write_size = MIN(buffer_len, max_len);

        do  {
            fut = ahci_write(ctx->sata_disk->disk_id, lba, iter_write_size, data + offset);
        }while(fut == NULL);

        list_list_insert(futs, fut);


        lba += iter_write_size / ctx->block_size;
        offset += iter_write_size;
    }

    iterator_t* iter = list_iterator_create(futs);

    while(iter->end_of_iterator(iter) != 0) {
        fut = (future_t*)iter->get_item(iter);

        future_get_data_and_destroy(fut);

        iter = iter->next(iter);
    }

    list_destroy(futs);

    return 0;
}

int8_t ahci_disk_impl_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data){
    disk_context_t* ctx = (disk_context_t*)d->context;

    PRINTLOG(AHCI, LOG_TRACE, "reading disk with lba: 0x%llx, count: 0x%llx\n", lba, count);

    if(ctx == NULL || data == NULL || count == 0) {
        PRINTLOG(AHCI, LOG_ERROR, "invalid parameter\n");
        return -1;
    }

    if(lba + count > ctx->sata_disk->lba_count) {
        PRINTLOG(AHCI, LOG_ERROR, "invalid parameter outside disk.\n");
        return -1;
    }

    if(ctx->block_size == 0) {
        PRINTLOG(AHCI, LOG_ERROR, "invalid block size\n");
        return -1;
    }

    uint64_t buffer_len = count * ctx->block_size;

    if(buffer_len == 0) {
        PRINTLOG(AHCI, LOG_ERROR, "invalid buffer length\n");
        return -1;
    }

    *data = memory_malloc_ext(ctx->sata_disk->heap, buffer_len, 0x0);

    if(*data == NULL) {
        PRINTLOG(AHCI, LOG_ERROR, "failed to allocate memory\n");
        return -1;
    }

    uint8_t* read_buf = *data;
    uint64_t offset = 0;
    uint32_t max_len = 65536 * ctx->block_size;
    future_t* fut = NULL;

    list_t* futs = list_create_list_with_heap(ctx->sata_disk->heap);

    if(futs == NULL) {
        PRINTLOG(AHCI, LOG_ERROR, "failed to create future list\n");
        memory_free_ext(ctx->sata_disk->heap, *data);
        return -1;
    }

    while(offset < buffer_len) {
        uint32_t iter_read_size = MIN(buffer_len, max_len);

        do  {
            PRINTLOG(AHCI, LOG_TRACE, "sending read request with lba: 0x%llx, count: 0x%x to offset 0x%llx\n", lba, iter_read_size, offset);
            fut = ahci_read(ctx->sata_disk->disk_id, lba, iter_read_size, read_buf + offset);
        }while(fut == NULL);

        if(fut == NULL) {
            list_destroy(futs);
            memory_free_ext(ctx->sata_disk->heap, *data);
            PRINTLOG(AHCI, LOG_ERROR, "failed to create future\n");
            return -1;
        }

        list_list_insert(futs, fut);

        lba += iter_read_size / ctx->block_size;
        offset += iter_read_size;
    }

    iterator_t* iter = list_iterator_create(futs);

    while(iter->end_of_iterator(iter) != 0) {
        fut = (future_t*)iter->get_item(iter);

        future_get_data_and_destroy(fut);

        iter = iter->next(iter);
    }

    list_destroy(futs);

    PRINTLOG(AHCI, LOG_TRACE, "read disk with lba: 0x%llx, count: 0x%llx\n", lba, count);

    return 0;
}

int8_t ahci_disk_impl_flush(const disk_or_partition_t* d) {
    disk_context_t* ctx = (disk_context_t*)d->context;

    future_t* f_fut = ahci_flush(ctx->sata_disk->disk_id);

    future_get_data_and_destroy(f_fut);

    return 0;
}

int8_t ahci_disk_impl_close(const disk_or_partition_t* d) {
    disk_context_t* ctx = (disk_context_t*)d->context;

    d->flush(d);

    memory_heap_t* heap = ctx->sata_disk->heap;

    memory_free_ext(heap, ctx);

    memory_free_ext(heap, (void*)d);

    return 0;
}

disk_t* ahci_disk_impl_open(ahci_sata_disk_t* sata_disk) {

    if(sata_disk == NULL) {
        return NULL;
    }

    if(sata_disk->inserted == 0) {
        return NULL;
    }

    disk_context_t* ctx = memory_malloc_ext(sata_disk->heap, sizeof(disk_context_t), 0);

    if(ctx == NULL) {
        return NULL;
    }

    ctx->sata_disk = sata_disk;
    ctx->block_size = sata_disk->logical_sector_size;

    disk_t* d = memory_malloc_ext(sata_disk->heap, sizeof(disk_t), 0);

    if(d == NULL) {
        memory_free_ext(sata_disk->heap, ctx);

        return NULL;
    }

    d->disk.context = ctx;
    d->disk.get_heap = ahci_disk_impl_get_heap;
    d->disk.get_size = ahci_disk_impl_get_size;
    d->disk.get_block_size = ahci_disk_impl_get_block_size;
    d->disk.write = ahci_disk_impl_write;
    d->disk.read = ahci_disk_impl_read;
    d->disk.flush = ahci_disk_impl_flush;
    d->disk.close = ahci_disk_impl_close;

    return d;
}
