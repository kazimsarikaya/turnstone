/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE (128 << 20)
#include "setup.h"
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
#include <cache.h>
#include <rbtree.h>

int32_t main(uint32_t argc, char_t** argv);
int32_t test_step1(uint32_t argc, char_t** argv);
int32_t test_step2(uint32_t argc, char_t** argv);
int32_t test_step3(uint32_t argc, char_t** argv);
int32_t test_step4(uint32_t argc, char_t** argv);


#define TOSDB_CAP (32 << 20)

int32_t test_step1(uint32_t argc, char_t** argv) {
    char_t* tosdb_out_file_name = (char_t*)"./tmp/tosdb.img";

    if(argc == 2) {
        tosdb_out_file_name = argv[1];
    }

    boolean_t pass = true;

    tosdb_backend_t* backend = tosdb_backend_memory_new(TOSDB_CAP);

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

    tosdb_table_t* table1 = tosdb_table_create_or_open(testdb, "table1", 1 << 10, 128 << 10, 8);

    if(!table1) {
        print_error("cannot create/open table1");
        pass = false;

        goto tdb_close;
    }

    if(!tosdb_table_column_add(table1, "id", DATA_TYPE_INT64)) {
        print_error("cannot add id column to table1");
        pass = false;

        goto tdb_close;
    }

    if(!tosdb_table_column_add(table1, "ssn", DATA_TYPE_STRING)) {
        print_error("cannot add ssn column to table1");
        pass = false;

        goto tdb_close;
    }

    if(!tosdb_table_column_add(table1, "fname", DATA_TYPE_STRING)) {
        print_error("cannot add fname column to table1");
        pass = false;

        goto tdb_close;
    }

    if(!tosdb_table_column_add(table1, "sname", DATA_TYPE_STRING)) {
        print_error("cannot add sname column to table1");
        pass = false;

        goto tdb_close;
    }

    if(!tosdb_table_column_add(table1, "age", DATA_TYPE_INT8)) {
        print_error("cannot add age column to table1");
        pass = false;

        goto tdb_close;
    }

    if(!tosdb_table_index_create(table1, "id", TOSDB_INDEX_PRIMARY)) {
        print_error("cannot add index for id to table1");
        pass = false;

        goto tdb_close;
    }

    if(!tosdb_table_index_create(table1, "ssn", TOSDB_INDEX_UNIQUE)) {
        print_error("cannot add index for ssn to table1");
        pass = false;

        goto tdb_close;
    }

    if(!tosdb_database_close(testdb)) {
        print_error("cannot colse testdb");
        pass = false;

        goto tdb_close;

    }

    testdb = tosdb_database_create_or_open(tosdb, "testdb");

    if(!testdb) {
        print_error("cannot re-open testdb");
        pass = false;

        goto tdb_close;
    }

    table1 = tosdb_table_create_or_open(testdb, "table1", 1 << 10, 128 << 10, 8);

    if(!table1) {
        print_error("cannot re-open table1");
        pass = false;

        goto tdb_close;
    }


    tosdb_record_t * rec = tosdb_table_create_record(table1);

    if(!rec) {
        print_error("cannot create rec for table1");
        pass = false;

        goto tdb_close;
    }

    if(!rec->set_int64(rec, "id", 1)) {
        print_error("cannot set id column for rec of table1");
        pass = false;

        goto rec_destroy;
    }

    if(!rec->set_string(rec, "ssn", "123456")) {
        print_error("cannot set ssn column for rec of table1");
        pass = false;

        goto rec_destroy;
    }

    if(!rec->set_string(rec, "fname", "joe")) {
        print_error("cannot set fname column for rec of table1");
        pass = false;

        goto rec_destroy;
    }

    if(!rec->set_string(rec, "sname", "doe")) {
        print_error("cannot set sname column for rec of table1");
        pass = false;

        goto rec_destroy;
    }

    if(!rec->set_int8(rec, "age", 37)) {
        print_error("cannot set age column for rec of table1");
        pass = false;

        goto rec_destroy;
    }

    if(!rec->upsert_record(rec)) {
        print_error("cannot insert rec to table1");
        pass = false;

        goto rec_destroy;
    }

rec_destroy:
    rec->destroy(rec);

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

    uint8_t* out_data = tosdb_backend_memory_get_contents(backend);

    if(!out_data) {
        print_error("cannot get backend data");
        pass = false;

        goto backend_close;
    }

    FILE* out = fopen(tosdb_out_file_name, "w");
    if(!out) {
        print_error("cannot open output file");
        pass = false;

        goto backend_close;
    }

    fwrite(out_data, 1, TOSDB_CAP, out);

    memory_free(out_data);

    fclose(out);

backend_close:
    if(!tosdb_backend_close(backend)) {
        pass = false;
    }

backend_failed:
    if(pass) {
        print_success("TESTS PASSED");
    } else {
        print_error("TESTS FAILED");
    }
    return pass?0:-1;
}

int32_t test_step2(uint32_t argc, char_t** argv) {
    char_t* tosdb_out_file_name = (char_t*)"./tmp/tosdb.img";

    if(argc == 2) {
        tosdb_out_file_name = argv[1];
    }

    FILE* in = fopen(tosdb_out_file_name, "r");

    if(!in) {
        print_error("cannot open tosdb file");

        return -1;
    }

    buffer_t db_buffer = buffer_new_with_capacity(NULL, TOSDB_CAP);

    if(!db_buffer) {
        fclose(in);
        print_error("cannot create db buffer");
        return -1;
    }

    uint8_t* read_buf = memory_malloc(4 << 10);

    if(!read_buf) {
        buffer_destroy(db_buffer);
        print_error("cannot create read buffer");
        fclose(in);

        return -1;
    }

    uint64_t total_read = 0;
    while(1) {
        uint64_t rc = fread(read_buf, 1, 4 << 10, in);
        if(rc == 0) {
            break;
        }

        total_read += rc;

        buffer_append_bytes(db_buffer, read_buf, 4 << 10);
        memory_memclean(read_buf, 4 << 10);
    }

    memory_free(read_buf);

    fclose(in);

    if(total_read != TOSDB_CAP) {
        buffer_destroy(db_buffer);
        print_error("cannot read db file");
        printf("total read: %lli\n", total_read);

        return -1;
    }

    boolean_t pass = true;

    tosdb_backend_t* backend = tosdb_backend_memory_from_buffer(db_buffer);

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

    tosdb_table_t* table1 = tosdb_table_create_or_open(testdb, "table1", 1 << 10, 128 << 10, 8);

    if(!table1) {
        print_error("cannot create/open table1");
        pass = false;

        goto tdb_close;
    }

    tosdb_record_t * rec = tosdb_table_create_record(table1);

    if(!rec) {
        print_error("cannot create rec for table1");
        pass = false;

        goto tdb_close;
    }

    if(!rec->set_int64(rec, "id", 2)) {
        print_error("cannot set id column for rec of table1");
        pass = false;

        goto rec_destroy;
    }

    if(!rec->set_string(rec, "ssn", "123457")) {
        print_error("cannot set ssn column for rec of table1");
        pass = false;

        goto rec_destroy;
    }

    if(!rec->set_string(rec, "fname", "john")) {
        print_error("cannot set fname column for rec of table1");
        pass = false;

        goto rec_destroy;
    }

    if(!rec->set_string(rec, "sname", "wick")) {
        print_error("cannot set sname column for rec of table1");
        pass = false;

        goto rec_destroy;
    }

    if(!rec->set_int8(rec, "age", 43)) {
        print_error("cannot set age column for rec of table1");
        pass = false;

        goto rec_destroy;
    }

    if(!rec->upsert_record(rec)) {
        print_error("cannot insert rec to table1");
        pass = false;

        goto rec_destroy;
    }

    rec->destroy(rec);

    rec = tosdb_table_create_record(table1);

    if(!rec) {
        print_error("cannot create key record");
        pass = false;

        goto tdb_close;
    }

    if(!rec->set_int64(rec, "id", 2)) {
        print_error("cannot set ssn for search");
        pass = false;

        goto rec_destroy;
    }

    if(!rec->get_record(rec)) {
        print_error("cannot find record at memtable");
        pass = false;

        goto rec_destroy;
    }

    char_t* fname = NULL;

    if(!rec->get_string(rec, "fname", &fname)) {
        print_error("cannot get fname");
        pass = false;

        goto rec_destroy;
    }

    int8_t age;

    if(!rec->get_int8(rec, "age", &age)) {
        print_error("cannot get age");
        pass = false;
        memory_free(fname);

        goto rec_destroy;
    }

    printf("fname: %s age: %i\n", fname, age);

    if(!strcmp(fname, "john") && age != 43) {
        print_error("record values wrong");
        pass = false;
        memory_free(fname);

        goto rec_destroy;
    }

    memory_free(fname);

    rec->destroy(rec);

    rec = tosdb_table_create_record(table1);

    if(!rec) {
        print_error("cannot create key record");
        pass = false;

        goto tdb_close;
    }

    if(!rec->set_string(rec, "ssn", "123456")) {
        print_error("cannot set ssn for search");
        pass = false;

        goto rec_destroy;
    }

    if(!rec->get_record(rec)) {
        print_error("cannot find record at sstable");
        pass = false;

        goto rec_destroy;
    }

    fname = NULL;

    if(!rec->get_string(rec, "fname", &fname)) {
        print_error("cannot get fname");
        pass = false;

        goto rec_destroy;
    }

    age = 0;

    if(!rec->get_int8(rec, "age", &age)) {
        print_error("cannot get age");
        pass = false;
        memory_free(fname);

        goto rec_destroy;
    }

    printf("fname: %s age: %i\n", fname, age);

    if(!strcmp(fname, "joe") && age != 37) {
        print_error("record values wrong");
        pass = false;
        memory_free(fname);

        goto rec_destroy;
    }

    memory_free(fname);

rec_destroy:
    rec->destroy(rec);

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

    uint8_t* out_data = tosdb_backend_memory_get_contents(backend);

    if(!out_data) {
        print_error("cannot get backend data");
        pass = false;

        goto backend_close;
    }

    FILE* out = fopen(tosdb_out_file_name, "w");
    if(!out) {
        print_error("cannot open output file");
        pass = false;

        goto backend_close;
    }

    fwrite(out_data, 1, TOSDB_CAP, out);

    memory_free(out_data);

    fclose(out);

backend_close:
    if(!tosdb_backend_close(backend)) {
        pass = false;
    }

backend_failed:
    if(pass) {
        print_success("TESTS PASSED");
    } else {
        print_error("TESTS FAILED");
    }
    return pass?0:-1;
}

int32_t test_step3(uint32_t argc, char_t** argv) {
    char_t* tosdb_out_file_name = (char_t*)"./tmp/tosdb.img";

    if(argc == 2) {
        tosdb_out_file_name = argv[1];
    }

    FILE* in = fopen(tosdb_out_file_name, "r");

    if(!in) {
        print_error("cannot open tosdb file");

        return -1;
    }

    buffer_t db_buffer = buffer_new_with_capacity(NULL, TOSDB_CAP);

    if(!db_buffer) {
        fclose(in);
        print_error("cannot create db buffer");
        return -1;
    }

    uint8_t* read_buf = memory_malloc(4 << 10);

    if(!read_buf) {
        buffer_destroy(db_buffer);
        print_error("cannot create read buffer");
        fclose(in);

        return -1;
    }

    uint64_t total_read = 0;
    while(1) {
        uint64_t rc = fread(read_buf, 1, 4 << 10, in);
        if(rc == 0) {
            break;
        }

        total_read += rc;

        buffer_append_bytes(db_buffer, read_buf, 4 << 10);
        memory_memclean(read_buf, 4 << 10);
    }

    memory_free(read_buf);

    fclose(in);

    if(total_read != TOSDB_CAP) {
        buffer_destroy(db_buffer);
        print_error("cannot read db file");
        printf("total read: %lli\n", total_read);

        return -1;
    }

    boolean_t pass = true;

    tosdb_backend_t* backend = tosdb_backend_memory_from_buffer(db_buffer);

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

    in = fopen("tmp/data.csv", "r");

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

    read_buf = memory_malloc(4 << 10);

    if(!read_buf) {
        buffer_destroy(db_buffer);
        print_error("cannot create read buffer");
        fclose(in);
        pass = false;

        goto tdb_close;
    }

    total_read = 0;
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

    uint8_t* out_data = tosdb_backend_memory_get_contents(backend);

    if(!out_data) {
        print_error("cannot get backend data");
        pass = false;

        goto backend_close;
    }

    FILE* out = fopen(tosdb_out_file_name, "w");
    if(!out) {
        print_error("cannot open output file");
        pass = false;

        goto backend_close;
    }

    fwrite(out_data, 1, TOSDB_CAP, out);

    memory_free(out_data);

    fclose(out);

backend_close:
    if(!tosdb_backend_close(backend)) {
        pass = false;
    }

backend_failed:
    if(pass) {
        print_success("TESTS PASSED");
    } else {
        print_error("TESTS FAILED");
    }
    return pass?0:-1;
}

int32_t test_step4(uint32_t argc, char_t** argv) {
    char_t* tosdb_out_file_name = (char_t*)"./tmp/tosdb.img";

    if(argc == 2) {
        tosdb_out_file_name = argv[1];
    }

    FILE* in = fopen(tosdb_out_file_name, "r");

    if(!in) {
        print_error("cannot open tosdb file");

        return -1;
    }

    buffer_t db_buffer = buffer_new_with_capacity(NULL, TOSDB_CAP);

    if(!db_buffer) {
        fclose(in);
        print_error("cannot create db buffer");
        return -1;
    }

    uint8_t* read_buf = memory_malloc(4 << 10);

    if(!read_buf) {
        buffer_destroy(db_buffer);
        print_error("cannot create read buffer");
        fclose(in);

        return -1;
    }

    uint64_t total_read = 0;
    while(1) {
        uint64_t rc = fread(read_buf, 1, 4 << 10, in);
        if(rc == 0) {
            break;
        }

        total_read += rc;

        buffer_append_bytes(db_buffer, read_buf, 4 << 10);
        memory_memclean(read_buf, 4 << 10);
    }

    memory_free(read_buf);

    fclose(in);

    if(total_read != TOSDB_CAP) {
        buffer_destroy(db_buffer);
        print_error("cannot read db file");
        printf("total read: %lli\n", total_read);

        return -1;
    }

    boolean_t pass = true;

    tosdb_backend_t* backend = tosdb_backend_memory_from_buffer(db_buffer);

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

    tosdb_record_t* s_rec = tosdb_table_create_record(table2);

    if(!s_rec) {
        print_error("cannot create key record");
        pass = false;

        goto tdb_close;
    }

    if(!s_rec->set_string(s_rec, "country", "Colombia")) {
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

    pass = false;

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

        printf("%lli %s %s\n", id, fname, country);

        pass = true;

        memory_free(fname);
        memory_free(country);

        res_rec->destroy(res_rec);

        iter = iter->next(iter);
    }

    if(!pass) {
        print_error("cannot search by country on non unique key");
    }

search_iter_destroy:
    iter->destroy(iter);
    linkedlist_destroy(s_recs);

rec_destroy:

    s_rec->destroy(s_rec);

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

    uint8_t* out_data = tosdb_backend_memory_get_contents(backend);

    if(!out_data) {
        print_error("cannot get backend data");
        pass = false;

        goto backend_close;
    }

    FILE* out = fopen(tosdb_out_file_name, "w");
    if(!out) {
        print_error("cannot open output file");
        pass = false;

        goto backend_close;
    }

    fwrite(out_data, 1, TOSDB_CAP, out);

    memory_free(out_data);

    fclose(out);

backend_close:
    if(!tosdb_backend_close(backend)) {
        pass = false;
    }

backend_failed:
    if(pass) {
        print_success("TESTS PASSED");
    } else {
        print_error("TESTS FAILED");
    }
    return pass?0:-1;
}

int32_t main(uint32_t argc, char_t** argv) {
    if(test_step1(argc, argv) != 0) {
        print_error("test step 1 failed");

        return -1;
    }

    if(test_step2(argc, argv) != 0) {
        print_error("test step 2 failed");

        return -1;
    }

    if(test_step3(argc, argv) != 0) {
        print_error("test step 3 failed");

        return -1;
    }

    if(test_step4(argc, argv) != 0) {
        print_error("test step 4 failed");

        return -1;
    }

    return 0;
}
