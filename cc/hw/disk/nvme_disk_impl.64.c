/**
 * @file nvme_disk_impl.64.c
 * @brief NVMe disk implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <disk.h>
#include <driver/nvme.h>
#include <utils.h>
#include <linkedlist.h>

MODULE("turnstone.kernel.hw.disk.nvme");

typedef struct nvme_disk_impl_context_t {
    nvme_disk_t* nvme_disk;
    uint64_t     block_size;
} nvme_disk_impl_context_t;

uint64_t nvme_disk_impl_get_size(const disk_or_partition_t* d);
uint64_t nvme_disk_impl_get_block_size(const disk_or_partition_t* d);
int8_t   nvme_disk_impl_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data);
int8_t   nvme_disk_impl_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data);
int8_t   nvme_disk_impl_flush(const disk_or_partition_t* d);
int8_t   nvme_disk_impl_close(const disk_or_partition_t* d);


uint64_t nvme_disk_impl_get_size(const disk_or_partition_t* d){
    nvme_disk_impl_context_t* ctx = (nvme_disk_impl_context_t*)d->context;
    return ctx->nvme_disk->lba_count * ctx->block_size;
}

uint64_t nvme_disk_impl_get_block_size(const disk_or_partition_t* d){
    nvme_disk_impl_context_t* ctx = (nvme_disk_impl_context_t*)d->context;
    return ctx->block_size;
}

int8_t nvme_disk_impl_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data) {
    nvme_disk_impl_context_t* ctx = (nvme_disk_impl_context_t*)d->context;

    if(data == NULL) {
        return -1;
    }

    uint64_t buffer_len = count * ctx->block_size;
    uint64_t real_buffer_len = buffer_len;
    uint8_t* write_buf = data;
    boolean_t need_to_free = false;

    if(real_buffer_len % 0x1000 || (uint64_t)write_buf % 0x1000) {
        real_buffer_len += 0x1000 - (real_buffer_len % 0x1000);
        write_buf = memory_malloc_ext(NULL, real_buffer_len, 0x1000);
        memory_memcopy(data, write_buf, buffer_len);
        need_to_free = true;
    }


    uint64_t offset = 0;
    uint16_t rem_lba = count;
    uint64_t max_lba = MIN(512, ctx->nvme_disk->max_prp_entries);
    future_t fut = NULL;

    linkedlist_t* futs = linkedlist_create_list_with_heap(NULL);

    while(rem_lba) {
        uint16_t iter_read_size = MIN(rem_lba, max_lba);

        do  {
            fut = nvme_write(ctx->nvme_disk->disk_id, lba, iter_read_size * ctx->block_size, write_buf + offset);
        }while(fut == NULL);

        linkedlist_list_insert(futs, fut);

        lba += iter_read_size;
        offset += iter_read_size * ctx->block_size;
        rem_lba -= iter_read_size;
    }

    if(need_to_free) {
        memory_free(write_buf);
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

int8_t nvme_disk_impl_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data){
    nvme_disk_impl_context_t* ctx = (nvme_disk_impl_context_t*)d->context;

    uint64_t buffer_len = count * ctx->block_size;

    if(buffer_len % 0x1000) {
        buffer_len += 0x1000 - (buffer_len % 0x1000);
    }

    *data = memory_malloc_ext(NULL, buffer_len, 0x1000);

    if(*data == NULL) {
        return -1;
    }

    uint8_t* read_buf = *data;
    uint64_t offset = 0;
    uint64_t rem_lba = count;
    uint64_t max_lba = MIN(512, ctx->nvme_disk->max_prp_entries);
    future_t fut = NULL;

    linkedlist_t* futs = linkedlist_create_list_with_heap(NULL);

    while(rem_lba) {
        uint16_t iter_read_size = MIN(rem_lba, max_lba);

        do  {
            fut = nvme_read(ctx->nvme_disk->disk_id, lba, iter_read_size * ctx->block_size, read_buf + offset);
        }while(fut == NULL);

        linkedlist_list_insert(futs, fut);

        lba += iter_read_size;
        offset += iter_read_size * ctx->block_size;
        rem_lba -= iter_read_size;
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

int8_t nvme_disk_impl_flush(const disk_or_partition_t* d) {
    nvme_disk_impl_context_t* ctx = (nvme_disk_impl_context_t*)d->context;

    future_t f_fut = nvme_flush(ctx->nvme_disk->disk_id);

    future_get_data_and_destroy(f_fut);

    return 0;
}

int8_t nvme_disk_impl_close(const disk_or_partition_t* d) {
    nvme_disk_impl_context_t* ctx = (nvme_disk_impl_context_t*)d->context;

    d->flush(d);

    memory_free(ctx);

    memory_free((void*)d);

    return 0;
}

disk_t* nvme_disk_impl_open(nvme_disk_t* nvme_disk) {

    if(nvme_disk == NULL) {
        return NULL;
    }

    nvme_disk_impl_context_t* ctx = memory_malloc(sizeof(nvme_disk_impl_context_t));

    if(ctx == NULL) {
        return NULL;
    }

    ctx->nvme_disk = nvme_disk;
    ctx->block_size = nvme_disk->lba_size;

    disk_t* d = memory_malloc(sizeof(disk_t));

    if(d == NULL) {
        memory_free(ctx);

        return NULL;
    }

    d->disk.context = ctx;
    d->disk.get_size = nvme_disk_impl_get_size;
    d->disk.get_block_size = nvme_disk_impl_get_block_size;
    d->disk.write = nvme_disk_impl_write;
    d->disk.read = nvme_disk_impl_read;
    d->disk.flush = nvme_disk_impl_flush;
    d->disk.close = nvme_disk_impl_close;

    return d;
}
