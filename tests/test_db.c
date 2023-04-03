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

int32_t main(uint32_t argc, char_t** argv);
int32_t test_step1(uint32_t argc, char_t** argv);
int32_t test_step2(uint32_t argc, char_t** argv);

data_t* record_create(uint64_t db_id, uint64_t table_id, uint64_t nr_args, ...);

data_t* record_create(uint64_t db_id, uint64_t table_id, uint64_t nr_args, ...) {
    va_list args;
    va_start(args, nr_args);

    data_t* items = memory_malloc(sizeof(data_t) * (nr_args + 2));

    if(items == NULL) {
        return NULL;
    }

    items[0].type = DATA_TYPE_INT64;
    items[0].value = (void*)db_id;

    items[1].type = DATA_TYPE_INT64;
    items[1].value = (void*)table_id;

    uint64_t tmp_int_value;
    float64_t tmp_float_value;

    for(uint64_t i = 0; i < nr_args; i++) {
        items[i + 2].type = va_arg(args, data_type_t);

        tmp_int_value = 0;
        tmp_float_value = 0;

        switch(items[i + 2].type) {
        case DATA_TYPE_BOOLEAN:
        case DATA_TYPE_CHAR:
        case DATA_TYPE_INT8:
        case DATA_TYPE_INT16:
        case DATA_TYPE_INT32:
            tmp_int_value = va_arg(args, int32_t);
            items[i + 2].value = (void*)tmp_int_value;
            break;
        case DATA_TYPE_INT64:
            items[i + 2].value = (void*)va_arg(args, int64_t);
            break;
        case DATA_TYPE_FLOAT32:
        case DATA_TYPE_FLOAT64:
            tmp_float_value = va_arg(args, float64_t);
            memory_memcopy(&tmp_float_value, &tmp_int_value, sizeof(float64_t));
            items[i + 2].value = (void*)tmp_int_value;
            break;
        case DATA_TYPE_STRING:
            items[i + 2].value = va_arg(args, void*);
            break;
        case DATA_TYPE_INT8_ARRAY:

            items[i + 2].value = va_arg(args, void*);
            break;

        default:
            items[i + 2].value = va_arg(args, void*);
            break;
        }

    }

    va_end(args);

    data_t* tmp =  memory_malloc(sizeof(data_t));

    if(tmp == NULL) {
        memory_free(items);

        return NULL;
    }

    tmp->type = DATA_TYPE_DATA;
    tmp->length = nr_args + 2;
    tmp->value = items;

    data_t* res = data_bson_serialize(tmp, DATA_SERIALIZE_WITH_FLAGS | DATA_SERIALIZE_WITH_TYPE | DATA_SERIALIZE_WITH_LENGTH);

    memory_free(items);

    memory_free(tmp);

    return res;
}

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

    tosdb_database_t* testdb = tosdb_database_create_or_open(tosdb, (char_t*)"testdb");

    if(!testdb) {
        print_error("cannot create/open testdb");
        pass = false;

        goto tdb_close;
    }


tdb_close:
    if(!tosdb_close(tosdb)) {
        print_error("cannot close tosdb");
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
        print_error("cannot create db buffer");
        return -1;
    }

    uint8_t* read_buf = memory_malloc(4 << 10);

    if(!read_buf) {
        buffer_destroy(db_buffer);
        print_error("cannot create read buffer");

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

    tosdb_database_t* testdb = tosdb_database_create_or_open(tosdb, (char_t*)"testdb");

    if(!testdb) {
        print_error("cannot create/open testdb");
        pass = false;

        goto tdb_close;
    }


tdb_close:
    if(!tosdb_close(tosdb)) {
        print_error("cannot close tosdb");
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

    return test_step2(argc, argv);
}
