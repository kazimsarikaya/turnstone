/**
 * @file aml.h
 * @brief acpi aml internal interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___ACPI_AML_INTERNAL_H
/*! prevent duplicate header error macro */
#define ___ACPI_AML_INTERNAL_H 0

#include <acpi/aml.h>

#ifndef ___ACPI_AML_IMPLEMENTATION
#error "This header is only for acpi implementation. Do not include this header directly."
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum acpi_aml_opcode_value_t {
    ACPI_AML_ZERO            = 0x00,
    ACPI_AML_ONE             = 0x01,
    ACPI_AML_ALIAS           = 0x06,
    ACPI_AML_NAME            = 0x08,
    ACPI_AML_BYTE_PREFIX     = 0x0A,
    ACPI_AML_WORD_PREFIX     = 0x0B,
    ACPI_AML_DWORD_PREFIX    = 0x0C,
    ACPI_AML_STRING_PREFIX   = 0x0D,
    ACPI_AML_QWORD_PREFIX    = 0x0E,
    ACPI_AML_SCOPE           = 0x10,
    ACPI_AML_BUFFER          = 0x11,
    ACPI_AML_PACKAGE         = 0x12,
    ACPI_AML_VARPACKAGE      = 0x13,
    ACPI_AML_METHOD          = 0x14,
    ACPI_AML_EXTERNAL        = 0x15,
    ACPI_AML_MULTI_PREFIX    = 0x2F,
    ACPI_AML_EXTOP_PREFIX    = 0x5B,
    ACPI_AML_ROOT_CHAR       = 0x5C,
    ACPI_AML_DUAL_PREFIX     = 0x2E,
    ACPI_AML_PARENT_CHAR     = 0x5E,
    ACPI_AML_LOCAL0          = 0x60,
    ACPI_AML_LOCAL1          = 0x61,
    ACPI_AML_LOCAL2          = 0x62,
    ACPI_AML_LOCAL3          = 0x63,
    ACPI_AML_LOCAL4          = 0x64,
    ACPI_AML_LOCAL5          = 0x65,
    ACPI_AML_LOCAL6          = 0x66,
    ACPI_AML_LOCAL7          = 0x67,
    ACPI_AML_ARG0            = 0x68,
    ACPI_AML_ARG1            = 0x69,
    ACPI_AML_ARG2            = 0x6A,
    ACPI_AML_ARG3            = 0x6B,
    ACPI_AML_ARG4            = 0x6C,
    ACPI_AML_ARG5            = 0x6D,
    ACPI_AML_ARG6            = 0x6E,
    ACPI_AML_STORE           = 0x70,
    ACPI_AML_REFOF           = 0x71,
    ACPI_AML_ADD             = 0x72,
    ACPI_AML_CONCAT          = 0x73,
    ACPI_AML_SUBTRACT        = 0x74,
    ACPI_AML_INCREMENT       = 0x75,
    ACPI_AML_DECREMENT       = 0x76,
    ACPI_AML_MULTIPLY        = 0x77,
    ACPI_AML_DIVIDE          = 0x78,
    ACPI_AML_SHL             = 0x79,
    ACPI_AML_SHR             = 0x7A,
    ACPI_AML_AND             = 0x7B,
    ACPI_AML_NAND            = 0x7C,
    ACPI_AML_OR              = 0x7D,
    ACPI_AML_NOR             = 0x7E,
    ACPI_AML_XOR             = 0x7F,
    ACPI_AML_NOT             = 0x80,
    ACPI_AML_FINDSETLEFTBIT  = 0x81,
    ACPI_AML_FINDSETRIGHTBIT = 0x82,
    ACPI_AML_DEREF           = 0x83,
    ACPI_AML_CONCATRES       = 0x84,
    ACPI_AML_MOD             = 0x85,
    ACPI_AML_NOTIFY          = 0x86,
    ACPI_AML_SIZEOF          = 0x87,
    ACPI_AML_INDEX           = 0x88,
    ACPI_AML_MATCH           = 0x89,
    ACPI_AML_DWORDFIELD      = 0x8A,
    ACPI_AML_WORDFIELD       = 0x8B,
    ACPI_AML_BYTEFIELD       = 0x8C,
    ACPI_AML_BITFIELD        = 0x8D,
    ACPI_AML_OBJECTTYPE      = 0x8E,
    ACPI_AML_QWORDFIELD      = 0x8F,
    ACPI_AML_LAND            = 0x90,
    ACPI_AML_LOR             = 0x91,
    ACPI_AML_LNOT            = 0x92,
    ACPI_AML_LEQUAL          = 0x93,
    ACPI_AML_LGREATER        = 0x94,
    ACPI_AML_LLESS           = 0x95,
    ACPI_AML_TOBUFFER        = 0x96,
    ACPI_AML_TODECIMALSTRING = 0x97,
    ACPI_AML_TOHEXSTRING     = 0x98,
    ACPI_AML_TOINTEGER       = 0x99,
    ACPI_AML_TOSTRING        = 0x9C,
    ACPI_AML_COPYOBJECT      = 0x9D,
    ACPI_AML_MID             = 0x9E,
    ACPI_AML_CONTINUE        = 0x9F,
    ACPI_AML_IF              = 0xA0,
    ACPI_AML_ELSE            = 0xA1,
    ACPI_AML_WHILE           = 0xA2,
    ACPI_AML_NOOP            = 0xA3,
    ACPI_AML_RETURN          = 0xA4,
    ACPI_AML_BREAK           = 0xA5,
    ACPI_AML_BREAKPOINT      = 0xCC,
    ACPI_AML_METHODCALL      = 0xFE, // not a real opcode, used for method call execution
    ACPI_AML_ONES            = 0xFF,
} acpi_aml_opcode_value_t;

// Extended opcodes
typedef enum acpi_aml_ext_opcode_value_t {
    ACPI_AML_MUTEX       = 0x01,
    ACPI_AML_EVENT       = 0x02,
    ACPI_AML_CONDREF     = 0x12,
    ACPI_AML_ARBFIELD    = 0x13,
    ACPI_AML_LOADTABLE   = 0x1F,
    ACPI_AML_LOAD        = 0x20,
    ACPI_AML_STALL       = 0x21,
    ACPI_AML_SLEEP       = 0x22,
    ACPI_AML_ACQUIRE     = 0x23,
    ACPI_AML_SIGNAL      = 0x24,
    ACPI_AML_WAIT        = 0x25,
    ACPI_AML_RESET       = 0x26,
    ACPI_AML_FROM_BCD    = 0x28,
    ACPI_AML_RELEASE     = 0x27,
    ACPI_AML_TO_BCD      = 0x29,
    ACPI_AML_REVISION    = 0x30,
    ACPI_AML_DEBUG       = 0x31,
    ACPI_AML_FATAL       = 0x32,
    ACPI_AML_TIMER       = 0x33,
    ACPI_AML_OPREGION    = 0x80,
    ACPI_AML_FIELD       = 0x81,
    ACPI_AML_DEVICE      = 0x82,
    ACPI_AML_PROCESSOR   = 0x83,
    ACPI_AML_POWERRES    = 0x84,
    ACPI_AML_THERMALZONE = 0x85,
    ACPI_AML_INDEXFIELD  = 0x86,
    ACPI_AML_BANKFIELD   = 0x87,
    ACPI_AML_DATAREGION  = 0x88,
} acpi_aml_ext_opcode_value_t;

typedef enum acpi_aml_field_access_type_t {
    ACPI_AML_FIELD_ACCESS_ANY,
    ACPI_AML_FIELD_ACCESS_BYTE,
    ACPI_AML_FIELD_ACCESS_WORD,
    ACPI_AML_FIELD_ACCESS_DWORD,
    ACPI_AML_FIELD_ACCESS_QWORD,
    ACPI_AML_FIELD_ACCESS_BUFFER,
    ACPI_AML_FIELD_ACCESS_BIT,
} acpi_aml_field_access_type_t;

typedef enum acpi_aml_field_update_rule_t {
    ACPI_AML_FIELD_UPDATE_PRESERVE,
    ACPI_AML_FIELD_UPDATE_WRITE_ONES,
    ACPI_AML_FIELD_UPDATE_WRITE_ZEROES,
    ACPI_AML_FIELD_UPDATE_OVERRIDE,
} acpi_aml_field_update_rule_t;

typedef enum acpi_aml_field_lock_rule_t {
    ACPI_AML_FIELD_LOCK_NOLOCK,
    ACPI_AML_FIELD_LOCK_LOCK,
} acpi_aml_field_lock_rule_t;


typedef enum acpi_aml_field_access_attribute_t {
    ACPI_AML_FIELD_ACCESS_ATTRIBUTE_NORMAL             = 0,
    ACPI_AML_FIELD_ACCESS_ATTRIBUTE_QUICK              = 2,
    ACPI_AML_FIELD_ACCESS_ATTRIBUTE_SEND_RECEIVE       = 4,
    ACPI_AML_FIELD_ACCESS_ATTRIBUTE_BYTE               = 6,
    ACPI_AML_FIELD_ACCESS_ATTRIBUTE_WORD               = 8,
    ACPI_AML_FIELD_ACCESS_ATTRIBUTE_BLOCK              = 10,
    ACPI_AML_FIELD_ACCESS_ATTRIBUTE_BYTES              = 11,
    ACPI_AML_FIELD_ACCESS_ATTRIBUTE_PROCESS_CALL       = 12,
    ACPI_AML_FIELD_ACCESS_ATTRIBUTE_BLOCK_PROCESS_CALL = 13,
    ACPI_AML_FIELD_ACCESS_ATTRIBUTE_READ_BYTES         = 14,
    ACPI_AML_FIELD_ACCESS_ATTRIBUTE_RAW_PROCESS_CALL   = 15,
} acpi_aml_field_access_attribute_t;

// Methods
#define ACPI_AML_METHOD_ARGC_MASK    0x07
#define ACPI_AML_METHOD_SERIALIZED_MASK   0x08


typedef enum acpi_aml_match_op_t {
    ACPI_AML_MATCH_OP_MTR,
    ACPI_AML_MATCH_OP_MEQ,
    ACPI_AML_MATCH_OP_MLE,
    ACPI_AML_MATCH_OP_MLT,
    ACPI_AML_MATCH_OP_MGE,
    ACPI_AML_MATCH_OP_MGT,
} acpi_aml_match_op_t;

typedef enum acpi_aml_opregt_t {
    ACPI_AML_OPREGT_SYSMEM,
    ACPI_AML_OPREGT_SYSIO,
    ACPI_AML_OPREGT_PCICFG,
    ACPI_AML_OPREGT_EMBEDDED,
    ACPI_AML_OPREGT_SMBUS,
    ACPI_AML_OPREGT_CMOS,
    ACPI_AML_OPREGT_PCIBAR,
    ACPI_AML_OPREGT_IPMI,
    ACPI_AML_OPREGT_GPIO,
    ACPI_AML_OPREGT_GSERBUS,
    ACPI_AML_OPREGT_PCC,
} acpi_aml_opregt_t;

typedef enum acpi_aml_object_type_t {
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
    ACPI_AML_OT_DEBUG, // 16
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
} acpi_aml_object_type_t;

struct acpi_aml_object_t {
    acpi_aml_object_type_t type;
    char_t*                name;
    int32_t                ref_count;
    union {
        char_t*            string;
        acpi_aml_object_t* opcode_exec_return;
        acpi_aml_object_t* alias_target;
        acpi_aml_object_t* refof_target;
        uint8_t            mutex_sync_flags;
        uint64_t           timer_value;
        struct {
            uint64_t value;
            uint8_t  bytecnt;
        } number;
        struct {
            int64_t  buflen;
            uint8_t* buf;
        } buffer;
        struct {
            acpi_aml_object_t* pkglen;
            list_t*            elements;
        } package;
        struct {
            uint8_t  arg_count;
            uint8_t  serflag;
            uint8_t  sync_level;
            int64_t  termlist_length;
            uint8_t* termlist;
        } method;
        struct {
            acpi_aml_object_type_t object_type;
            uint8_t                arg_count;
        } external;
        struct {
            uint8_t  system_level;
            uint16_t resource_order;
        } powerres;
        struct {
            uint8_t  procid;
            uint32_t pblk_addr;
            uint8_t  pblk_len;
        } processor;
        struct {
            char_t* signature;
            char_t* oemid;
            char_t* oemtableid;
        } dataregion;
        struct {
            acpi_aml_opregt_t region_space;
            uint64_t          region_offset;
            uint64_t          region_len;
        } opregion;
        struct {
            acpi_aml_object_t*                related_object;
            acpi_aml_object_t*                selector_object;
            acpi_aml_object_t*                selector_data;
            acpi_aml_field_access_type_t      access_type;
            acpi_aml_field_access_attribute_t access_attrib;
            acpi_aml_field_lock_rule_t        lock_rule;
            acpi_aml_field_update_rule_t      update_rule;
            uint64_t                          offset;
            uint64_t                          sizeasbit;
        } field;
        struct {
            uint8_t idx_local_or_arg;
        } local_or_arg;
    };
};

struct acpi_aml_parser_context_t {
    memory_heap_t*     heap;
    uint8_t*           data;
    uint64_t           length;
    uint64_t           remaining;
    char_t*            scope_prefix;
    index_t*           symbols;
    index_t*           local_symbols;
    list_t*            devices;
    list_t*            pci_roots;
    list_t*            interrupt_map;
    acpi_aml_object_t* pic;
    struct {
        uint8_t  type;
        uint32_t code;
        uint64_t arg;
    } fatal_error;
    struct {
        boolean_t while_break;
        boolean_t while_cont;
        boolean_t fatal;
        boolean_t inside_method;
        boolean_t method_return;
        boolean_t dismiss_execute_method;
    }        flags;
    uint64_t timer_base;
    int8_t   revision;
    void*    method_context;
};

typedef struct acpi_aml_opcode_t {
    uint8_t            operand_count;
    uint16_t           opcode;
    acpi_aml_object_t* operands[8];
    acpi_aml_object_t* return_obj;
} acpi_aml_opcode_t;

typedef struct acpi_aml_method_context_t {
    uint8_t            arg_count;
    acpi_aml_object_t* mthobjs[16]; // 0-7 -> locals 8-14 -> args 15 -> return
    boolean_t          dirty_args[7];
} acpi_aml_method_context_t;

void acpi_aml_parser_context_destroy(acpi_aml_parser_context_t* ctx);

index_t* acpi_aml_create_symbol_table(acpi_aml_parser_context_t* ctx, uint8_t level);

// util functions
boolean_t acpi_aml_is_root_char(uint8_t* c);
boolean_t acpi_aml_is_parent_prefix_char(uint8_t* c);
boolean_t acpi_aml_is_nameseg(uint8_t* data);
boolean_t acpi_aml_is_namestring_start(uint8_t* data);

char_t*  acpi_aml_normalize_name(acpi_aml_parser_context_t* ctx, const char_t* prefix, const char_t* name);
uint64_t acpi_aml_parse_package_length(acpi_aml_parser_context_t* ctx);
uint64_t acpi_aml_len_namestring(acpi_aml_parser_context_t* ctx);
int8_t   acpi_aml_executor_opcode(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode);
int8_t   acpi_aml_add_obj_to_symboltable(acpi_aml_parser_context_t * ctx, acpi_aml_object_t* obj);

boolean_t acpi_aml_is_null_target(acpi_aml_object_t* obj);

void               acpi_aml_destroy_symbol_table(acpi_aml_parser_context_t* ctx, boolean_t local);
acpi_aml_object_t* acpi_aml_symbol_lookup_at_table(acpi_aml_parser_context_t*, index_t* table, const char_t* prefix, const char_t* symbol_name);

acpi_aml_object_t* acpi_aml_duplicate_object(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj);
acpi_aml_object_t* acpi_aml_get_real_object(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj);
acpi_aml_object_t* acpi_aml_get_if_arg_local_obj(acpi_aml_parser_context_t*, acpi_aml_object_t*, boolean_t write, boolean_t copy);

int8_t acpi_aml_write_as_string(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* src, acpi_aml_object_t* dst);
int8_t acpi_aml_write_as_buffer(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* src, acpi_aml_object_t* dst);
int8_t acpi_aml_read_as_integer(acpi_aml_parser_context_t* ctx, const acpi_aml_object_t* obj, int64_t* res);
int8_t acpi_aml_write_as_integer(acpi_aml_parser_context_t * ctx, int64_t val, acpi_aml_object_t* obj);

char_t* acpi_aml_parse_eisaid(acpi_aml_parser_context_t* ctx, uint64_t eisaid_num);

int8_t acpi_aml_execute(acpi_aml_parser_context_t*, acpi_aml_object_t * mth, acpi_aml_object_t ** return_obj, ...);

boolean_t                acpi_aml_is_pci_root(const acpi_aml_device_t* dev);
const acpi_aml_device_t* acpi_aml_get_pci_root(const acpi_aml_device_t* dev);
uint64_t                 acpi_aml_get_device_pci_address(const acpi_aml_device_t* dev);

int8_t acpi_device_build(acpi_aml_parser_context_t* ctx);
int8_t acpi_device_init(acpi_aml_parser_context_t* ctx);
int8_t acpi_device_reserve_memory_ranges(acpi_aml_parser_context_t* ctx);
int8_t acpi_build_interrupt_map(acpi_aml_parser_context_t* ctx);

void acpi_aml_destroy_object(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj);

int8_t acpi_aml_parse_all_items(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed);
int8_t acpi_aml_parse_one_item(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed);
int8_t acpi_aml_parse_op_code_with_cnt(uint16_t oc, uint8_t opcnt, acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed, acpi_aml_object_t* preop);

#define CREATE_PARSER_F(name) int8_t acpi_aml_parse_ ## name(acpi_aml_parser_context_t*, void**, uint64_t*);

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

CREATE_PARSER_F(region);

CREATE_PARSER_F(field);

CREATE_PARSER_F(fatal);

CREATE_PARSER_F(acquire);

CREATE_PARSER_F(extopcnt_0);
CREATE_PARSER_F(extopcnt_1);
CREATE_PARSER_F(extopcnt_2);
CREATE_PARSER_F(extopcnt_6);

#define PARSER_F_NAME(name) acpi_aml_parse_ ## name

#define CREATE_EXEC_F(name) int8_t acpi_aml_exec_ ## name(acpi_aml_parser_context_t*, acpi_aml_opcode_t*);

CREATE_EXEC_F(store);
CREATE_EXEC_F(refof);
CREATE_EXEC_F(concat);
CREATE_EXEC_F(findsetbit);
CREATE_EXEC_F(derefof);
CREATE_EXEC_F(concatres);
CREATE_EXEC_F(notify);
CREATE_EXEC_F(op_sizeof);
CREATE_EXEC_F(index);
CREATE_EXEC_F(match);
CREATE_EXEC_F(object_type);
CREATE_EXEC_F(to_buffer);
CREATE_EXEC_F(to_decimalstring);
CREATE_EXEC_F(to_hexstring);
CREATE_EXEC_F(to_integer);
CREATE_EXEC_F(to_string);

CREATE_EXEC_F(op1_tgt0_maths);
CREATE_EXEC_F(op1_tgt1_maths);
CREATE_EXEC_F(op2_tgt1_maths);
CREATE_EXEC_F(op2_tgt2_maths);

CREATE_EXEC_F(op2_logic);


CREATE_EXEC_F(copy);
CREATE_EXEC_F(mid);

CREATE_EXEC_F(mth_return);

CREATE_EXEC_F(condrefof);
CREATE_EXEC_F(load_table);
CREATE_EXEC_F(load);
CREATE_EXEC_F(stall);
CREATE_EXEC_F(sleep);
CREATE_EXEC_F(acquire);
CREATE_EXEC_F(signal);
CREATE_EXEC_F(wait);
CREATE_EXEC_F(reset);
CREATE_EXEC_F(release);
CREATE_EXEC_F(from_bcd);
CREATE_EXEC_F(to_bcd);

CREATE_EXEC_F(method);

#define EXEC_F_NAME(name) acpi_aml_exec_ ## name

#ifdef __cplusplus
}
#endif

#endif
