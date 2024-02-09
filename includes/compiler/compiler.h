/**
 * @file compiler.h
 * @brief Turnstone OS compiler header
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___COMPILER_H
#define ___COMPILER_H 0

#include <types.h>
#include <buffer.h>
#include <linkedlist.h>
#include <hashmap.h>

/**
 * @enum compiler_token_type_t **/
typedef enum compiler_token_type_t {
    COMPILER_TOKEN_TYPE_EOF = 0,
    COMPILER_TOKEN_TYPE_UNKNOWN,
    COMPILER_TOKEN_TYPE_INTEGER_CONST,
    COMPILER_TOKEN_TYPE_REAL_CONST,
    COMPILER_TOKEN_TYPE_STRING_CONST,
    COMPILER_TOKEN_TYPE_PLUS,
    COMPILER_TOKEN_TYPE_MINUS,
    COMPILER_TOKEN_TYPE_OR,
    COMPILER_TOKEN_TYPE_XOR,
    COMPILER_TOKEN_TYPE_MULTIPLY,
    COMPILER_TOKEN_TYPE_REAL_DIVIDE,
    COMPILER_TOKEN_TYPE_INTEGER_DIVIDE,
    COMPILER_TOKEN_TYPE_MOD,
    COMPILER_TOKEN_TYPE_AND,
    COMPILER_TOKEN_TYPE_NOT,
    COMPILER_TOKEN_TYPE_LPAREN,
    COMPILER_TOKEN_TYPE_RPAREN,
    COMPILER_TOKEN_TYPE_ID,
    COMPILER_TOKEN_TYPE_ASSIGN,
    COMPILER_TOKEN_TYPE_SEMI,
    COMPILER_TOKEN_TYPE_DOT,
    COMPILER_TOKEN_TYPE_COMMA,
    COMPILER_TOKEN_TYPE_COLON,
    COMPILER_TOKEN_TYPE_EQUAL,
    COMPILER_TOKEN_TYPE_NOT_EQUAL,
    COMPILER_TOKEN_TYPE_LESS_THAN,
    COMPILER_TOKEN_TYPE_LESS_THAN_OR_EQUAL,
    COMPILER_TOKEN_TYPE_GREATER_THAN,
    COMPILER_TOKEN_TYPE_GREATER_THAN_OR_EQUAL,
    COMPILER_TOKEN_TYPE_IN,
    COMPILER_TOKEN_TYPE_SHL,
    COMPILER_TOKEN_TYPE_SHR,
    COMPILER_TOKEN_TYPE_BEGIN,
    COMPILER_TOKEN_TYPE_END,
    COMPILER_TOKEN_TYPE_PROGRAM,
    COMPILER_TOKEN_TYPE_PROCEDURE,
    COMPILER_TOKEN_TYPE_FUNCTION,
    COMPILER_TOKEN_TYPE_VAR,
    COMPILER_TOKEN_TYPE_CONST,
    COMPILER_TOKEN_TYPE_INTEGER,
    COMPILER_TOKEN_TYPE_REAL,
    COMPILER_TOKEN_TYPE_CHAR,
    COMPILER_TOKEN_TYPE_STRING,
    COMPILER_TOKEN_TYPE_BOOLEAN,
    COMPILER_TOKEN_TYPE_NULL,
    COMPILER_TOKEN_TYPE_IF,
    COMPILER_TOKEN_TYPE_THEN,
    COMPILER_TOKEN_TYPE_ELSE,
    COMPILER_TOKEN_TYPE_WHILE,
    COMPILER_TOKEN_TYPE_DO,
    COMPILER_TOKEN_TYPE_REPEAT,
    COMPILER_TOKEN_TYPE_UNTIL,
    COMPILER_TOKEN_TYPE_FOR,
    COMPILER_TOKEN_TYPE_TO,
    COMPILER_TOKEN_TYPE_DOWNTO,
    COMPILER_TOKEN_TYPE_STEP,
    COMPILER_TOKEN_TYPE_CONTINUE,
    COMPILER_TOKEN_TYPE_BREAK,
    COMPILER_TOKEN_TYPE_LBRACKET,
    COMPILER_TOKEN_TYPE_RBRACKET,
    COMPILER_TOKEN_TYPE_TYPE,
    COMPILER_TOKEN_TYPE_RECORD,
    COMPILER_TOKEN_TYPE_WITH,
    COMPILER_TOKEN_TYPE_PACKED,
} compiler_token_type_t;

typedef struct compiler_token_t {
    compiler_token_type_t type;
    boolean_t             not_free;
    int64_t               value;
    float64_t             real_value;
    const char_t*         text;
    boolean_t             is_const;
    uint64_t              size;
    boolean_t             is_unsigned;
    boolean_t             is_array;
} compiler_token_t;

typedef enum compiler_ast_node_type_t {
    COMPILER_AST_NODE_TYPE_INTEGER_CONST = 0,
    COMPILER_AST_NODE_TYPE_REAL_CONST,
    COMPILER_AST_NODE_TYPE_STRING_CONST,
    COMPILER_AST_NODE_TYPE_RELATIONAL_OP,
    COMPILER_AST_NODE_TYPE_BINARY_OP,
    COMPILER_AST_NODE_TYPE_UNARY_OP,
    COMPILER_AST_NODE_TYPE_NO_OP,
    COMPILER_AST_NODE_TYPE_ASSIGN,
    COMPILER_AST_NODE_TYPE_PROGRAM,
    COMPILER_AST_NODE_TYPE_DECLS,
    COMPILER_AST_NODE_TYPE_VAR,
    COMPILER_AST_NODE_TYPE_TYPE,
    COMPILER_AST_NODE_TYPE_BLOCK,
    COMPILER_AST_NODE_TYPE_COMPOUND,
    COMPILER_AST_NODE_TYPE_FUNCTION_CALL,
    COMPILER_AST_NODE_TYPE_IF,
    COMPILER_AST_NODE_TYPE_WHILE,
    COMPILER_AST_NODE_TYPE_REPEAT,
    COMPILER_AST_NODE_TYPE_WITH,
} compiler_ast_node_type_t;

typedef enum compiler_symbol_type_t {
    COMPILER_SYMBOL_TYPE_UNKNOWN = -1,
    COMPILER_SYMBOL_TYPE_VOID = 0,
    COMPILER_SYMBOL_TYPE_BOOLEAN,
    COMPILER_SYMBOL_TYPE_INTEGER,
    COMPILER_SYMBOL_TYPE_REAL,
    COMPILER_SYMBOL_TYPE_STRING,
    COMPILER_SYMBOL_TYPE_CUSTOM,
} compiler_symbol_type_t;

typedef struct compiler_symbol_t {
    const char_t*          name;
    compiler_symbol_type_t type;
    compiler_symbol_type_t hidden_type;
    int64_t                custom_type_id;
    int64_t                size;
    boolean_t              initilized;
    int64_t                int_value;
    int64_t*               int_array_value;
    float64_t              real_value;
    const char_t*          string_value;
    uint16_t               stack_offset;
    boolean_t              is_const;
    boolean_t              is_local;
    boolean_t              is_array;
    uint64_t               array_size;
} compiler_symbol_t;

typedef struct compiler_type_t compiler_type_t;

struct compiler_type_t {
    const char_t*         name;
    int64_t               id;
    compiler_token_type_t type;
    boolean_t             is_packed;
    linkedlist_t*         fields;
};

typedef struct compiler_ast_node_t compiler_ast_node_t;

struct compiler_ast_node_t {
    compiler_ast_node_type_t type;
    compiler_token_t*        token;
    compiler_ast_node_t*     left;
    compiler_ast_node_t*     right;
    compiler_ast_node_t*     next;
    compiler_ast_node_t*     condition;
    linkedlist_t*            children;
    compiler_type_t*         type_data;
    int16_t                  used_register;
    compiler_symbol_t*       symbol;
    boolean_t                is_array_subscript;
    compiler_ast_node_t*     array_subscript;
};

typedef struct compiler_ast_t {
    compiler_ast_node_t * root;
} compiler_ast_t;

#define COMPILER_VM_REG_COUNT 13

typedef struct symbol_table_t symbol_table_t;

struct symbol_table_t {
    symbol_table_t* parent;
    hashmap_t*      symbols;
};

typedef struct compiler_t {
    compiler_ast_t *       ast;
    const char_t*          program_name;
    compiler_symbol_t*     program_name_symbol;
    buffer_t*              text_buffer;
    buffer_t*              data_buffer;
    buffer_t*              rodata_buffer;
    buffer_t*              bss_buffer;
    symbol_table_t*        main_symbol_table;
    symbol_table_t*        current_symbol_table;
    uint16_t               stack_size;
    uint16_t               next_stack_offset;
    boolean_t              is_const;
    boolean_t              is_at_reg;
    boolean_t              busy_regs[COMPILER_VM_REG_COUNT];
    int32_t                next_label_id;
    linkedlist_t*          cond_label_stack;
    int64_t                cond_depth;
    linkedlist_t*          loop_label_stack;
    int64_t                loop_depth;
    boolean_t              is_cond_eval;
    boolean_t              is_cond_reverse;
    int64_t                computed_size;
    compiler_symbol_type_t computed_type;
} compiler_t;


int8_t compiler_token_destroy(compiler_token_t * token);

int8_t compiler_symbol_destroyer(memory_heap_t* heap, void* symbol);

#define compiler_destroy_symbol_list(list) linkedlist_destroy_with_type(list, LINKEDLIST_DESTROY_WITH_DATA, compiler_symbol_destroyer)
int8_t compiler_ast_node_destroy(compiler_ast_node_t * node);
int8_t compiler_ast_node_destroyer(memory_heap_t* heap, void* node);
int8_t compiler_ast_init(compiler_ast_t * ast);
int8_t compiler_ast_destroy(compiler_ast_t * ast);


typedef enum compiler_reg_ids_t {
    COMPILER_VM_REG_RAX = 0,
    COMPILER_VM_REG_RBX,
    COMPILER_VM_REG_RCX,
    COMPILER_VM_REG_RDX,
    COMPILER_VM_REG_RSI,
    COMPILER_VM_REG_RDI,
    COMPILER_VM_REG_R8,
    COMPILER_VM_REG_R9,
    COMPILER_VM_REG_R10,
    COMPILER_VM_REG_R11,
    COMPILER_VM_REG_R12,
    COMPILER_VM_REG_R13,
    COMPILER_VM_REG_R14,
} compiler_reg_ids_t;

const int16_t compiler_fcall_reg_order[] = {
    COMPILER_VM_REG_RDI,
    COMPILER_VM_REG_RSI,
    COMPILER_VM_REG_RDX,
    COMPILER_VM_REG_R10,
    COMPILER_VM_REG_R8,
    COMPILER_VM_REG_R9,
};

extern const char_t*const compiler_regs[];

int8_t                   compiler_init(compiler_t * compiler, compiler_ast_t * ast);
int8_t                   compiler_destroy(compiler_t * compiler);
int8_t                   compiler_execute_ast_node(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t                   compiler_execute_unary_op(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t                   compiler_execute_binary_op(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t                   compiler_execute_relational_op(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t                   compiler_execute_block(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t                   compiler_execute_compound(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t                   compiler_execute(compiler_t * compiler, int64_t* result);
int8_t                   compiler_print_symbol_table(compiler_t * compiler);
int8_t                   compiler_destroy_symbol_table(compiler_t * compiler);
int8_t                   compiler_build_stack(compiler_t* compiler);
int8_t                   compiler_find_free_reg(compiler_t* compiler);
const compiler_symbol_t* compiler_find_symbol(compiler_t* compiler, const char_t* name);
int8_t                   compiler_execute_function_call(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t                   compiler_execute_string_const(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t                   compiler_execute_if(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t                   compiler_execute_while(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t                   compiler_execute_repeat(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t                   compiler_execute_save(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t                   compiler_execute_load(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
const char_t*            compiler_cast_reg_to_size(const char_t* reg, uint8_t size);
char_t                   compiler_get_reg_suffix(uint8_t size);
int8_t                   compiler_define_symbol(compiler_t* compiler, compiler_symbol_t* symbol, size_t symbol_size);


#define SYS_exit 60ULL
#define SYS_write 1ULL

#endif
