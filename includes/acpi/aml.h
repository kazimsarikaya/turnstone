/**
 * @file aml.h
 * @brief acpi interface
 */
#ifndef ___ACPI_AML_H
/*! prevent duplicate header error macro */
#define ___ACPI_AML_H 0


#include <types.h>
#include <memory.h>
#include <linkedlist.h>
#include <indexer.h>
#include <acpi.h>

//types

typedef enum {
	ACPI_AML_OT_UNINITIALIZED,
	ACPI_AML_OT_NUMBER,
	ACPI_AML_OT_STRING,
	ACPI_AML_OT_BUFFER,
	ACPI_AML_OT_PACKAGE,
	ACPI_AML_OT_FIELD,
	ACPI_AML_OT_DEVICE,
	ACPI_AML_OT_EVENT,
	ACPI_AML_OT_METHOD,
	ACPI_AML_OT_MUTEX,
	ACPI_AML_OT_OPREGION,
	ACPI_AML_OT_POWERRES,
	ACPI_AML_OT_PROCESSOR,
	ACPI_AML_OT_THERMALZONE,
	ACPI_AML_OT_BUFFERFIELD,
	ACPI_AML_OT_DDBHANDLE,
	ACPI_AML_OT_DEBUG,  // 16
	ACPI_AML_OT_ALIAS,
	ACPI_AML_OT_OPCODE_EXEC_RETURN,
	ACPI_AML_OT_SCOPE,
	ACPI_AML_OT_EXTERNAL,
	ACPI_AML_OT_DATAREGION,
	ACPI_AML_OT_METHODCALL,
	ACPI_AML_OT_RUNTIMEREF,
	ACPI_AML_OT_TIMER,
	ACPI_AML_OT_LOCAL_OR_ARG, // 25
	ACPI_AML_OT_REFOF,
}acpi_aml_object_type_t;

typedef struct _acpi_aml_object_t {
	acpi_aml_object_type_t type;
	char_t* name;
	union {
		struct {
			uint64_t value;
			uint8_t bytecnt;
		}number;
		char_t* string;
		struct {
			int64_t buflen;
			uint8_t* buf;
		} buffer;
		struct {
			struct _acpi_aml_object_t* pkglen;
			linkedlist_t elements;
		} package;
		struct _acpi_aml_object_t* opcode_exec_return;
		struct _acpi_aml_object_t* alias_target;
		struct _acpi_aml_object_t* refof_target;
		uint8_t mutex_sync_flags;
		struct {
			uint8_t arg_count;
			uint8_t serflag;
			uint8_t sync_level;
			int64_t termlist_length;
			uint8_t* termlist;
		} method;
		struct {
			uint8_t object_type;
			uint8_t arg_count;
		} external;
		struct {
			uint8_t system_level;
			uint16_t resource_order;
		} powerres;
		struct {
			uint8_t procid;
			uint32_t pblk_addr;
			uint8_t pblk_len;
		}processor;
		struct {
			char_t* signature;
			char_t* oemid;
			char_t* oemtableid;
		}dataregion;
		struct {
			uint8_t region_space;
			uint64_t region_offset;
			uint64_t region_len;
		} opregion;
		struct {
			struct _acpi_aml_object_t* related_object;
			struct _acpi_aml_object_t* selector_object;
			struct _acpi_aml_object_t* selector_data;
			uint8_t access_type;
			uint8_t access_attrib;
			uint8_t lock_rule;
			uint8_t update_rule;
			uint64_t offset;
			uint64_t sizeasbit;
		} field;
		struct {
			uint8_t idx_local_or_arg;
		} local_or_arg;
		uint64_t timer_value;
	};
}acpi_aml_object_t;

typedef struct {
	uint16_t min;
	uint16_t max;
}acpi_aml_device_bus_t;

typedef struct {
	uint16_t min;
	uint16_t max;
}acpi_aml_device_ioport_t;

typedef struct {
	boolean_t master;
	uint8_t channels;
	uint8_t speed;
}acpi_aml_device_dma_t;

typedef struct {
	boolean_t edge;
	boolean_t low;
	boolean_t shared;
	boolean_t wake_capability;
	uint32_t interrupt_no;
}acpi_aml_device_interrupt_t;

typedef enum {
	ACPI_AML_DEVICE_MEMORY_RANGE_MEMORY,
	ACPI_AML_DEVICE_MEMORY_RANGE_RESERVED,
	ACPI_AML_DEVICE_MEMORY_RANGE_ACPI,
	ACPI_AML_DEVICE_MEMORY_RANGE_NVS,
}acpi_aml_device_memory_range_type_t;

typedef struct {
	boolean_t writable;
	boolean_t cacheable;
	boolean_t prefetchable;
	acpi_aml_device_memory_range_type_t type;
	uint64_t min;
	uint64_t max;
}acpi_aml_device_memory_range_t;

typedef struct acpi_aml_device {
	char_t* name;
	struct acpi_aml_device* parent;
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
	boolean_t disabled;
	linkedlist_t buses;
	linkedlist_t ioports;
	linkedlist_t dmas;
	linkedlist_t memory_ranges;
	linkedlist_t interrupts;
}acpi_aml_device_t;

typedef struct {
	uint32_t address;
	uint32_t interrupt_no;
}acpi_aml_interrupt_map_item_t;

typedef struct {
	memory_heap_t* heap;
	uint8_t* data;
	uint64_t length;
	uint64_t remaining;
	char_t* scope_prefix;
	index_t* symbols;
	index_t* local_symbols;
	linkedlist_t devices;
	linkedlist_t interrupt_map;
	acpi_aml_object_t* pic;
	struct {
		uint8_t type;
		uint32_t code;
		uint64_t arg;
	} fatal_error;
	struct {
		uint8_t while_break;
		uint8_t while_cont;
		uint8_t fatal;
		uint8_t inside_method;
		uint8_t method_return;
		uint8_t dismiss_execute_method;
	} flags;
	uint64_t timer;
	int8_t revision;
	void* method_context;
}acpi_aml_parser_context_t;

int8_t acpi_aml_object_name_comparator(const void* data1, const void* data2);
int8_t acpi_aml_device_name_comparator(const void* data1, const void* data2);

acpi_aml_parser_context_t* acpi_aml_parser_context_create_with_heap(memory_heap_t* heap, uint8_t rev);
#define acpi_aml_parser_context_create(rev) acpi_aml_parser_context_create_with_heap(NULL, rev)
void acpi_aml_parser_context_destroy(acpi_aml_parser_context_t* ctx);

int8_t acpi_aml_parser_parse_table(acpi_aml_parser_context_t* ctx, acpi_sdt_header_t* table);

acpi_aml_object_t* acpi_aml_symbol_lookup(acpi_aml_parser_context_t*, char_t*);
int8_t acpi_aml_read_as_integer(acpi_aml_parser_context_t*, acpi_aml_object_t*, int64_t*);
int8_t acpi_aml_write_as_integer(acpi_aml_parser_context_t*, int64_t, acpi_aml_object_t*);

int8_t acpi_device_build(acpi_aml_parser_context_t*);
int8_t acpi_device_init(acpi_aml_parser_context_t*);
void acpi_device_print_all(acpi_aml_parser_context_t* ctx);
void acpi_device_print(acpi_aml_parser_context_t* ctx, acpi_aml_device_t* d);
acpi_aml_device_t* acpi_device_lookup(acpi_aml_parser_context_t* ctx, char_t* dev_name);
int8_t acpi_device_reserve_memory_ranges(acpi_aml_parser_context_t* ctx);
int8_t acpi_build_interrupt_map(acpi_aml_parser_context_t* ctx);

void acpi_aml_print_symbol_table(acpi_aml_parser_context_t*);
void acpi_aml_print_object(acpi_aml_parser_context_t*, acpi_aml_object_t*);

void acpi_aml_destroy_symbol_table(acpi_aml_parser_context_t*, uint8_t);
void acpi_aml_destroy_object(acpi_aml_parser_context_t*, acpi_aml_object_t*);

#endif
