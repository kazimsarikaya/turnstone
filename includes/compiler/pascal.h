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

typedef enum pascal_token_type_t {
    PASCAL_TOKEN_TYPE_EOF = 0,
    PASCAL_TOKEN_TYPE_UNKNOWN,
    PASCAL_TOKEN_TYPE_INTEGER_CONST,
    PASCAL_TOKEN_TYPE_PLUS,
    PASCAL_TOKEN_TYPE_MINUS,
    PASCAL_TOKEN_TYPE_MULTIPLY,
    PASCAL_TOKEN_TYPE_DIVIDE,
    PASCAL_TOKEN_TYPE_LPAREN,
    PASCAL_TOKEN_TYPE_RPAREN,
    PASCAL_TOKEN_TYPE_ID,
    PASCAL_TOKEN_TYPE_ASSIGN, // 10
    PASCAL_TOKEN_TYPE_SEMI,
    PASCAL_TOKEN_TYPE_DOT,
    PASCAL_TOKEN_TYPE_COMMA,
    PASCAL_TOKEN_TYPE_COLON,
    PASCAL_TOKEN_TYPE_EQUAL,
    PASCAL_TOKEN_TYPE_BEGIN,
    PASCAL_TOKEN_TYPE_END,
    PASCAL_TOKEN_TYPE_PROGRAM,
    PASCAL_TOKEN_TYPE_VAR,
    PASCAL_TOKEN_TYPE_INTEGER,
} pascal_token_type_t;

typedef struct pascal_token_t {
    pascal_token_type_t type;
    boolean_t           not_free;
    int32_t             value;
    const char_t*       text;
} pascal_token_t;

typedef struct pascal_lexer_t {
    buffer_t * buffer;
    char_t     current_char;
} pascal_lexer_t;

typedef enum pascal_ast_node_type_t {
    PASCAL_AST_NODE_TYPE_INTEGER_CONST = 0,
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
} pascal_ast_node_type_t;

typedef enum pascal_symbol_type_t {
    PASCAL_SYMBOL_TYPE_INTEGER = 0,
} pascal_symbol_type_t;

typedef struct pascal_symol_t {
    const char_t*        name;
    pascal_symbol_type_t type;
    uint32_t             size;
    int64_t              int_value;
    uint16_t             stack_offset;
} pascal_symbol_t;

typedef struct pascal_ast_node_t {
    pascal_ast_node_type_t     type;
    pascal_token_t*            token;
    struct pascal_ast_node_t * left;
    struct pascal_ast_node_t * right;
    linkedlist_t*              children;
    int16_t                    used_register;
} pascal_ast_node_t;

typedef struct pascal_ast_t {
    pascal_ast_node_t * root;
} pascal_ast_t;

typedef struct pascal_parser_t {
    pascal_lexer_t * lexer;
    pascal_token_t * current_token;
} pascal_parser_t;

#define PASCAL_VM_REG_COUNT 14

typedef struct pascal_vm_t {
    pascal_ast_t *   ast;
    const char_t*    program_name;
    pascal_symbol_t* program_name_symbol;
    buffer_t*        asm_buffer;
    buffer_t*        data_buffer;
    hashmap_t*       symbol_table;
    uint16_t         stack_size;
    uint16_t         next_stack_offset;
    boolean_t        is_const;
    boolean_t        is_at_reg;
    boolean_t        is_at_stack;
    uint16_t         at_stack_offset;
    boolean_t        busy_regs[PASCAL_VM_REG_COUNT];
} pascal_vm_t;



int8_t pascal_lexer_init(pascal_lexer_t * lexer, buffer_t * buffer);
int8_t pascal_token_destroy(pascal_token_t * token);

#ifdef ___COMPILER_PASCAL_IMPLEMENTATION
int8_t pascal_lexer_advance(pascal_lexer_t * lexer);
char_t pascal_lexer_peek(pascal_lexer_t * lexer);
int8_t pascal_lexer_skip_whitespace(pascal_lexer_t * lexer);
int8_t pascal_lexer_get_integer(pascal_lexer_t * lexer, int32_t * value);
int8_t pascal_lexer_get_next_token(pascal_lexer_t * lexer, pascal_token_t ** token);
int8_t pascal_lexer_get_id(pascal_lexer_t * lexer, pascal_token_t ** token);


int8_t pascal_symbol_destroyer(memory_heap_t* heap, void* symbol);

#define pascal_destroy_symbol_list(list) linkedlist_destroy_with_type(list, LINKEDLIST_DESTROY_WITH_DATA, pascal_symbol_destroyer)
int8_t pascal_ast_node_destroy(pascal_ast_node_t * node);
int8_t pascal_ast_node_destroyer(memory_heap_t* heap, void* node);

int8_t pascal_parser_eat(pascal_parser_t * parser, pascal_token_type_t type, boolean_t need_free);
int8_t pascal_parser_factor(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_term(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_expr(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_program(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_block(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_decls(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_compound_statement(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_statement(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_assignment_statement(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_variables(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_variable(pascal_parser_t * parser, pascal_ast_node_t ** node);
int8_t pascal_parser_var(pascal_parser_t * parser, pascal_ast_node_t ** node);

#endif


int8_t pascal_ast_init(pascal_ast_t * ast);
int8_t pascal_ast_destroy(pascal_ast_t * ast);

int8_t pascal_parser_init(pascal_parser_t * parser, pascal_lexer_t * lexer);
int8_t pascal_parser_parse(pascal_parser_t * parser, pascal_ast_t * ast);



#endif
