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
		uint8_t inside_method;
		uint8_t method_return;
	} flags;
	uint64_t timer;
	int8_t revision;
	void* method_context;
}acpi_aml_parser_context_t;


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
	ACPI_AML_OT_DEBUG,
	ACPI_AML_OT_ALIAS,
	ACPI_AML_OT_OPCODE_EXEC_RETURN,
	ACPI_AML_OT_SCOPE,
	ACPI_AML_OT_EXTERNAL,
	ACPI_AML_OT_DATAREGION,
	ACPI_AML_OT_METHODCALL,
	ACPI_AML_OT_RUNTIMEREF,
	ACPI_AML_OT_TIMER,
	ACPI_AML_OT_LOCAL_OR_ARG,
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

typedef enum {
	ACPI_AML_RESOURCE_SMALLITEM=0,
	ACPI_AML_RESOURCE_LARGEITEM,
}acpi_aml_resource_type_t;

typedef enum {
	ACPI_AML_RESOURCE_SMALLITEM_IRQ=4,
	ACPI_AML_RESOURCE_SMALLITEM_DMA=5,
	ACPI_AML_RESOURCE_SMALLITEM_START_DEPFUNC=6,
	ACPI_AML_RESOURCE_SMALLITEM_END_DEPFUNC=7,
	ACPI_AML_RESOURCE_SMALLITEM_IO=8,
	ACPI_AML_RESOURCE_SMALLITEM_FIXEDIO=9,
	ACPI_AML_RESOURCE_SMALLITEM_FIXEDDMA=10,
	ACPI_AML_RESOURCE_SMALLITEM_VENDOR=14,
	ACPI_AML_RESOURCE_SMALLITEM_ENDTAG=15,
}acpi_aml_resource_smallitem_name_t;

typedef enum {
	ACPI_AML_RESOURCE_LARGEITEM_24BIT_MEMORY_RANGE=1,
	ACPI_AML_RESOURCE_LARGEITEM_GENERIC_REGISTER=2,
	ACPI_AML_RESOURCE_LARGEITEM_VENDOR=4,
	ACPI_AML_RESOURCE_LARGEITEM_32BIT_MEMORY_RANGE=5,
	ACPI_AML_RESOURCE_LARGEITEM_32BIT_FIXEDMEMORY_RANGE=6,
	ACPI_AML_RESOURCE_LARGEITEM_DWORDADDRESS_SPACE=7,
	ACPI_AML_RESOURCE_LARGEITEM_WORD_ADDRESS_SPACE=8,
	ACPI_AML_RESOURCE_LARGEITEM_EXTENDED_INTERRUPT=9,
	ACPI_AML_RESOURCE_LARGEITEM_QWORD_ADDRESS_SPACE=10,
	ACPI_AML_RESOURCE_LARGEITEM_EXTENDED_ADDRESS_SPACE=11,
	ACPI_AML_RESOURCE_LARGEITEM_GPIO_CONNECTION=12,
	ACPI_AML_RESOURCE_LARGEITEM_PIN_FUNCTION=13,
	ACPI_AML_RESOURCE_LARGEITEM_GENERIC_SERIAL_BUS_CONNECTION=14,
	ACPI_AML_RESOURCE_LARGEITEM_PIN_CONFIGURATION=15,
	ACPI_AML_RESOURCE_LARGEITEM_PIN_GROUP=16,
	ACPI_AML_RESOURCE_LARGEITEM_PIN_GROUP_FUNCTION=17,
	ACPI_AML_RESOURCE_LARGEITEM_PIN_GROUP_CONFIGURATION=18,
}acpi_aml_resource_largeitem_name_t;

typedef union {
	struct {
		uint8_t unused : 7;
		acpi_aml_resource_type_t type : 1;
	}__attribute__((packed)) type;

	struct {
		uint8_t length : 3;
		acpi_aml_resource_smallitem_name_t name : 4;
		acpi_aml_resource_type_t type : 1;

		union {
			struct {
				uint16_t irq_masks : 16;
				uint8_t mode : 1;
				uint8_t ignored : 2;
				uint8_t polarity : 1;
				uint8_t sharing : 1;
				uint8_t wake_capability : 1;
				uint8_t reserved : 2;
			}__attribute__((packed)) irq;

			struct {
				uint8_t channels : 8;
				uint8_t size : 2;
				uint8_t bus_master_status : 1;
				uint8_t ignored : 2;
				uint8_t speed_type : 2;
				uint8_t zero : 1;
			}__attribute__((packed)) dma;

			struct {
				uint8_t compability : 2;
				uint8_t performance : 2;
				uint8_t reserved : 4;
			}__attribute__((packed)) start_depfunc;

			struct {
				uint8_t decode : 1;
				uint8_t unused : 7;
				uint16_t min : 16;
				uint16_t max : 16;
				uint8_t align : 8;
				uint8_t length : 8;
			}__attribute__((packed)) io;

			struct {
				uint16_t base : 10;
				uint8_t unused : 6;
				uint16_t length : 16;
			}__attribute__((packed)) fixedio;

			struct {
				uint16_t request_line : 16;
				uint16_t channel : 16;
				uint8_t size : 8;
			}__attribute__((packed)) fixeddma;

			struct {
				uint8_t checksum : 8;
			}__attribute__((packed)) end_tag;

		}__attribute__((packed));

	}__attribute__((packed)) smallitem;

	struct {
		acpi_aml_resource_largeitem_name_t name : 7;
		acpi_aml_resource_type_t type : 1;
		uint16_t length : 16;

		union {
			struct {
				uint8_t readwrite : 1;
				uint8_t ignored : 7;
				uint16_t min : 16;
				uint16_t max : 16;
				uint16_t align : 16;
				uint16_t length : 16;
			}__attribute__((packed)) memory_range_24bit;

			struct {

			}__attribute__((packed)) vendor;

			struct {

			}__attribute__((packed)) memory_range_32bit;

			struct {

			}__attribute__((packed)) memory_range_32bit_fixed;

			struct {

			}__attribute__((packed)) word_address_space;

			struct {

			}__attribute__((packed)) dword_address_space;

			struct {

			}__attribute__((packed)) qword_address_space;

			struct {

			}__attribute__((packed)) extended_address_space;

			struct {

			}__attribute__((packed)) extended_interrupt;

			struct {

			}__attribute__((packed)) generic_register;

			struct {

			}__attribute__((packed)) gpio_descriptor;

			struct {

			}__attribute__((packed)) generic_serial_bus;

			struct {

			}__attribute__((packed)) pin_function;

			struct {

			}__attribute__((packed)) pin_configuration;

			struct {

			}__attribute__((packed)) pin_group;

			struct {
				uint8_t revision_id : 8;
				uint16_t flags : 16;
				uint16_t function : 16;
				uint8_t source_index : 8;
				uint16_t source_name_offset : 16;
				uint16_t source_label_offset : 16;
				uint16_t vendor_data_offset : 16;
				uint16_t vendor_data_length : 16;
			}__attribute__((packed)) pin_group_function;

			struct {
				uint8_t revision_id : 8;
				uint16_t flags : 16;
				uint8_t type : 8;
				uint32_t value : 32;
				uint8_t source_index : 8;
				uint16_t source_name_offset : 16;
				uint16_t source_label_offset : 16;
				uint16_t vendor_data_length : 16;
			}__attribute__((packed)) pin_group_configuration;

		}__attribute__((packed));

	}__attribute__((packed)) largeitem;

}__attribute__((packed)) acpi_aml_resource_t;

acpi_aml_parser_context_t* acpi_aml_parser_context_create_with_heap(memory_heap_t*, uint8_t, uint8_t*, int64_t);
#define acpi_aml_parser_context_create(rev, aml, len) acpi_aml_parser_context_create_with_heap(NULL, rev, aml, len)
void acpi_aml_parser_context_destroy(acpi_aml_parser_context_t*);

int8_t acpi_aml_parse(acpi_aml_parser_context_t*);

void acpi_aml_print_symbol_table(acpi_aml_parser_context_t*);
void acpi_aml_print_object(acpi_aml_parser_context_t*, acpi_aml_object_t*);

void acpi_aml_destroy_symbol_table(acpi_aml_parser_context_t*, uint8_t);
void acpi_aml_destroy_object(acpi_aml_parser_context_t*, acpi_aml_object_t*);

#endif
