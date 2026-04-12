/**
 * @file dmidecode.c
 * @brief DMI decode program
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x800000
#include "setup.h"
#include <driver/smbios.h>


int32_t main(int32_t argc, char_t** argv) {

    if(argc <= 1) {
        print_error("invalid argument count");

        return -1;
    }

    argc--;
    argv++;

    FILE* in = fopen(*argv, "r");

    if(in == NULL) {
        print_error("cannot open dmi data file");

        return -1;
    }

    fseek(in, 0, SEEK_END);
    size_t size = ftell(in);
    fseek(in, 0, SEEK_SET);

    uint8_t* data = memory_malloc(size);

    if(data == NULL) {
        print_error("cannot allocate memory dmi data");

        fclose(in);

        return -1;
    }

    if(fread(data, 1, size, in) != size) {
        print_error("cannot read dmi data");

        memory_free(data);
        fclose(in);

        return -1;
    }

    fclose(in);

    uint8_t* original_data = data;

    uint8_t major_version = data[0x7];
    uint8_t minor_version = data[0x8];
    size_t offset         = data[0x10];
    data += offset;

    if(smbios_print_all_structures_from_raw_data(data, major_version, minor_version) != 0) {
        print_error("cannot print dmi data");
    } else {
        print_success("dmi data printed successfully");
    }

    memory_free(original_data);


    return 0;
}
