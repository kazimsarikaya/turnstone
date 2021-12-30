/**
 * @file aml.h
 * @brief acpi aml internal interface
 */
#ifndef ___ACPI_AML_INTERNAL_H
/*! prevent duplicate header error macro */
#define ___ACPI_AML_INTERNAL_H 0

#include "aml.h"


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
#define ACPI_AML_NAND                0x7C
#define ACPI_AML_OR                  0x7D
#define ACPI_AML_NOR                 0x7E
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
#define ACPI_AML_MID                 0x9E
#define ACPI_AML_CONTINUE            0x9F
#define ACPI_AML_IF                  0xA0
#define ACPI_AML_ELSE                0xA1
#define ACPI_AML_WHILE               0xA2
#define ACPI_AML_NOOP                0xA3
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
#define ACPI_AML_INDEXFIELD          0x86
#define ACPI_AML_BANKFIELD           0x87
#define ACPI_AML_DATAREGION          0x88

// Field Access Type
#define ACPI_AML_FIELD_ANY_ACCESS    0x00
#define ACPI_AML_FIELD_BYTE_ACCESS   0x01
#define ACPI_AML_FIELD_WORD_ACCESS   0x02
#define ACPI_AML_FIELD_DWORD_ACCESS  0x03
#define ACPI_AML_FIELD_QWORD_ACCESS  0x04
#define ACPI_AML_FIELD_BUF_ACCESS    0x05
#define ACPI_AML_FIELD_BIT_ACCESS    0x06 // extra def, not in acpi spec
#define ACPI_AML_FIELD_NOLOCK        0x00
#define ACPI_AML_FIELD_LOCK          0x01
#define ACPI_AML_FIELD_PRESERVE      0x00
#define ACPI_AML_FIELD_WRITE_ONES    0x01
#define ACPI_AML_FIELD_WRITE_ZEROES  0x02
#define ACPI_AML_FIELD_OVERRIDE      0x03 // extra def, not in acpi spec
#define ACPI_AML_FACCATTRB_NORMAL    0x00
#define ACPI_AML_FACCATTRB_QUICK     0x02
#define ACPI_AML_FACCATTRB_SR        0x04 // send recive
#define ACPI_AML_FACCATTRB_BYTE      0x06
#define ACPI_AML_FACCATTRB_WORD      0x08
#define ACPI_AML_FACCATTRB_BLOCK     0x0A
#define ACPI_AML_FACCATTRB_PC        0x0C // process call
#define ACPI_AML_FACCATTRB_BPC       0x0D // block process call
#define ACPI_AML_FEACCATTRB_BYTES    0x0B
#define ACPI_AML_FEACCATTRB_RBYTES   0x0E
#define ACPI_AML_FEACCATTRB_RPC      0x0F // raw process call

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

#define ACPI_AML_OPREGT_SYSMEM       0x00
#define ACPI_AML_OPREGT_SYSIO        0x01
#define ACPI_AML_OPREGT_PCICFG       0x02
#define ACPI_AML_OPREGT_EMBCTL       0x03
#define ACPI_AML_OPREGT_SMBUS        0x04
#define ACPI_AML_OPREGT_SYSCMOS      0x05
#define ACPI_AML_OPREGT_PCIBAR       0x06
#define ACPI_AML_OPREGT_IPMI         0x07
#define ACPI_AML_OPREGT_GPIO         0x08
#define ACPI_AML_OPREGT_GSERBUS      0x09
#define ACPI_AML_OPREGT_PCC          0x0A

#define ACPI_AML_METHODCALL          0xFE

typedef struct {
	uint8_t operand_count;
	uint16_t opcode;
	acpi_aml_object_t* operands[8];
	acpi_aml_object_t* return_obj;
}acpi_aml_opcode_t;

typedef struct {
	uint8_t arg_count;
	acpi_aml_object_t** mthobjs; // 0-7 -> locals 8-14 -> args 15 -> return
	uint8_t dirty_args[7];
}acpi_aml_method_context_t;

int8_t acpi_aml_parse_all_items(acpi_aml_parser_context_t*, void**, uint64_t*);
int8_t acpi_aml_parse_one_item(acpi_aml_parser_context_t*, void**, uint64_t*);

// util functions
int8_t acpi_aml_is_lead_name_char(uint8_t*);
int8_t acpi_aml_is_digit_char(uint8_t*);
int8_t acpi_aml_is_name_char(uint8_t*);
int8_t acpi_aml_is_root_char(uint8_t*);
int8_t acpi_aml_is_parent_prefix_char(uint8_t*);
char_t* acpi_aml_normalize_name(acpi_aml_parser_context_t*, char_t*, char_t*);
int8_t acpi_aml_is_nameseg(uint8_t*);
int8_t acpi_aml_is_namestring_start(uint8_t*);
uint64_t acpi_aml_parse_package_length(acpi_aml_parser_context_t*);
uint64_t acpi_aml_len_namestring(acpi_aml_parser_context_t*);
acpi_aml_object_t* acpi_aml_symbol_lookup(acpi_aml_parser_context_t*, char_t*);
acpi_aml_object_t* acpi_aml_symbol_lookup_at_table(acpi_aml_parser_context_t*, linkedlist_t, char_t*, char_t*);
int8_t acpi_aml_executor_opcode(acpi_aml_parser_context_t*, acpi_aml_opcode_t*);
int8_t acpi_aml_add_obj_to_symboltable(acpi_aml_parser_context_t* ctx, acpi_aml_object_t*);
uint8_t acpi_aml_get_index_of_extended_code(uint8_t);
int8_t acpi_aml_parse_op_code_with_cnt(uint16_t, uint8_t, acpi_aml_parser_context_t*, void**, uint64_t*, acpi_aml_object_t*);

int8_t acpi_aml_is_null_target(acpi_aml_object_t*);

acpi_aml_object_t* acpi_aml_duplicate_object(acpi_aml_parser_context_t*, acpi_aml_object_t*);
acpi_aml_object_t* acpi_aml_get_real_object(acpi_aml_parser_context_t*, acpi_aml_object_t*);
acpi_aml_object_t* acpi_aml_get_if_arg_local_obj(acpi_aml_parser_context_t*, acpi_aml_object_t*, uint8_t, uint8_t);

int8_t acpi_aml_read_as_integer(acpi_aml_parser_context_t*, acpi_aml_object_t*, int64_t*);
int8_t acpi_aml_write_as_integer(acpi_aml_parser_context_t*, int64_t, acpi_aml_object_t*);
int8_t acpi_aml_write_as_string(acpi_aml_parser_context_t*, acpi_aml_object_t*, acpi_aml_object_t*);
int8_t acpi_aml_write_as_buffer(acpi_aml_parser_context_t*, acpi_aml_object_t*, acpi_aml_object_t*);

#endif
