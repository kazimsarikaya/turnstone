/**
 * @file aml.h
 * @brief acpi interface
 */
#ifndef ___ACPI_AML_H
/*! prevent duplicate header error macro */
#define ___ACPI_AML_H 0


#include <linkedlist.h>
#include <types.h>


#define ACPI_AML_ZERO                0x00
#define ACPI_AML_ONE                 0x01
#define ACPI_AML_ALIAS               0x06
#define ACPI_AML_NAME                0x08
#define ACPI_AML_BYTE_PREFIX         0x0A
#define ACPI_AML_WORD_PREFIX         0x0B
#define ACPI_AML_DWORD_PREFIX        0x0C
#define ACPI_AML_STRING_PREFIX       0x0D
#define ACPI_AML_QWORD_PREFIX        0x0E
#define ACPI_AML_SCOPE               0x10
#define ACPI_AML_BUFFER              0x11
#define ACPI_AML_PACKAGE             0x12
#define ACPI_AML_VARPACKAGE          0x13
#define ACPI_AML_METHOD              0x14
#define ACPI_AML_EXTERNAL            0x15
#define ACPI_AML_DUAL_PREFIX         0x2E
#define ACPI_AML_MULTI_PREFIX        0x2F
#define ACPI_AML_EXTOP_PREFIX        0x5B
#define ACPI_AML_ROOT_CHAR           0x5C
#define ACPI_AML_PARENT_CHAR         0x5E
#define ACPI_AML_LOCAL0              0x60
#define ACPI_AML_LOCAL1              0x61
#define ACPI_AML_LOCAL2              0x62
#define ACPI_AML_LOCAL3              0x63
#define ACPI_AML_LOCAL4              0x64
#define ACPI_AML_LOCAL5              0x65
#define ACPI_AML_LOCAL6              0x66
#define ACPI_AML_LOCAL7              0x67
#define ACPI_AML_ARG0                0x68
#define ACPI_AML_ARG1                0x69
#define ACPI_AML_ARG2                0x6A
#define ACPI_AML_ARG3                0x6B
#define ACPI_AML_ARG4                0x6C
#define ACPI_AML_ARG5                0x6D
#define ACPI_AML_ARG6                0x6E
#define ACPI_AML_STORE               0x70
#define ACPI_AML_REFOF               0x71
#define ACPI_AML_ADD                 0x72
#define ACPI_AML_CONCAT              0x73
#define ACPI_AML_SUBTRACT            0x74
#define ACPI_AML_INCREMENT           0x75
#define ACPI_AML_DECREMENT           0x76
#define ACPI_AML_MULTIPLY            0x77
#define ACPI_AML_DIVIDE              0x78
#define ACPI_AML_SHL                 0x79
#define ACPI_AML_SHR                 0x7A
#define ACPI_AML_AND                 0x7B
#define ACPI_AML_OR                  0x7D
#define ACPI_AML_XOR                 0x7F
#define ACPI_AML_NOT                 0x80
#define ACPI_AML_FINDSETLEFTBIT      0x81
#define ACPI_AML_FINDSETRIGHTBIT     0x82
#define ACPI_AML_DEREF               0x83
#define ACPI_AML_CONCATRES           0x84
#define ACPI_AML_MOD                 0x85
#define ACPI_AML_NOTIFY              0x86
#define ACPI_AML_SIZEOF              0x87
#define ACPI_AML_INDEX               0x88
#define ACPI_AML_MATCH               0x89
#define ACPI_AML_DWORDFIELD          0x8A
#define ACPI_AML_WORDFIELD           0x8B
#define ACPI_AML_BYTEFIELD           0x8C
#define ACPI_AML_BITFIELD            0x8D
#define ACPI_AML_OBJECTTYPE          0x8E
#define ACPI_AML_QWORDFIELD          0x8F
#define ACPI_AML_LAND                0x90
#define ACPI_AML_LOR                 0x91
#define ACPI_AML_LNOT                0x92
#define ACPI_AML_LEQUAL              0x93
#define ACPI_AML_LGREATER            0x94
#define ACPI_AML_LLESS               0x95
#define ACPI_AML_TOBUFFER            0x96
#define ACPI_AML_TODECIMALSTRING     0x97
#define ACPI_AML_TOHEXSTRING         0x98
#define ACPI_AML_TOINTEGER           0x99
#define ACPI_AML_TOSTRING            0x9C
#define ACPI_AML_COPYOBJECT          0x9D
#define ACPI_AML_CONTINUE            0x9F
#define ACPI_AML_IF                  0xA0
#define ACPI_AML_ELSE                0xA1
#define ACPI_AML_WHILE               0xA2
#define ACPI_AML_NOP                 0xA3
#define ACPI_AML_RETURN              0xA4
#define ACPI_AML_BREAK               0xA5
#define ACPI_AML_BREAKPOINT          0xCC
#define ACPI_AML_ONES                0xFF

// Extended opcodes
#define ACPI_AML_MUTEX               0x01
#define ACPI_AML_EVENT               0x02
#define ACPI_AML_CONDREF             0x12
#define ACPI_AML_ARBFIELD            0x13
#define ACPI_AML_LOADTABLE           0x1F
#define ACPI_AML_LOAD                0x20
#define ACPI_AML_STALL               0x21
#define ACPI_AML_SLEEP               0x22
#define ACPI_AML_ACQUIRE             0x23
#define ACPI_AML_SIGNAL              0x24
#define ACPI_AML_WAIT                0x25
#define ACPI_AML_RESET               0x26
#define ACPI_AML_FROM_BCD            0x28
#define ACPI_AML_RELEASE             0x27
#define ACPI_AML_TO_BCD              0x29
#define ACPI_AML_REVISION            0x30
#define ACPI_AML_DEBUG               0x31
#define ACPI_AML_FATAL               0x32
#define ACPI_AML_TIMER               0x33
#define ACPI_AML_OPREGION            0x80
#define ACPI_AML_FIELD               0x81
#define ACPI_AML_DEVICE              0x82
#define ACPI_AML_PROCESSOR           0x83
#define ACPI_AML_POWERRES            0x84
#define ACPI_AML_THERMALZONE         0x85
#define ACPI_AML_INDEXFIELD          0x86  // ACPI spec v5.0 section 19.5.60
#define ACPI_AML_BANKFIELD           0x87
#define ACPI_AML_DATAREGION          0x88

// Field Access Type
#define ACPI_AML_FIELD_ANY_ACCESS    0x00
#define ACPI_AML_FIELD_BYTE_ACCESS   0x01
#define ACPI_AML_FIELD_WORD_ACCESS   0x02
#define ACPI_AML_FIELD_DWORD_ACCESS  0x03
#define ACPI_AML_FIELD_QWORD_ACCESS  0x04
#define ACPI_AML_FIELD_LOCK          0x10
#define ACPI_AML_FIELD_PRESERVE      0x00
#define ACPI_AML_FIELD_WRITE_ONES    0x01
#define ACPI_AML_FIELD_WRITE_ZEROES  0x02

// Methods
#define ACPI_AML_METHOD_ARGC_MASK    0x07
#define ACPI_AML_METHOD_SERIALIZED   0x08

// Match Comparison Type
#define ACPI_AML_MATCH_MTR           0x00
#define ACPI_AML_MATCH_MEQ           0x01
#define ACPI_AML_MATCH_MLE           0x02
#define ACPI_AML_MATCH_MLT           0x03
#define ACPI_AML_MATCH_MGE           0x04
#define ACPI_AML_MATCH_MGT           0x05

//types

typedef struct {
	uint8_t* data;
	uint64_t length;
	uint64_t remaining;
	char_t* scope_prefix;
	linkedlist_t symbols;
	linkedlist_t local_symbols;
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
}acpi_aml_object_type_t;

typedef struct _acpi_aml_object_type_t {
	acpi_aml_object_type_t type;
	uint64_t refcount;
	char_t* name;
	union {
		uint64_t number;
		char_t* string;
		struct {
			int64_t buflen;
			uint8_t* buf;
		} buffer;
		struct {
			struct _acpi_aml_object_type_t* pkglen;
			linkedlist_t elements;
		} package;
		struct _acpi_aml_object_type_t* opcode_exec_return;
		struct _acpi_aml_object_type_t* alias_target;
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
	};
}acpi_aml_object_t;

typedef struct {
	uint8_t operand_count;
	uint16_t opcode;
	acpi_aml_object_t* operands[6];
	acpi_aml_object_t* return_obj;
}apci_aml_opcode_t;


typedef int8_t (* acpi_aml_parse_f)(acpi_aml_parser_context_t* ctx, void**, uint64_t*);

#define CREATE_PARSER_F(name) \
	int8_t acpi_aml_parse_ ## name(acpi_aml_parser_context_t*, void**, uint64_t*);

#define PARSER_F_NAME(name) acpi_aml_parse_ ## name

// util functions
int64_t acpi_aml_cast_as_integer(acpi_aml_object_t*);
int8_t acpi_aml_is_lead_name_char(uint8_t*);
int8_t acpi_aml_is_digit_char(uint8_t*);
int8_t acpi_aml_is_name_char(uint8_t*);
int8_t acpi_aml_is_root_char(uint8_t*);
int8_t acpi_aml_is_parent_prefix_char(uint8_t*);
char_t* acpi_aml_normalize_name(char_t*, char_t*);
int8_t acpi_aml_is_nameseg(uint8_t*);
int8_t acpi_aml_is_namestring_start(uint8_t*);
uint64_t acpi_aml_parse_package_length(acpi_aml_parser_context_t*);
uint64_t acpi_aml_len_namestring(acpi_aml_parser_context_t*);
acpi_aml_object_t* acpi_aml_symbol_lookup(acpi_aml_parser_context_t*, char_t*);
int8_t acpi_aml_executor_opcode(acpi_aml_parser_context_t*, apci_aml_opcode_t*);
int8_t acpi_aml_add_obj_to_symboltable(acpi_aml_parser_context_t* ctx, acpi_aml_object_t*);
uint8_t acpi_aml_get_index_of_extended_code(uint8_t);

acpi_aml_parser_context_t* acp_aml_parser_context_create(uint8_t*, int64_t);

// parser methods
CREATE_PARSER_F(one_item);
CREATE_PARSER_F(all_items);

CREATE_PARSER_F(namestring);

CREATE_PARSER_F(alias);
CREATE_PARSER_F(name);
CREATE_PARSER_F(scope);
CREATE_PARSER_F(const_data);

CREATE_PARSER_F(opcnt_0);
CREATE_PARSER_F(opcnt_1);
CREATE_PARSER_F(opcnt_2);
CREATE_PARSER_F(opcnt_3);
CREATE_PARSER_F(opcnt_4);

CREATE_PARSER_F(op_match);
CREATE_PARSER_F(logic_ext);

CREATE_PARSER_F(op_if);
CREATE_PARSER_F(op_else);
CREATE_PARSER_F(op_while);

CREATE_PARSER_F(create_field);

CREATE_PARSER_F(op_extended);

CREATE_PARSER_F(buffer);
CREATE_PARSER_F(package);
CREATE_PARSER_F(varpackage);
CREATE_PARSER_F(method);
CREATE_PARSER_F(external);

CREATE_PARSER_F(symbol);

CREATE_PARSER_F(byte_data);

CREATE_PARSER_F(mutex);
CREATE_PARSER_F(event);
CREATE_PARSER_F(opregion);
CREATE_PARSER_F(field);
CREATE_PARSER_F(indexfield);
CREATE_PARSER_F(bankfield);
CREATE_PARSER_F(dataregion);

CREATE_PARSER_F(fatal);

CREATE_PARSER_F(extopcnt_0);
CREATE_PARSER_F(extopcnt_1);
CREATE_PARSER_F(extopcnt_2);
CREATE_PARSER_F(extopcnt_4);
CREATE_PARSER_F(extopcnt_6);


#define UNIMPLPARSER(name) \
	int8_t acpi_aml_parse_ ## name(acpi_aml_parser_context_t * ctx, void** data, uint64_t * consumed){ \
		UNUSED(data); \
		UNUSED(ctx); \
		UNUSED(consumed); \
		return -1; \
	}

#ifdef ___TESTMODE
int printf(const char* format, ...);
#endif

#endif
