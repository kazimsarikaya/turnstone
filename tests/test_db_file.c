/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#define RAMSIZE (128 << 20)
#include "setup.h"
#include <crc.h>
#include <disk.h>
#include <efi.h>
#include <fat.h>
#include <strings.h>
#include <utils.h>
#include <random.h>
#include <linkedlist.h>
#include <time.h>
#include <utils.h>
#include <buffer.h>
#include <data.h>
#include <sha2.h>
#include <bplustree.h>
#include <map.h>
#include <xxhash.h>
#include <tosdb/tosdb.h>
#include <strings.h>
#include <bloomfilter.h>
#include <math.h>
#include <zpack.h>
#include <binarysearch.h>
#include <tokenizer.h>
#include <set.h>
#include <tosdb/tosdb_cache.h>
#include <cache.h>
#include <hashmap.h>
#include <rbtree.h>

#define TOSDB_CAP (32 << 20)

int32_t main(uint32_t argc, char_t** argv);


typedef struct disk_file_context_t {
    FILE*    fp_disk;
    uint64_t file_size;
    uint64_t block_size;
} disk_file_context_t;

uint64_t disk_file_get_disk_size(const disk_or_partition_t* d);
uint64_t disk_file_get_block_size(const disk_or_partition_t* d);
int8_t   disk_file_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data);
int8_t   disk_file_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data);
int8_t   disk_file_close(const disk_or_partition_t* d);
int8_t   disk_file_flush(const disk_or_partition_t* d);
disk_t*  disk_file_open(const char_t* file_name, int64_t size);

uint64_t disk_file_get_disk_size(const disk_or_partition_t* d){
    disk_file_context_t* ctx = (disk_file_context_t*)d->context;
    return ctx->file_size;
}

uint64_t disk_file_get_block_size(const disk_or_partition_t* d){
    disk_file_context_t* ctx = (disk_file_context_t*)d->context;
    return ctx->block_size;
}

int8_t disk_file_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data) {
    disk_file_context_t* ctx = (disk_file_context_t*)d->context;

    fseek(ctx->fp_disk, lba * ctx->block_size, SEEK_SET);

    int64_t f_res = fwrite(data, count * ctx->block_size, 1, ctx->fp_disk);
    fflush(ctx->fp_disk);

    return f_res == 1?0:-1;
}

int8_t disk_file_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data){
    disk_file_context_t* ctx = (disk_file_context_t*)d->context;

    fseek(ctx->fp_disk, lba * ctx->block_size, SEEK_SET);

    *data = memory_malloc(count * ctx->block_size);
    fread(*data, count * ctx->block_size, 1, ctx->fp_disk);

    return 0;
}

int8_t disk_file_flush(const disk_or_partition_t* d) {
    disk_file_context_t* ctx = (disk_file_context_t*)d->context;

    fflush(ctx->fp_disk);

    return 0;
}

int8_t disk_file_close(const disk_or_partition_t* d) {
    disk_file_context_t* ctx = (disk_file_context_t*)d->context;
    fclose(ctx->fp_disk);

    memory_free(ctx);

    memory_free((void*)d);

    return 0;
}

disk_t* disk_file_open(const char_t* file_name, int64_t size) {

    FILE* fp_disk;

    if(size != -1) {
        fp_disk = fopen(file_name, "w");
        uint8_t data = 0;
        fseek(fp_disk, size - 1, SEEK_SET);
        fwrite(&data, 1, 1, fp_disk);
        fclose(fp_disk);
    }

    fp_disk = fopen(file_name, "r+");
    fseek(fp_disk, 0, SEEK_END);
    size = ftell(fp_disk);

    disk_file_context_t* ctx = memory_malloc(sizeof(disk_file_context_t));

    if(ctx == NULL) {
        fclose(fp_disk);

        return NULL;
    }

    ctx->fp_disk = fp_disk;
    ctx->file_size = size;
    ctx->block_size = 512;

    disk_t* d = memory_malloc(sizeof(disk_t));

    if(d == NULL) {
        memory_free(ctx);
        fclose(fp_disk);

        return NULL;
    }

    d->disk.context = ctx;
    d->disk.get_size = disk_file_get_disk_size;
    d->disk.get_block_size = disk_file_get_block_size;
    d->disk.write = disk_file_write;
    d->disk.read = disk_file_read;
    d->disk.close = disk_file_close;
    d->disk.flush = disk_file_flush;

    return d;
}


int32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    crc32_init_table();

    const char_t* disk_name = "tmp/tosdb-disk.img";

    disk_t* d = disk_file_open(disk_name, 1 << 30);


    d = gpt_get_or_create_gpt_disk(d);


    disk_partition_context_t* part_ctx;

    efi_guid_t esp_guid = EFI_PART_TYPE_EFI_SYSTEM_PART_GUID;
    part_ctx = gpt_create_partition_context(&esp_guid, "efi", 2048, 206847);
    d->add_partition(d, part_ctx);
    memory_free(part_ctx->internal_context);
    memory_free(part_ctx);


    efi_guid_t kernel_guid = EFI_PART_TYPE_TURNSTONE_KERNEL_PART_GUID;
    part_ctx = gpt_create_partition_context(&kernel_guid, "kernel", 206848, 206848 + (TOSDB_CAP / 512) - 1);
    d->add_partition(d, part_ctx);
    memory_free(part_ctx->internal_context);
    memory_free(part_ctx);

    disk_or_partition_t* part = (disk_or_partition_t*)d->get_partition(d, 1);
    disk_or_partition_t* disk = (disk_or_partition_t*)d;

    boolean_t pass = true;

    tosdb_backend_t* backend = tosdb_backend_disk_new(part);

    if(!backend) {
        print_error("cannot create backend");
        pass = false;

        goto backend_failed;
    }

    tosdb_t* tosdb = tosdb_new(backend);

    if(!tosdb) {
        print_error("cannot create tosdb");
        pass = false;

        goto backend_close;
    }

    tosdb_cache_config_t cc = {0};
    cc.bloomfilter_size = 2 << 20;
    cc.index_data_size = 4 << 20;
    cc.secondary_index_data_size = 4 << 20;
    cc.valuelog_size = 16 << 20;

    if(!tosdb_cache_config_set(tosdb, &cc)) {
        print_error("cannot create tosdb");
        pass = false;

        goto tdb_close;
    }

    tosdb_database_t* testdb = tosdb_database_create_or_open(tosdb, "testdb");

    if(!testdb) {
        print_error("cannot create/open testdb");
        pass = false;

        goto tdb_close;
    }

    tosdb_table_t* table2 = tosdb_table_create_or_open(testdb, "table2", 1 << 10, 128 << 10, 8);

    if(!table2) {
        print_error("cannot create/open table2");
        pass = false;

        goto tdb_close;
    }

    if(!tosdb_table_column_add(table2, "id", DATA_TYPE_INT64)) {
        print_error("cannot add id column to table2");
        pass = false;

        goto tdb_close;
    }

    if(!tosdb_table_column_add(table2, "fname", DATA_TYPE_STRING)) {
        print_error("cannot add fname column to table2");
        pass = false;

        goto tdb_close;
    }

    if(!tosdb_table_column_add(table2, "sname", DATA_TYPE_STRING)) {
        print_error("cannot add sname column to table2");
        pass = false;

        goto tdb_close;
    }

    if(!tosdb_table_column_add(table2, "country", DATA_TYPE_STRING)) {
        print_error("cannot add age column to table2");
        pass = false;

        goto tdb_close;
    }

    if(!tosdb_table_index_create(table2, "id", TOSDB_INDEX_PRIMARY)) {
        print_error("cannot add index for id to table2");
        pass = false;

        goto tdb_close;
    }

    if(!tosdb_table_index_create(table2, "country", TOSDB_INDEX_SECONDARY)) {
        print_error("cannot add index for ssn to table2");
        pass = false;

        goto tdb_close;
    }

    FILE* in = fopen("tmp/data.csv", "r");

    if(!in) {
        print_error("cannot open csv file");
        pass = false;

        goto tdb_close;
    }

    fseek(in, 0, SEEK_END);
    uint64_t csv_cap = ftell(in);
    fseek(in, 0, SEEK_SET);

    buffer_t csv_buffer = buffer_new_with_capacity(NULL, csv_cap);

    if(!csv_buffer) {
        fclose(in);
        print_error("cannot create csv buffer");
        pass = false;

        goto tdb_close;
    }

    uint8_t* read_buf = memory_malloc(4 << 10);

    if(!read_buf) {
        part->close(part);
        disk->close(disk);
        print_error("cannot create read buffer");
        fclose(in);
        pass = false;

        goto tdb_close;
    }

    uint64_t total_read = 0;
    while(1) {
        uint64_t rc = fread(read_buf, 1, 4 << 10, in);
        if(rc == 0) {
            break;
        }

        total_read += rc;

        buffer_append_bytes(csv_buffer, read_buf, rc);
        memory_memclean(read_buf, 4 << 10);
    }

    memory_free(read_buf);

    fclose(in);

    if(total_read != csv_cap) {
        buffer_destroy(csv_buffer);
        print_error("cannot read csv file");
        printf("total read: %lli\n", total_read);
        pass = false;

        goto tdb_close;;
    }


    buffer_seek(csv_buffer, 0, BUFFER_SEEK_DIRECTION_START);

    token_delimiter_type_t delims[] = {
        TOKEN_DELIMETER_TYPE_COMMA,
        TOKEN_DELIMETER_TYPE_LF,
        TOKEN_DELIMETER_TYPE_NULL
    };

    iterator_t* tokenizer = tokenizer_new(csv_buffer, delims, delims);

    if(!tokenizer) {
        print_error("cannot create tokenizer");
        buffer_destroy(csv_buffer);
        pass = false;

        goto tdb_close;
    }

    while(tokenizer->end_of_iterator(tokenizer) != 0) {
        tosdb_record_t* rec = tosdb_table_create_record(table2);

        if(!rec) {
            print_error("cannot create rec");
            pass = false;

            goto token_error;
        }

        token_t* token = (token_t*)tokenizer->get_item(tokenizer);

        uint64_t id = atoi((const char_t*)token->value);

        printf("id: %lli ", id);

        rec->set_int64(rec, "id", id);

        memory_free(token);

        tokenizer = tokenizer->next(tokenizer);
        token = (token_t*)tokenizer->get_item(tokenizer);

        printf("%s ", token->value);

        rec->set_string(rec, "fname", (const char_t*)token->value);

        memory_free(token);

        tokenizer = tokenizer->next(tokenizer);
        token = (token_t*)tokenizer->get_item(tokenizer);

        printf("%s ", token->value);

        rec->set_string(rec, "sname", (const char_t*)token->value);

        memory_free(token);

        tokenizer = tokenizer->next(tokenizer);
        token = (token_t*)tokenizer->get_item(tokenizer);

        printf("%s\n", token->value);

        rec->set_string(rec, "country", (const char_t*)token->value);

        memory_free(token);

        rec->upsert_record(rec);
        rec->destroy(rec);

        tokenizer = tokenizer->next(tokenizer);
    }

token_error:
    tokenizer->destroy(tokenizer);

    buffer_destroy(csv_buffer);

    if(!pass) {
        goto tdb_close;
    }

    tosdb_table_close(table2);
    table2 = tosdb_table_create_or_open(testdb, "table2", 1 << 10, 128 << 10, 8);

    tosdb_record_t* s_rec = tosdb_table_create_record(table2);

    if(!s_rec) {
        print_error("cannot create key record");
        pass = false;

        goto tdb_close;
    }

    if(!s_rec->set_string(s_rec, "country", "Singapore")) {
        print_error("cannot set ssn for search");
        pass = false;

        goto rec_destroy;
    }

    linkedlist_t s_recs = s_rec->search_record(s_rec);

    iterator_t* iter = linkedlist_iterator_create(s_recs);

    if(!iter) {
        print_error("cannot create search iterator");
        pass = false;

        goto rec_destroy;
    }

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_record_t* res_rec = (tosdb_record_t*)iter->delete_item(iter);

        if(!res_rec) {
            print_error("cannot get one of search result");
            pass = false;

            break;
        }

        int64_t id = 0;

        if(!res_rec->get_int64(res_rec, "id", &id)) {
            print_error("cannot get id");
            pass = false;

            goto search_iter_destroy;
        }

        char_t* fname = NULL;

        if(!res_rec->get_string(res_rec, "fname", &fname)) {
            print_error("cannot get fname");
            pass = false;

            goto search_iter_destroy;
        }

        char_t* country = NULL;

        if(!res_rec->get_string(res_rec, "country", &country)) {
            print_error("cannot get country");
            pass = false;

            goto search_iter_destroy;
        }

        if(id == 22) {
            res_rec->set_string(res_rec, "country", "Colombia");
            res_rec->upsert_record(res_rec);
        }

        printf("%lli %s %s\n", id, fname, country);

        memory_free(fname);
        memory_free(country);

        res_rec->destroy(res_rec);

        iter = iter->next(iter);
    }

search_iter_destroy:
    iter->destroy(iter);
    linkedlist_destroy(s_recs);

rec_destroy:

    s_rec->destroy(s_rec);

    tosdb_record_t* d_rec = tosdb_table_create_record(table2);

    d_rec->set_int64(d_rec, "id", 52);

    if(!d_rec->delete_record(d_rec)) {
        print_error("cannot delete record");
        pass = false;
    }

    d_rec->destroy(d_rec);

    if(!pass) {
        goto tdb_close;
    }


    set_t* pks = tosdb_table_get_primary_keys(table2);

    if(!pks) {
        print_error("cannot get pks");
        pass = false;

        goto tdb_close;
    }

    int64_t rec_cnt = 0;

    iter = set_create_iterator(pks);

    while(iter->end_of_iterator(iter) != 0) {
        d_rec = (tosdb_record_t*)iter->get_item(iter);

        int64_t id = 0;

        if(d_rec->get_int64(d_rec, "id", &id)) {
            //printf("pk id %lli\n", id);
            rec_cnt++;
        }

        d_rec->destroy(d_rec);

        iter = iter->next(iter);
    }

    iter->destroy(iter);
    set_destroy(pks);

    if(rec_cnt != 100) {
        print_error("pk count failed");
        pass = false;
    }

    logging_module_levels[TOSDB] = LOG_DEBUG;

    tosdb_table_close(table2);
    table2 = tosdb_table_create_or_open(testdb, "table2", 1 << 10, 128 << 10, 8);


    tosdb_compact(tosdb, TOSDB_COMPACTION_TYPE_MAJOR);

tdb_close:
    if(!tosdb_close(tosdb)) {
        print_error("cannot close tosdb");
        pass = false;
    }

    if(!tosdb_free(tosdb)) {
        print_error("cannot free tosdb");
        pass = false;

        goto backend_close;
    }

backend_close:
    if(!tosdb_backend_close(backend)) {
        print_error("cannot close backend");
        pass = false;
    }

    part->close(part);
    disk->close(disk);

backend_failed:
    if(pass) {
        print_success("TESTS PASSED");
    } else {
        print_error("TESTS FAILED");
    }

    return pass?0:-1;
}
