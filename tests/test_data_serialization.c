/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <data.h>
#include <buffer.h>
#include <utils.h>
#include <strings.h>
#include <xxhash.h>

uint32_t main(uint32_t argc, char_t** argv);
int8_t   test_ser_deser_primitive(char_t* name, data_type_t dt, uint64_t datalen, void* data);
int8_t   test_ser_deser_datalist(void);
int8_t   test_ser_deser_recursive(void);

void print_hex(uint8_t* data, uint64_t len);
void print_hex(uint8_t* data, uint64_t len) {
    for(uint64_t i = 0; i < len; i++) {
        if(i % 16 == 0) {
            printf("\n");
        }

        if(i % 8 == 0) {
            printf("     ");
        }

        printf("0x%02x ", data[i]);
    }

    printf("\n");
}

int8_t test_ser_deser_primitive(char_t* name, data_type_t dt, uint64_t datalen, void* data) {
    uint64_t ihash = xxhash64_hash((void*)&data, datalen);

    data_t tmp_name = {DATA_TYPE_STRING, 0, NULL, name};
    data_t tmp_data = {dt, datalen, &tmp_name, data};

    data_t* ser_data = data_bson_serialize(&tmp_data);

    data_t* dser_data = data_bson_deserialize(ser_data);

    print_hex(ser_data->value, ser_data->length);

    memory_free(ser_data->value);
    memory_free(ser_data);

    if(dser_data == NULL) {
        print_error("TESTS FAILED, cannot deserialize");

        return -1;
    }

    if(dser_data->name == NULL) {
        print_error("TESTS FAILED name null");

        return -1;
    }

    if(strcmp((char_t*)dser_data->name->value, name) != 0) {
        print_error("TESTS FAILED name diff");

        return -1;
    }

    printf("name %s len %lli\n", (char_t*)dser_data->name->value, dser_data->name->length);

    uint64_t ohash = xxhash64_hash((void*)&dser_data->value, dser_data->length);

    if(dser_data->type != dt  || ihash != ohash) {
        print_error("TESTS FAILED dt/hash");
        printf("ds type %i val 0x%llx\n", dser_data->type, (uint64_t)dser_data->value);

        return -1;
    }

    if(dser_data->name) {
        if(dser_data->name->type >= DATA_TYPE_STRING) {
            memory_free(dser_data->name->value);
            memory_free(dser_data->name);
        }
    }

    if(dser_data->type >= DATA_TYPE_STRING) { // FIXME: DATA_TYPE_DATA memory leak
        memory_free(dser_data->value);
    }

    memory_free(dser_data);

    return 0;
}

int8_t test_ser_deser_datalist(void) {


    data_t tmp_data_lvl2 = {DATA_TYPE_INT32, 0, NULL, (void*)0x12345ULL};
    data_t tmp_data_lvl1 = {DATA_TYPE_DATA, 1, NULL, &tmp_data_lvl2};
    data_t tmp_data_lvl0 = {DATA_TYPE_DATA, 1, NULL, &tmp_data_lvl1};

    data_t* ser_data = data_bson_serialize(&tmp_data_lvl0);

    if(ser_data == NULL) {
        print_error("TESTS FAILED cannot ser");

        return -1;
    }

    print_hex(ser_data->value, ser_data->length);

    data_t* dser_data = data_bson_deserialize(ser_data);

    memory_free(ser_data->value);
    memory_free(ser_data);

    if(dser_data == NULL) {
        print_error("TESTS FAILED cannot ser");

        return -1;
    }

    if(dser_data->type != DATA_TYPE_DATA || dser_data->length != 1 || dser_data->value == NULL) {
        print_error("TESTS FAILED lvl0 dt/length/value mismatch");

        return -1;
    }

    data_t* res_tmp_data_lvl1 = dser_data->value;

    if(res_tmp_data_lvl1->type != DATA_TYPE_DATA || res_tmp_data_lvl1->length != 1 || res_tmp_data_lvl1->value == NULL) {
        print_error("TESTS FAILED lvl1 dt/length/value mismatch");
        printf("lvl1 %i %lli\n", res_tmp_data_lvl1->type, res_tmp_data_lvl1->length);

        return -1;
    }

    data_t* res_tmp_data_lvl2 = res_tmp_data_lvl1->value;

    if(res_tmp_data_lvl2->type != DATA_TYPE_INT32 || res_tmp_data_lvl2->value != (void*)0x12345ULL) {
        print_error("TESTS FAILED lvl2 dt/value mismatch");
        printf("lvl2 %i %lli 0x%llx\n", res_tmp_data_lvl2->type, res_tmp_data_lvl2->length, (uint64_t)res_tmp_data_lvl2->value);

        return -1;
    }

    memory_free(res_tmp_data_lvl2);
    memory_free(res_tmp_data_lvl1);
    memory_free(dser_data);

    return 0;
}

int8_t test_ser_deser_recursive(void) {
    data_t* data_list = memory_malloc(sizeof(data_t) * 10);

    if(data_list == NULL) {
        return -1;
    }

    for(int16_t i = 0; i < 10; i++) {
        data_list[i].type = DATA_TYPE_INT32;
        data_list[i].value = (void*)(i * 31ULL + 0x1234);
    }

    data_t tmp_data = {DATA_TYPE_DATA, 10, NULL, data_list};

    data_t* ser_data = data_bson_serialize(&tmp_data);

    if(ser_data == NULL) {
        print_error("TESTS FAILED cannot ser");

        return -1;
    }

    print_hex(ser_data->value, ser_data->length);

    memory_free(data_list);

    data_t* dser_data = data_bson_deserialize(ser_data);

    memory_free(ser_data->value);
    memory_free(ser_data);

    if(dser_data == NULL) {
        print_error("TESTS FAILED cannot de ser");

        return -1;
    }

    if(dser_data->type != DATA_TYPE_DATA) {
        print_error("TESTS FAILED incorrect dt");

        return -1;
    }

    if(dser_data->length != 10) {
        print_error("TESTS FAILED length dismatch");

        return -1;
    }

    if(dser_data->value == NULL) {
        print_error("TESTS FAILED value null");

        return -1;
    }

    data_list = dser_data->value;

    for(int16_t i = 0; i < 10; i++) {
        if(data_list[i].type != DATA_TYPE_INT32) {
            print_error("TESTS FAILED list item dt mismatch");

            return -1;
        }

        if(data_list[i].value != (void*)(i * 31ULL + 0x1234)) {
            print_error("TESTS FAILED list item value mismatch");

            return -1;
        }
    }


    memory_free(dser_data->value);
    memory_free(dser_data);

    return 0;
}

uint32_t main(uint32_t argc, char_t** argv) {

    UNUSED(argc);
    UNUSED(argv);

    uint64_t res = 0;

    res += test_ser_deser_primitive((char_t*)"test-int16", DATA_TYPE_INT16, sizeof(uint16_t), (void*)0x1254);
    res += test_ser_deser_primitive((char_t*)"test-of-int-32", DATA_TYPE_INT64, sizeof(uint64_t), (void*)0x1254369856ULL);

    float32_t test = 1234.56789;

    res += test_ser_deser_primitive((char_t*)"single floating point test", DATA_TYPE_FLOAT32, sizeof(float32_t), &test);

    res += test_ser_deser_datalist();

    res += test_ser_deser_recursive();

    if(res == 0) {
        print_success("TESTS PASSED");
    }

    return 0;
}
