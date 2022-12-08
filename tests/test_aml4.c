/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x100000 * 8
#include "setup.h"
#include <acpi.h>
#include <acpi/aml.h>
#include <linkedlist.h>
#include <strings.h>
#include <utils.h>
#include <bplustree.h>

int8_t pci_io_port_write_data(uint32_t address, uint32_t data, uint8_t bc){
    UNUSED(address);
    UNUSED(data);
    UNUSED(bc);
    return 0;
}
uint32_t pci_io_port_read_data(uint32_t address, uint8_t bc){
    UNUSED(address);
    UNUSED(bc);
    return 0x1234;
}

uint32_t main(uint32_t argc, char_t** argv) {
    setup_ram();
    FILE* fp;
    int64_t size;
    if(argc < 2) {
        print_error("no required parameter");
        return -1;
    }

    argv++;
    printf("file name: %s\n", *argv);
    fp = fopen(*argv, "r");
    if(fp == NULL) {
        print_error("can not open the file");
        return -2;
    }
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    printf("file size: %i\n", size);
    fseek(fp, 0x0, SEEK_SET);
    uint8_t* aml_data = memory_malloc(sizeof(uint8_t) * size);
    if(aml_data == NULL) {
        print_error("cannot malloc madt data area");
        return -3;
    }
    fread(aml_data, 1, size, fp);
    fclose(fp);
    print_success("file readed");

    acpi_sdt_header_t* hdr = (acpi_sdt_header_t*)aml_data;

    char_t* table_name = memory_malloc(sizeof(char_t) * 5);
    memory_memcopy(hdr->signature, table_name, 4);
    printf("table name: %s\n", table_name);
    memory_free(table_name);

    int8_t res = -1;

    acpi_aml_parser_context_t* ctx = acpi_aml_parser_context_create(hdr->revision);
    if(ctx == NULL) {
        print_error("cannot create parser context");
    } else {
        res = acpi_aml_parser_parse_table(ctx, hdr);

        if(res == 0) {
            print_success("aml parsed");
        } else {
            print_error("aml not parsed");
        }

        if(acpi_device_build(ctx) != 0) {
            PRINTLOG(ACPI, LOG_ERROR, "devices cannot be builded");
        } else {
            acpi_device_print_all(ctx);
            acpi_aml_print_symbol_table(ctx);

            if(acpi_device_init(ctx) != 0) {
                PRINTLOG(ACPI, LOG_ERROR, "devices cannot be initialized");
            }
        }

        acpi_aml_parser_context_destroy(ctx);
    }

    memory_free(aml_data);
    dump_ram("tmp/mem.dump");

    if(res == 0) {
        print_success("Tests are passed");
    } else {
        print_error("Tests are not passed");
    }

    return 0;
}
