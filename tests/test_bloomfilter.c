/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <math.h>
#include <xxhash.h>
#include <memory.h>
#include <utils.h>
#include <bloomfilter.h>
#include <random.h>
#include <strings.h>
#include <buffer.h>

int32_t main(uint32_t argc, char_t** argv);

int32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    boolean_t pass = true;

    bloomfilter_t* bf = bloomfilter_new(100, 0.80);

    if(!bf) {
        print_error("cannot create bloom filter");
    }

    data_t d1;
    d1.type = DATA_TYPE_INT8_ARRAY;
    d1.value = (uint8_t*)"hello world";
    d1.length = strlen(d1.value);

    data_t d2;
    d2.type = DATA_TYPE_INT8_ARRAY;
    d2.value = (uint8_t*)"bye world";
    d2.length = strlen(d2.value);

    bloomfilter_add(bf, &d1);

    if(!bloomfilter_check(bf, &d1)) {
        print_error("cannot find added value");
        pass = false;
    }

    if(bloomfilter_check(bf, &d2)) {
        print_error("value should not be in bf");
        pass = false;
    }

    data_t* sbf = bloomfilter_serialize(bf);

    if(!bf) {
        print_error("cannot serialize bloom filter");
        pass = false;
    }

    bloomfilter_t* bf2 = bloomfilter_deserialize(sbf);

    if(bf2) {
        if(!bloomfilter_check(bf2, &d1)) {
            print_error("cannot find value at deserialized bf");
            pass = false;
        }

        bloomfilter_destroy(bf2);
    } else {
        print_error("cannot deserialize bf");
        pass = false;
    }

    memory_free(sbf->value);
    memory_free(sbf);

    bloomfilter_destroy(bf);

    if(pass) {
        print_success("TESTS PASSED");
    } else {
        print_error("TESTS FAILED");
    }

    return 0;
}
