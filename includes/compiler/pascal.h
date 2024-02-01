/**
 * @file pascal.h
 * @brief Turnstone OS pascal compiler header
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___COMPILER_PASCAL_H
#define ___COMPILER_PASCAL_H 0

#include <types.h>
#include <buffer.h>
#include <linkedlist.h>
#include <hashmap.h>

/**
 * @enum pascal_token_type_t **/
typedef enum pascal_token_type_t {
    PASCAL_TOKEN_TYPE_EOF = 0,
    PASCAL_TOKEN_TYPE_UNKNOWN,
    PASCAL_TOKEN_TYPE_INTEGER_CONST,
    PASCAL_TOKEN_TYPE_REAL_CONST,
    PASCAL_TOKEN_TYPE_STRING_CONST,
    PASCAL_TOKEN_TYPE_PLUS,
    PASCAL_TOKEN_TYPE_MINUS,
    PASCAL_TOKEN_TYPE_OR,
    PASCAL_TOKEN_TYPE_XOR,
    PASCAL_TOKEN_TYPE_MULTIPLY,
    PASCAL_TOKEN_TYPE_REAL_DIVIDE,
    PASCAL_TOKEN_TYPE_INTEGER_DIVIDE,
    PASCAL_TOKEN_TYPE_MOD,
    PASCAL_TOKEN_TYPE_AND,
    PASCAL_TOKEN_TYPE_NOT,
    PASCAL_TOKEN_TYPE_LPAREN,
    PASCAL_TOKEN_TYPE_RPAREN,
    PASCAL_TOKEN_TYPE_ID,
    PASCAL_TOKEN_TYPE_ASSIGN,
    PASCAL_TOKEN_TYPE_SEMI,
    PASCAL_TOKEN_TYPE_DOT,
    PASCAL_TOKEN_TYPE_COMMA,
    PASCAL_TOKEN_TYPE_COLON,
    PASCAL_TOKEN_TYPE_EQUAL,
    PASCAL_TOKEN_TYPE_NOT_EQUAL,
    PASCAL_TOKEN_TYPE_LESS_THAN,
    PASCAL_TOKEN_TYPE_LESS_THAN_OR_EQUAL,
    PASCAL_TOKEN_TYPE_GREATER_THAN,
    PASCAL_TOKEN_TYPE_GREATER_THAN_OR_EQUAL,
    PASCAL_TOKEN_TYPE_IN,
    PASCAL_TOKEN_TYPE_SHL,
    PASCAL_TOKEN_TYPE_SHR,
    PASCAL_TOKEN_TYPE_BEGIN,
    PASCAL_TOKEN_TYPE_END,
    PASCAL_TOKEN_TYPE_PROGRAM,
    PASCAL_TOKEN_TYPE_PROCEDURE,
    PASCAL_TOKEN_TYPE_FUNCTION,
    PASCAL_TOKEN_TYPE_VAR,
    PASCAL_TOKEN_TYPE_CONST,
    PASCAL_TOKEN_TYPE_INTEGER,
    PASCAL_TOKEN_TYPE_REAL,
    PASCAL_TOKEN_TYPE_STRING,
    PASCAL_TOKEN_TYPE_IF,
    PASCAL_TOKEN_TYPE_THEN,
    PASCAL_TOKEN_TYPE_ELSE,
    PASCAL_TOKEN_TYPE_WHILE,
    PASCAL_TOKEN_TYPE_DO,
    PASCAL_TOKEN_TYPE_REPEAT,
    PASCAL_TOKEN_TYPE_UNTIL,
    PASCAL_TOKEN_TYPE_FOR,
    PASCAL_TOKEN_TYPE_TO,
    PASCAL_TOKEN_TYPE_DOWNTO,
    PASCAL_TOKEN_TYPE_STEP,
    PASCAL_TOKEN_TYPE_CONTINUE,
    PASCAL_TOKEN_TYPE_BREAK,
} pascal_token_type_t;

const char_t* pascal_keywords[] = {
    "program",
    "type",
    "const",
    "var",
    "bit",
    "int8",
    "int16",
    "int32",
    "int64",
    "uint8",
    "uint16",
    "uint32",
    "uint64",
    "float32",
    "float64",
    "begin",
    "end",
    "procedure",
    "function",
    "div",
    "mod",
    "and",
    "or",
    "xor",
    "not",
    "in",
    "nil",
    "true",
    "false",
    "if",
    "then",
    "else",
    "with",
    "while",
    "do",
    "repeat",
    "until",
    "for",
    "to",
    "downto",
    "step",
    "case",
    "of",
    "otherwise",
    "break",
    "continue",
    "exit",
    "return",
    "sizeof",
    "typeof",
    "offsetof",
    "sizeof_field",
    "typeof_field",
    "offsetof_field",

};

typedef struct pascal_token_t {
    pascal_token_type_t type;
    boolean_t           not_free;
    int64_t             value;
    float64_t           real_value;
    const char_t*       text;
    boolean_t           is_const;
    uint64_t            size;
    boolean_t           is_unsigned;
    boolean_t           is_array;
} pascal_token_t;

typedef struct pascal_lexer_t {
    buffer_t * buffer;
    char_t     current_char;
} pascal_lexer_t;

typedef enum pascal_ast_node_type_t {
    PASCAL_AST_NODE_TYPE_INTEGER_CONST = 0,
    PASCAL_AST_NODE_TYPE_REAL_CONST,
    PASCAL_AST_NODE_TYPE_STRING_CONST,
    PASCAL_AST_NODE_TYPE_RELATIONAL_OP,
    PASCAL_AST_NODE_TYPE_BINARY_OP,
    PASCAL_AST_NODE_TYPE_UNARY_OP,
    PASCAL_AST_NODE_TYPE_NO_OP,
    PASCAL_AST_NODE_TYPE_ASSIGN,
    PASCAL_AST_NODE_TYPE_PROGRAM,
    PASCAL_AST_NODE_TYPE_DECLS,
    PASCAL_AST_NODE_TYPE_VAR,
    PASCAL_AST_NODE_TYPE_LVAR,
    PASCAL_AST_NODE_TYPE_BLOCK,
    PASCAL_AST_NODE_TYPE_COMPOUND,
    PASCAL_AST_NODE_TYPE_FUNCTION_CALL,
    PASCAL_AST_NODE_TYPE_IF,
    PASCAL_AST_NODE_TYPE_WHILE,
    PASCAL_AST_NODE_TYPE_REPEAT,
} pascal_ast_node_type_t;

typedef enum pascal_symbol_type_t {
    PASCAL_SYMBOL_TYPE_UNKNOWN = -1,
    PASCAL_SYMBOL_TYPE_VOID = 0,
    PASCAL_SYMBOL_TYPE_BOOLEAN,
    PASCAL_SYMBOL_TYPE_INTEGER,
    PASCAL_SYMBOL_TYPE_REAL,
    PASCAL_SYMBOL_TYPE_STRING,
} pascal_symbol_type_t;

typedef struct pascal_symol_t {
    const char_t*        name;
    pascal_symbol_type_t type;
    int64_t              size;
    int64_t              int_value;
    float64_t            real_value;
    const char_t*        string_value;
    uint16_t             stack_offset;
    boolean_t            is_const;
    boolean_t            is_local;
} pascal_symbol_t;

typedef struct pascal_ast_node_t pascal_ast_node_t;

struct pascal_ast_node_t {
    pascal_ast_node_type_t type;
    pascal_token_t*        token;
    pascal_ast_node_t*     left;
    pascal_ast_node_t*     right;
    pascal_ast_node_t*     condition;
    linkedlist_t*          children;
    int16_t                used_register;
    pascal_symbol_t*       symbol;
};

typedef struct pascal_ast_t {
    pascal_ast_node_t * root;
} pascal_ast_t;

typedef struct pascal_parser_t {
    pascal_lexer_t * lexer;
    pascal_token_t * current_token;
} pascal_parser_t;

#define PASCAL_VM_REG_COUNT 13

typedef struct symbol_table_t symbol_table_t;

struct symbol_table_t {
    symbol_table_t* parent;
    hashmap_t*      symbols;
};

typedef struct pascal_compiler_t {
    pascal_ast_t *       ast;
    const char_t*        program_name;
    pascal_symbol_t*     program_name_symbol;
    buffer_t*            text_buffer;
    buffer_t*            data_buffer;
    buffer_t*            rodata_buffer;
    buffer_t*            bss_buffer;
    symbol_table_t*      main_symbol_table;
    symbol_table_t*      current_symbol_table;
    uint16_t             stack_size;
    uint16_t             next_stack_offset;
    boolean_t            is_const;
    boolean_t            is_at_reg;
    boolean_t            is_at_mem;
    boolean_t            is_at_stack;
    uint16_t             at_stack_offset;
    boolean_t            busy_regs[PASCAL_VM_REG_COUNT];
    int32_t              next_label_id;
    linkedlist_t*        cond_label_stack;
    int64_t              cond_depth;
    linkedlist_t*        loop_label_stack;
    int64_t              loop_depth;
    boolean_t            is_cond_eval;
    boolean_t            is_cond_reverse;
    int64_t              computed_size;
    pascal_symbol_type_t computed_type;
} pascal_compiler_t;



int8_t pascal_lexer_init(pascal_lexer_t * lexer, buffer_t * buffer);
int8_t pascal_token_destroy(pascal_token_t * token);

#ifdef ___COMPILER_PASCAL_IMPLEMENTATION
int8_t pascal_lexer_advance(pascal_lexer_t * lexer);
char_t pascal_lexer_peek(pascal_lexer_t * lexer);
int8_t pascal_lexer_skip_whitespace(pascal_lexer_t * lexer);
int8_t pascal_lexer_get_number(pascal_lexer_t * lexer, pascal_token_t ** token);
int8_t pascal_lexer_get_next_token(pascal_lexer_t * lexer, pascal_token_t ** token);
int8_t pascal_lexer_get_id(pascal_lexer_t * lexer, pascal_token_t ** token);
int8_t pascal_lexer_get_string(pascal_lexer_t * lexer, pascal_token_t ** token);


int8_t pascal_symbol_destroyer(memory_heap_t* heap, void* symbol);

#define pascal_destroy_symbol_list(list) linkedlist_destroy_with_type(list, LINKEDLIST_DESTROY_WITH_DATA, pascal_symbol_destroyer)
int8_t pascal_ast_node_destroy(pascal_ast_node_t * node);
int8_t pascal_ast_node_destroyer(memory_heap_t* heap, void* node);

int8_t pascal_parser_eat(pascal_parser_t * parser, pascal_token_type_t type, boolean_t need_free);
int8_t pascal_parser_factor(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_term(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_expr(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_simple_expr(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_program(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_block(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_decls(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_compound_statement(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_statement(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_assignment_statement(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_variables(pascal_parser_t * parser, pascal_ast_node_t ** node, boolean_t is_const, boolean_t is_local);
int8_t pascal_parser_variable(pascal_parser_t * parser, pascal_ast_node_t ** node, boolean_t is_const, boolean_t is_local);
int8_t pascal_parser_var(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_function_call(pascal_parser_t* parser, pascal_ast_node_t** node);
int8_t pascal_parser_if_statement(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_while_statement(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_repeat_statement(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_for_statement(pascal_parser_t * parser, pascal_ast_node_t ** node);

#endif


int8_t pascal_ast_init(pascal_ast_t * ast);
int8_t pascal_ast_destroy(pascal_ast_t * ast);

int8_t pascal_parser_init(pascal_parser_t * parser, pascal_lexer_t * lexer);
int8_t pascal_parser_parse(pascal_parser_t * parser, pascal_ast_t * ast);



#endif
