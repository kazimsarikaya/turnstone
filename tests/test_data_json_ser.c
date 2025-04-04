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

static int8_t test_ser_deser_primitive(const char_t* name, data_type_t dt, uint64_t datalen, const void* data) {
    data_t tmp_name = {DATA_TYPE_STRING, 0, NULL, (void*)name};
    data_t tmp_data = {dt, datalen, &tmp_name, (void*)data};

    data_t* ser_data = data_json_serialize(&tmp_data);

    if(ser_data == NULL) {
        print_error("TESTS FAILED, cannot serialize");

        return -1;
    }

    printf("Serialized data: %s\n", (char_t*)ser_data->value);

    data_free(ser_data);


    return 0;
}



uint32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);
    int8_t ret = 0;

    ret = test_ser_deser_primitive("test", DATA_TYPE_STRING, 4, "test");
    if(ret != 0) {
        print_error("TESTS FAILED");

        return -1;
    }

    ret = test_ser_deser_primitive("test", DATA_TYPE_INT8_ARRAY, 8, "ABCDEFGH");
    if(ret != 0) {
        print_error("TESTS FAILED");

        return -1;
    }

    ret = test_ser_deser_primitive("test", DATA_TYPE_STRING, 19, "multi\nline\nstring");
    if(ret != 0) {
        print_error("TESTS FAILED");

        return -1;
    }

    ret = test_ser_deser_primitive("test", DATA_TYPE_INT32, 0, (void*)42);
    if(ret != 0) {
        print_error("TESTS FAILED");

        return -1;
    }

    float64_t pi = 3.14159265359;
    float64_t* piptr = &pi;
    uint64_t piptradr = (uint64_t)piptr;
    uint64_t picast = *((uint64_t*)piptradr);
    ret = test_ser_deser_primitive("test", DATA_TYPE_FLOAT64, 0, (void*)picast);
    if(ret != 0) {
        print_error("TESTS FAILED");

        return -1;
    }

    return 0;
}
