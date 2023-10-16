/**
 * @file disk_gpt.64.c
 * @brief GPT Disk Implementation.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <efi.h>
#include <disk.h>
#include <crc.h>
#include <strings.h>
#include <memory.h>
#include <random.h>

MODULE("turnstone.kernel.hw.disk");

typedef struct gpt_parts_iter_metadata_t {
    gpt_disk_t* disk;
    uint8_t     current_part_no;
} gpt_parts_iter_metadata_t;

int8_t                    gpt_disk_close(const disk_or_partition_t* d);
int8_t                    gpt_write_gpt_metadata(const disk_t* d);
int8_t                    gpt_check_and_format_if_need(const disk_t* d);
uint8_t                   gpt_add_partition(const disk_t* d, disk_partition_context_t* part_ctx);
int8_t                    gpt_del_partition(const disk_t* d, uint8_t partno);
disk_partition_context_t* gpt_get_partition_context(const disk_t* d, uint8_t partno);
disk_partition_t*         gpt_get_partition(const disk_t* d, uint8_t partno);
disk_partition_t*         gpt_get_partition_by_type_data(const disk_t* d, const void* data);
int8_t                    gpt_part_iter_destroy(iterator_t* iter);
iterator_t*               gpt_part_iter_next(iterator_t* iter);
int8_t                    gpt_part_iter_end_of_iterator(iterator_t* iter);
const void*               gpt_part_iter_get_item(iterator_t* iter);
const void*               gpt_part_iter_delete_item(iterator_t* iter);
iterator_t*               gpt_get_partition_contexts(const disk_t* d);

int8_t efi_create_guid(efi_guid_t* guid){
    guid->time_low = rand();
    guid->time_mid = rand();
    guid->time_high_version = rand();

    for(int32_t i = 0; i < 8; i++) {
        guid->data[i] = rand();
    }

    return 0;
}


int8_t efi_guid_equal(efi_guid_t guid1, efi_guid_t guid2) {
    if(guid1.time_low == guid2.time_low && guid1.time_mid == guid2.time_mid &&  guid1.time_high_version == guid2.time_high_version) {
        for(int64_t i = 0; i < 8; i++) {
            if(guid1.data[i] != guid2.data[i]) {
                return -1;
            }
        }
        return 0;
    }

    return -1;
}



int8_t gpt_disk_close(const disk_or_partition_t* d) {

    gpt_disk_t* gd = (gpt_disk_t*)d;

    gd->underlaying_disk_pointer->close((disk_or_partition_t*)gd->underlaying_disk_pointer);

    memory_free(gd->gpt_header);
    memory_free(gd->partitions);

    memory_free(gd);

    return 0;
}

int8_t gpt_write_gpt_metadata(const disk_t* d) {
    gpt_disk_t* gd = (gpt_disk_t*)d;
    disk_or_partition_t* dp = (disk_or_partition_t*)d;

    uint64_t block_size = dp->get_block_size(dp);

    uint64_t size = dp->get_size(dp);
    int64_t gpt_parts_size = sizeof(efi_partition_entry_t) * 128;
    int64_t max_lba = size / block_size - 1;
    uint64_t gpt_parts_block_count = (gpt_parts_size + block_size - 1)  / block_size;

    int32_t pes_crc32 = CRC32_SEED;
    pes_crc32 = crc32_sum((uint8_t*)gd->partitions, gpt_parts_size, pes_crc32);
    pes_crc32 ^= CRC32_SEED;

    efi_partition_table_header_t* gpt = (efi_partition_table_header_t*)gd->gpt_header;

    gpt->partition_entries_crc32 = pes_crc32;

    gpt->header.crc32 = 0;
    int32_t gpt_crc32 = CRC32_SEED;
    gpt_crc32 = crc32_sum((uint8_t*)gpt, sizeof(efi_partition_table_header_t), gpt_crc32);
    gpt_crc32 ^= CRC32_SEED;
    gpt->header.crc32 = gpt_crc32;

    dp->write(dp, 1, 1, (uint8_t*)gd->gpt_header);

    dp->write(dp, 2, gpt_parts_block_count, (uint8_t*)gd->partitions);
    dp->write(dp, gpt->last_usable_lba + 1, gpt_parts_block_count, (uint8_t*)gd->partitions);

    gpt->partition_entry_lba = gpt->last_usable_lba + 1;
    gpt->my_lba = max_lba;
    gpt->alternate_lba = 1;

    gpt->header.crc32 = 0;
    gpt_crc32 = CRC32_SEED;
    gpt_crc32 = crc32_sum((uint8_t*)gpt, sizeof(efi_partition_table_header_t), gpt_crc32);
    gpt_crc32 ^= CRC32_SEED;
    gpt->header.crc32 = gpt_crc32;

    dp->write(dp, max_lba, 1, (uint8_t*)gd->gpt_header);

    memory_free(gd->gpt_header);

    dp->read(dp, 1, 1, (uint8_t**)&gd->gpt_header);

    return 0;
}

int8_t gpt_check_and_format_if_need(const disk_t* d) {
    gpt_disk_t* gd = (gpt_disk_t*)d;
    disk_or_partition_t* dp = (disk_or_partition_t*)d;

    uint64_t block_size = dp->get_block_size(dp);

    uint64_t size = dp->get_size((disk_or_partition_t*)d);
    uint64_t sector_count = size / block_size;

    int64_t gpt_parts_size = sizeof(efi_partition_entry_t) * 128;
    int64_t max_lba = size / block_size - 1;
    uint64_t gpt_parts_block_count = (gpt_parts_size + block_size - 1)  / block_size;

    uint8_t* data;

    dp->read(dp, 0, 1, &data);

    efi_pmbr_partition_t* pmbr_part = (efi_pmbr_partition_t*)(data + 0x1be);

    if(pmbr_part->part_type == EFI_PMBR_PART_TYPE) {
        memory_free(data);

        dp->read(dp, 1, 1, &data);

        efi_partition_table_header_t* gpt_table = (efi_partition_table_header_t*)data;

        if(gpt_table->header.signature == EFI_PART_TABLE_HEADER_SIGNATURE) {
            memory_free(data);

            dp->read(dp, 1, 1, (uint8_t**)&gd->gpt_header);

            dp->read(dp, 2, gpt_parts_block_count, (uint8_t**)&gd->partitions);

            return 0;
        }

        memory_free(data);
    } else {
        memory_free(data);
    }


    data = memory_malloc_ext(0, block_size, 0x1000);

    pmbr_part = (efi_pmbr_partition_t*)(data + 0x1be);
    pmbr_part->first_chs.sector = 2;
    pmbr_part->part_type = EFI_PMBR_PART_TYPE;
    pmbr_part->last_chs.head = 0xFF;
    pmbr_part->last_chs.sector = 0xFF;
    pmbr_part->last_chs.cylinder = 0xFF;
    pmbr_part->first_lba = 1;

    if(sector_count > 0xFFFFFFFF) {
        pmbr_part->sector_count = 0xFFFFFFFF;
    } else {
        pmbr_part->sector_count = sector_count;
    }

    data[510] = 0x55;
    data[511] = 0xAA;

    dp->write(dp, 0, 1, data);

    memory_free(data);

    gd->gpt_header = memory_malloc_ext(0, block_size, 0x1000);
    gd->partitions = memory_malloc_ext(0, gpt_parts_block_count * block_size, 0x1000);

    efi_partition_table_header_t* gpt = (efi_partition_table_header_t*)gd->gpt_header;

    gpt->header.signature = EFI_PART_TABLE_HEADER_SIGNATURE;
    gpt->header.revision = EFI_PART_TABLE_HEADER_REVISION;
    gpt->header.header_size = sizeof(efi_partition_table_header_t);

    gpt->my_lba = 1;
    gpt->alternate_lba = max_lba;
    gpt->first_usable_lba = gpt->my_lba + gpt_parts_block_count + 1;
    gpt->last_usable_lba =  gpt->alternate_lba - gpt_parts_block_count - 1;
    efi_create_guid(&gpt->disk_guid);
    gpt->partition_entry_lba = 2;
    gpt->partition_entry_count = 128;
    gpt->partition_entry_size = sizeof(efi_partition_entry_t);

    gpt_write_gpt_metadata(d);

    return 0;
}

uint8_t gpt_add_partition(const disk_t* d, disk_partition_context_t* part_ctx){

    efi_partition_entry_t* ic = ( efi_partition_entry_t*)part_ctx->internal_context;

    gpt_disk_t* gd = (gpt_disk_t*)d;

    efi_guid_t unused = EFI_PART_TYPE_UNUSED_GUID;

    for(uint32_t i = 0; i < gd->gpt_header->partition_entry_count; i++) {
        if(efi_guid_equal(unused, gd->partitions[i].partition_type_guid) == 0) {
            memory_memcopy(ic, gd->partitions + i, sizeof(efi_partition_entry_t));

            gpt_write_gpt_metadata(d);
            return (uint8_t)i;
        }
    }

    return -1;
}

int8_t gpt_del_partition(const disk_t* d, uint8_t partno){

    if(partno > 127) {
        return -1;
    }

    gpt_disk_t* gd = (gpt_disk_t*)d;

    memory_memclean(gd->partitions + partno, sizeof(efi_partition_entry_t));

    gpt_write_gpt_metadata(d);

    return 0;
}

disk_partition_context_t*  gpt_get_partition_context(const disk_t* d, uint8_t partno) {
    if(partno > 127) {
        return NULL;
    }

    gpt_disk_t* gd = (gpt_disk_t*)d;

    disk_partition_context_t* res = memory_malloc(sizeof(disk_partition_context_t));

    if(res == NULL) {
        return NULL;
    }

    res->internal_context = &gd->partitions[partno];
    res->start_lba = gd->partitions[partno].starting_lba;
    res->end_lba = gd->partitions[partno].ending_lba;

    return res;
}

int8_t gpt_part_iter_destroy(iterator_t* iter){
    if(iter == NULL) {
        return -1;
    }

    memory_free(iter->metadata);
    memory_free(iter);

    return 0;
}

iterator_t* gpt_part_iter_next(iterator_t* iter){
    if(iter == NULL) {
        return NULL;
    }

    gpt_parts_iter_metadata_t* md = iter->metadata;

    if(md == NULL) {
        return NULL;
    }

    md->current_part_no++;

    return iter;
}

int8_t gpt_part_iter_end_of_iterator(iterator_t* iter){
    if(iter == NULL) {
        return 0;
    }

    gpt_parts_iter_metadata_t* md = iter->metadata;

    if(md == NULL) {
        return 0;
    }

    if(md->current_part_no > md->disk->gpt_header->partition_entry_count) {
        return 0;
    }

    return -1;
}

const void* gpt_part_iter_get_item(iterator_t* iter){
    if(iter == NULL) {
        return NULL;
    }

    gpt_parts_iter_metadata_t* md = iter->metadata;

    if(md == NULL) {
        return NULL;
    }

    return gpt_get_partition_context((disk_t*)md->disk, md->current_part_no);
}

const void* gpt_part_iter_delete_item(iterator_t* iter){
    if(iter == NULL) {
        return NULL;
    }

    gpt_parts_iter_metadata_t* md = iter->metadata;

    if(md == NULL) {
        return NULL;
    }

    disk_partition_context_t* tmp = gpt_get_partition_context((disk_t*)md->disk, md->current_part_no);
    gpt_del_partition((disk_t*)md->disk, md->current_part_no);

    return tmp;
}

iterator_t* gpt_get_partition_contexts(const disk_t* d){
    if(d == NULL) {
        return NULL;
    }

    iterator_t* iter = memory_malloc(sizeof(iterator_t));

    if(iter == NULL) {
        return NULL;
    }

    gpt_parts_iter_metadata_t* md = memory_malloc(sizeof(gpt_parts_iter_metadata_t));

    if(md == NULL) {
        memory_free(iter);

        return NULL;
    }

    md->disk = (gpt_disk_t*)d;
    md->current_part_no = 0;

    iter->metadata = md;
    iter->destroy = gpt_part_iter_destroy;
    iter->next = gpt_part_iter_next;
    iter->end_of_iterator = gpt_part_iter_end_of_iterator;
    iter->get_item = gpt_part_iter_get_item;
    iter->delete_item = gpt_part_iter_delete_item;

    return iter;
}

disk_partition_context_t* gpt_create_partition_context(efi_guid_t* type, const char_t* name, uint64_t start, uint64_t end){
    disk_partition_context_t* res = memory_malloc(sizeof(disk_partition_context_t));

    efi_partition_entry_t* ic = memory_malloc(sizeof(efi_partition_entry_t));

    memory_memcopy(type, &ic->partition_type_guid, sizeof(efi_guid_t));

    efi_create_guid(&ic->unique_partition_guid);

    ic->starting_lba = start;
    ic->ending_lba = end;

    wchar_t* pname = char_to_wchar(name);
    memory_memcopy(pname, ic->partition_name, strlen(name) * sizeof(wchar_t));
    memory_free(pname);

    res->internal_context = ic;
    res->start_lba = ic->starting_lba;
    res->end_lba = ic->ending_lba;

    return res;
}

disk_t* gpt_get_or_create_gpt_disk(disk_t* underlaying_disk){
    if(underlaying_disk == NULL) {
        return NULL;
    }

    gpt_disk_t* gd = memory_malloc(sizeof(gpt_disk_t));

    if(gd == NULL) {
        return NULL;
    }

    memory_memcopy(underlaying_disk, gd, sizeof(disk_t));

    gd->underlaying_disk.disk.close = gpt_disk_close;
    gd->underlaying_disk.add_partition = gpt_add_partition;
    gd->underlaying_disk.del_partition = gpt_del_partition;
    gd->underlaying_disk.get_partition_context = gpt_get_partition_context;
    gd->underlaying_disk.get_partition_contexts = gpt_get_partition_contexts;
    gd->underlaying_disk.get_partition = gpt_get_partition;
    gd->underlaying_disk.get_partition_by_type_data = gpt_get_partition_by_type_data;
    gd->underlaying_disk_pointer = (disk_or_partition_t*)underlaying_disk;

    disk_t* d = (disk_t*)gd;

    gpt_check_and_format_if_need(d);

    return d;
}

typedef struct disk_partition_ctx_t {
    const disk_t*                   disk;
    const disk_partition_context_t* ctx;
} disk_partition_ctx_t;

uint64_t                        disk_partition_get_size(const disk_or_partition_t* d);
uint64_t                        disk_partition_get_block_size(const disk_or_partition_t* d);
int8_t                          disk_partition_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data);
int8_t                          disk_partition_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data);
int8_t                          disk_partition_flush(const disk_or_partition_t* d);
int8_t                          disk_partition_close(const disk_or_partition_t* d);
const disk_partition_context_t* disk_partition_get_context(const disk_partition_t* p);
const disk_t*                   disk_partition_get_disk(const disk_partition_t* p);

disk_partition_t* gpt_get_partition(const disk_t* d, uint8_t partno) {
    gpt_disk_t* gd = (gpt_disk_t*) d;

    disk_partition_context_t* ctx = gd->underlaying_disk.get_partition_context(d, partno);

    if(!ctx) {
        return NULL;
    }

    disk_partition_ctx_t* dctx = memory_malloc(sizeof(disk_partition_ctx_t));

    if(!dctx) {
        memory_free(ctx);

        return NULL;
    }

    dctx->ctx = ctx;
    dctx->disk = d;

    disk_partition_t* res = memory_malloc(sizeof(disk_partition_t));

    if(!res) {
        memory_free(ctx);
        memory_free(dctx);

        return NULL;
    }

    res->partition.context = dctx;
    res->partition.close = disk_partition_close;
    res->partition.read = disk_partition_read;
    res->partition.write = disk_partition_write;
    res->partition.flush = disk_partition_flush;
    res->partition.get_size = disk_partition_get_size;
    res->partition.get_block_size = disk_partition_get_block_size;
    res->get_disk = disk_partition_get_disk;
    res->get_context = disk_partition_get_context;

    return res;
}


disk_partition_t* gpt_get_partition_by_type_data(const disk_t* d, const void* data) {
    gpt_disk_t* gd = (gpt_disk_t*) d;

    efi_guid_t* guid = (efi_guid_t*)data;

    iterator_t* it = gd->underlaying_disk.get_partition_contexts(d);

    if(!it) {
        return NULL;
    }

    disk_partition_context_t* ctx = NULL;


    while(it->end_of_iterator(it) != 0) {
        disk_partition_context_t* tmp_ctx = (disk_partition_context_t*)it->get_item(it);

        efi_partition_entry_t* entry = (efi_partition_entry_t*)tmp_ctx->internal_context;

        if(efi_guid_equal(entry->partition_type_guid, *guid) == 0) {
            ctx = tmp_ctx;

            break;
        }


        memory_free(tmp_ctx);

        it = it->next(it);
    }

    it->destroy(it);


    if(!ctx) {
        return NULL;
    }

    disk_partition_ctx_t* dctx = memory_malloc(sizeof(disk_partition_ctx_t));

    if(!dctx) {
        memory_free(ctx);

        return NULL;
    }

    dctx->ctx = ctx;
    dctx->disk = d;

    disk_partition_t* res = memory_malloc(sizeof(disk_partition_t));

    if(!res) {
        memory_free(ctx);
        memory_free(dctx);

        return NULL;
    }

    res->partition.context = dctx;
    res->partition.close = disk_partition_close;
    res->partition.read = disk_partition_read;
    res->partition.write = disk_partition_write;
    res->partition.flush = disk_partition_flush;
    res->partition.get_size = disk_partition_get_size;
    res->partition.get_block_size = disk_partition_get_block_size;
    res->get_disk = disk_partition_get_disk;
    res->get_context = disk_partition_get_context;

    return res;
}

const disk_partition_context_t* disk_partition_get_context(const disk_partition_t* p) {
    if(!p) {
        return NULL;
    }

    disk_partition_ctx_t* dctx = p->partition.context;

    return dctx->ctx;
}

const disk_t* disk_partition_get_disk(const disk_partition_t* p) {
    if(!p) {
        return NULL;
    }

    disk_partition_ctx_t* dctx = p->partition.context;

    return dctx->disk;
}

uint64_t disk_partition_get_size(const disk_or_partition_t* d) {
    if(!d) {
        return 0;
    }

    disk_partition_ctx_t* dctx = d->context;

    uint64_t bs = disk_partition_get_block_size(d);

    return bs * (dctx->ctx->end_lba - dctx->ctx->start_lba + 1);
}

uint64_t disk_partition_get_block_size(const disk_or_partition_t* d) {
    if(!d) {
        return 0;
    }

    disk_partition_ctx_t* dctx = d->context;
    uint64_t bs = ((disk_or_partition_t*)dctx->disk)->get_block_size((disk_or_partition_t*)dctx->disk);

    return bs;
}

int8_t disk_partition_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data) {
    if(!d) {
        return 0;
    }

    disk_partition_ctx_t* dctx = d->context;

    return dctx->disk->disk.write((disk_or_partition_t*)dctx->disk, dctx->ctx->start_lba + lba, count, data);
}

int8_t disk_partition_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data) {
    if(!d) {
        return 0;
    }

    disk_partition_ctx_t* dctx = d->context;

    return dctx->disk->disk.read((disk_or_partition_t*)dctx->disk, dctx->ctx->start_lba + lba, count, data);
}


int8_t disk_partition_flush(const disk_or_partition_t* d) {
    if(!d) {
        return 0;
    }

    disk_partition_ctx_t* dctx = d->context;

    return dctx->disk->disk.flush((disk_or_partition_t*)dctx->disk);
}

int8_t disk_partition_close(const disk_or_partition_t* d) {
    if(!d) {
        return 0;
    }

    disk_partition_ctx_t* dctx = d->context;

    memory_free((void*)dctx->ctx);
    memory_free(dctx);
    memory_free((void*)d);

    return 0;
}
