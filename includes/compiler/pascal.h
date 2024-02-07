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
#include <compiler/compiler.h>

typedef struct pascal_lexer_t {
    buffer_t * buffer;
    char_t     current_char;
} pascal_lexer_t;


typedef struct pascal_parser_t {
    pascal_lexer_t *   lexer;
    compiler_token_t * current_token;
} pascal_parser_t;


int8_t pascal_lexer_init(pascal_lexer_t * lexer, buffer_t * buffer);

int8_t pascal_lexer_advance(pascal_lexer_t * lexer);
char_t pascal_lexer_peek(pascal_lexer_t * lexer);
int8_t pascal_lexer_skip_whitespace(pascal_lexer_t * lexer);
int8_t pascal_lexer_get_number(pascal_lexer_t * lexer, compiler_token_t ** token);
int8_t pascal_lexer_get_next_token(pascal_lexer_t * lexer, compiler_token_t ** token);
int8_t pascal_lexer_get_id(pascal_lexer_t * lexer, compiler_token_t ** token);
int8_t pascal_lexer_get_string(pascal_lexer_t * lexer, compiler_token_t ** token);


int8_t pascal_parser_eat(pascal_parser_t * parser, compiler_token_type_t type, boolean_t need_free);
int8_t pascal_parser_factor(pascal_parser_t * parser, compiler_ast_node_t ** node);
int8_t pascal_parser_term(pascal_parser_t * parser, compiler_ast_node_t ** node);
int8_t pascal_parser_expr(pascal_parser_t * parser, compiler_ast_node_t ** node);
int8_t pascal_parser_simple_expr(pascal_parser_t * parser, compiler_ast_node_t ** node);
int8_t pascal_parser_program(pascal_parser_t * parser, compiler_ast_node_t ** node);
int8_t pascal_parser_block(pascal_parser_t * parser, compiler_ast_node_t ** node);
int8_t pascal_parser_decls(pascal_parser_t * parser, compiler_ast_node_t ** node);
int8_t pascal_parser_compound_statement(pascal_parser_t * parser, compiler_ast_node_t ** node);
int8_t pascal_parser_statement(pascal_parser_t * parser, compiler_ast_node_t ** node);
int8_t pascal_parser_assignment_statement(pascal_parser_t * parser, compiler_ast_node_t ** node);
int8_t pascal_parser_variables(pascal_parser_t * parser, compiler_ast_node_t ** node, boolean_t is_const, boolean_t is_local);
int8_t pascal_parser_variable(pascal_parser_t * parser, compiler_ast_node_t ** node, boolean_t is_const, boolean_t is_local);
int8_t pascal_parser_var(pascal_parser_t * parser, compiler_ast_node_t ** node);
int8_t pascal_parser_function_call(pascal_parser_t* parser, compiler_ast_node_t** node);
int8_t pascal_parser_if_statement(pascal_parser_t * parser, compiler_ast_node_t ** node);
int8_t pascal_parser_while_statement(pascal_parser_t * parser, compiler_ast_node_t ** node);
int8_t pascal_parser_repeat_statement(pascal_parser_t * parser, compiler_ast_node_t ** node);
int8_t pascal_parser_for_statement(pascal_parser_t * parser, compiler_ast_node_t ** node);

int8_t pascal_parser_init(pascal_parser_t * parser, pascal_lexer_t * lexer);
int8_t pascal_parser_parse(pascal_parser_t * parser, compiler_ast_t * ast);



#endif
