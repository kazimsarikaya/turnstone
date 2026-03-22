/**
 * @file aml.h
 * @brief acpi interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___ACPI_AML_H
/*! prevent duplicate header error macro */
#define ___ACPI_AML_H 0


#include <types.h>
#include <memory.h>
#include <list.h>
#include <hashmap.h>
#include <indexer.h>
#include <acpi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct acpi_aml_parser_context_t acpi_aml_parser_context_t;
typedef struct acpi_aml_object_t         acpi_aml_object_t;
typedef struct acpi_aml_device_t         acpi_aml_device_t;

typedef enum acpi_aml_device_memory_range_type_t {
    ACPI_AML_DEVICE_MEMORY_RANGE_MEMORY,
    ACPI_AML_DEVICE_MEMORY_RANGE_RESERVED,
    ACPI_AML_DEVICE_MEMORY_RANGE_ACPI,
    ACPI_AML_DEVICE_MEMORY_RANGE_NVS,
} acpi_aml_device_memory_range_type_t;

typedef struct acpi_aml_device_memory_range_t {
    boolean_t                           writable;
    boolean_t                           cacheable;
    boolean_t                           prefetchable;
    acpi_aml_device_memory_range_type_t type;
    uint64_t                            min;
    uint64_t                            max;
} acpi_aml_device_memory_range_t;

typedef struct acpi_aml_device_bus_t {
    uint16_t min;
    uint16_t max;
} acpi_aml_device_bus_t;

typedef struct acpi_aml_device_ioport_t {
    uint16_t min;
    uint16_t max;
    uint16_t alignment;
    uint16_t length;
} acpi_aml_device_ioport_t;

typedef struct acpi_aml_device_dma_t {
    boolean_t master;
    uint8_t   channels;
    uint8_t   speed;
} acpi_aml_device_dma_t;

typedef struct acpi_aml_device_interrupt_t {
    boolean_t edge;
    boolean_t low;
    boolean_t shared;
    boolean_t wake_capability;
    uint32_t  interrupt_no;
} acpi_aml_device_interrupt_t;

typedef struct acpi_aml_interrupt_map_item_t {
    uint32_t address;
    uint32_t interrupt_no;
} acpi_aml_interrupt_map_item_t;

struct acpi_aml_device_t {
    char_t*            name;
    acpi_aml_device_t* parent;
    acpi_aml_object_t* self;
    acpi_aml_object_t* adr;
    acpi_aml_object_t* crs;
    acpi_aml_object_t* dis;
    acpi_aml_object_t* hid;
    acpi_aml_object_t* ini;
    acpi_aml_object_t* prs;
    acpi_aml_object_t* prt;
    acpi_aml_object_t* srs;
    acpi_aml_object_t* sta;
    acpi_aml_object_t* uid;
    acpi_aml_object_t* pxm;
    acpi_aml_object_t* bbn;
    acpi_aml_object_t* cid;
    acpi_aml_object_t* osc;
    boolean_t          disabled;
    list_t*            buses;
    list_t*            ioports;
    list_t*            dmas;
    list_t*            memory_ranges;
    list_t*            interrupts;
    hashmap_t*         properties;
};

acpi_aml_parser_context_t* acpi_aml_parser_context_create_with_heap(memory_heap_t* heap, uint8_t rev);
#define acpi_aml_parser_context_create(rev) acpi_aml_parser_context_create_with_heap(NULL, rev)

int8_t acpi_aml_parser_parse_table(acpi_aml_parser_context_t* ctx, acpi_sdt_header_t* table);

acpi_aml_object_t* acpi_aml_symbol_lookup(acpi_aml_parser_context_t* ctx, const char_t* symbol_name);

void acpi_device_print_all(acpi_aml_parser_context_t* ctx);
void acpi_device_print(acpi_aml_parser_context_t* ctx, const acpi_aml_device_t* d);

const acpi_aml_device_t* acpi_device_lookup(acpi_aml_parser_context_t* ctx, const char_t* dev_name, uint64_t address);
#define acpi_device_lookup_by_address(c, a) acpi_device_lookup(c, NULL, a)
#define acpi_device_lookup_by_name(c, n) acpi_device_lookup(c, n, 0)

uint8_t* acpi_device_get_interrupts(acpi_aml_parser_context_t* ctx, uint64_t addr, uint8_t* int_count);

void acpi_aml_print_symbol_table(acpi_aml_parser_context_t* ctx);
void acpi_aml_print_object(acpi_aml_parser_context_t* ctx, const acpi_aml_object_t* obj);

#ifdef __cplusplus
}
#endif

#endif
