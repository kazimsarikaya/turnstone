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

    char_t* tosdb = NULL;
    char_t* tosdbp = NULL;
    char_t* output = NULL;

    while(argc) {
        if(strcmp(*argv, "-dbn") == 0 || strcmp(*argv, "--tosdb-file-name") == 0) {
            argc--;
            argv++;

            tosdb = *argv;

            argc--;
            argv++;
        } else if(strcmp(*argv, "-o") == 0) {
            argc--;
            argv++;

            output = *argv;

            argc--;
            argv++;
        } else if(strcmp(*argv, "-dbp") == 0 || strcmp(*argv, "--tosdb-file-path") == 0) {
            argc--;
            argv++;

            tosdbp = *argv;

            argc--;
            argv++;
        } else {
            print_error("invalid argument");

            return -1;
        }
    }

    FILE* in = fopen(tosdbp, "r");

    if(in == NULL) {
        print_error("cannot open tosdb");

        return -1;
    }

    fseek(in, 0, SEEK_END);
    int64_t tosdb_size   = ftell(in);
    fclose(in);

    data_t tosdb_name = {DATA_TYPE_STRING, 0, NULL, (char_t*)"tosdb"};
    data_t tosdb_data = {DATA_TYPE_STRING, 0, &tosdb_name, tosdb};

    data_t tosdb_size_name = {DATA_TYPE_STRING, 0, NULL, (char_t*)"tosdb-size"};
    data_t tosdb_size_data = {DATA_TYPE_INT64, 0, &tosdb_size_name, (void*)tosdb_size};

    data_t pxeconf[] = {tosdb_data, tosdb_size_data};

    data_t pxeconf_name = {DATA_TYPE_STRING, 0, NULL, (char_t*)"pxe-config"};
    data_t pxeconf_data = {DATA_TYPE_DATA, 2, &pxeconf_name, pxeconf};

    printf("tosdb %s path %s size: 0x%lx\n", tosdb, tosdbp, tosdb_size);

    data_t* ser_data = data_bson_serialize(&pxeconf_data, DATA_SERIALIZE_WITH_ALL);

    FILE* pco = fopen(output, "w");

    if(pco == NULL) {
        print_error("cannot open output");

        return -1;
    }

    fwrite(ser_data->value, ser_data->length, 1, pco);
    fclose(pco);

    data_free(ser_data);

    return 0;
}
