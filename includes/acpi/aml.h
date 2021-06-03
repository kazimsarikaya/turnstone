/**
 * @file aml.h
 * @brief acpi interface
 */
#ifndef ___ACPI_AML_H
/*! prevent duplicate header error macro */
#define ___ACPI_AML_H 0


#include <linkedlist.h>
#include <types.h>
#include <memory.h>

//types

typedef struct {
	memory_heap_t* heap;
	uint8_t* data;
	uint64_t length;
	uint64_t remaining;
	char_t* scope_prefix;
	linkedlist_t symbols;
	linkedlist_t local_symbols;
	struct {
		uint8_t type;
		uint32_t code;
		uint64_t arg;
	} fatal_error;
	struct {
		uint8_t while_break;
		uint8_t while_cont;
		uint8_t fatal;
	} flags;
}acpi_aml_parser_context_t;


typedef enum {
	ACPI_AML_OT_NUMBER,
	ACPI_AML_OT_STRING,
	ACPI_AML_OT_ALIAS,
	ACPI_AML_OT_BUFFER,
	ACPI_AML_OT_PACKAGE,
	ACPI_AML_OT_OPCODE_EXEC_RETURN,
	ACPI_AML_OT_SCOPE,
	ACPI_AML_OT_DEVICE,
	ACPI_AML_OT_POWERRES,
	ACPI_AML_OT_PROCESSOR,
	ACPI_AML_OT_THERMALZONE,
	ACPI_AML_OT_METHOD,
	ACPI_AML_OT_EXTERNAL,
	ACPI_AML_OT_MUTEX,
	ACPI_AML_OT_EVENT,
	ACPI_AML_OT_DATAREGION,
	ACPI_AML_OT_OPREGION,
	ACPI_AML_OT_FIELD,
	ACPI_AML_OT_METHODCALL,
	ACPI_AML_OT_RUNTIMEREF,
}acpi_aml_object_type_t;

typedef struct _acpi_aml_object_t {
	acpi_aml_object_type_t type;
	uint64_t refcount;
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
	};
}acpi_aml_object_t;

acpi_aml_parser_context_t* acpi_aml_parser_context_create_with_heap(memory_heap_t*, uint8_t*, int64_t);
#define acpi_aml_parser_context_create(aml, len) acpi_aml_parser_context_create_with_heap(NULL, aml, len)

int64_t acpi_aml_read_as_integer(acpi_aml_object_t*);
int8_t acpi_aml_write_as_integer(acpi_aml_parser_context_t*, int64_t, acpi_aml_object_t*);

int8_t acpi_aml_parse(acpi_aml_parser_context_t*);

void acpi_aml_print_symbol_table(acpi_aml_parser_context_t*);
void acpi_aml_print_object(acpi_aml_object_t*);

#endif
