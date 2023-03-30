/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x800000
#include "setup.h"
#include <data.h>
#include <buffer.h>
#include <utils.h>
#include <strings.h>
#include <xxhash.h>


int32_t main(int32_t argc, char_t** argv) {

    if(argc <= 1) {
        print_error("invalid argument count");

        return -1;
    }

    argc--;
    argv++;

    char_t* kernel = NULL;
    char_t* kernelp = NULL;
    char_t* output = NULL;

    while(argc) {
        if(strcmp(*argv, "-k") == 0) {
            argc--;
            argv++;

            kernel = *argv;

            argc--;
            argv++;
        } else if(strcmp(*argv, "-o") == 0) {
            argc--;
            argv++;

            output = *argv;

            argc--;
            argv++;
        } else if(strcmp(*argv, "-kp") == 0) {
            argc--;
            argv++;

            kernelp = *argv;

            argc--;
            argv++;
        } else {
            print_error("invalid argument");

            return -1;
        }
    }

    FILE* kin = fopen(kernelp, "r");

    if(kin == NULL) {
        print_error("cannot open kernel");

        return -1;
    }

    fseek(kin, 0, SEEK_END);
    int64_t kernel_size   = ftell(kin);
    fclose(kin);

    data_t kernel_name = {DATA_TYPE_STRING, 0, NULL, (char_t*)"kernel"};
    data_t kernel_data = {DATA_TYPE_STRING, 0, &kernel_name, kernel};

    data_t kernel_size_name = {DATA_TYPE_STRING, 0, NULL, (char_t*)"kernel-size"};
    data_t kernel_size_data = {DATA_TYPE_INT64, 0, &kernel_size_name, (void*)kernel_size};

    data_t pxeconf[] = {kernel_data, kernel_size_data};

    data_t pxeconf_name = {DATA_TYPE_STRING, 0, NULL, (char_t*)"pxe-config"};
    data_t pxeconf_data = {DATA_TYPE_DATA, 2, &pxeconf_name, pxeconf};

    printf("kernel %s path %s size: 0x%lx\n", kernel, kernelp, kernel_size);

    data_t* ser_data = data_bson_serialize(&pxeconf_data, DATA_SERIALIZE_WITH_ALL);

    FILE* pco = fopen(output, "w");

    if(pco == NULL) {
        print_error("cannot open output");

        return -1;
    }

    fwrite(ser_data->value, ser_data->length, 1, pco);
    fclose(pco);

    return 0;
}
