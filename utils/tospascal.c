/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAM_SIZE 0x1000000 // 16 MB
#include "setup.h"
#include <buffer.h>
#include <linkedlist.h>
#include <hashmap.h>
#include <logging.h>
#include <strings.h>
#include <utils.h>


int32_t main(int32_t argc, char * argv[]);

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

int8_t pascal_token_destroy(pascal_token_t * token);

int8_t pascal_token_destroy(pascal_token_t * token) {
    if (token == NULL) {
        return -1;
    }

    if (token->not_free == false) {
        if (token->text != NULL) {
            memory_free((void*)token->text);
        }

        memory_free(token);
    }

    return 0;
}

pascal_token_t reserved_tokens[] = {
    {PASCAL_TOKEN_TYPE_BEGIN, true, 0, "BEGIN"},
    {PASCAL_TOKEN_TYPE_END, true, 0, "END"},
    {PASCAL_TOKEN_TYPE_PROGRAM, true, 0, "PROGRAM"},
    {PASCAL_TOKEN_TYPE_VAR, true, 0, "VAR"},
    {PASCAL_TOKEN_TYPE_INTEGER, true, 0, "INTEGER"},
};

typedef struct pascal_lexer_t {
    buffer_t * buffer;
    char_t     current_char;
} pascal_lexer_t;

int8_t pascal_lexer_init(pascal_lexer_t * lexer, buffer_t * buffer);
int8_t pascal_lexer_advance(pascal_lexer_t * lexer);
char_t pascal_lexer_peek(pascal_lexer_t * lexer);
int8_t pascal_lexer_skip_whitespace(pascal_lexer_t * lexer);
int8_t pascal_lexer_get_integer(pascal_lexer_t * lexer, int32_t * value);
int8_t pascal_lexer_get_next_token(pascal_lexer_t * lexer, pascal_token_t ** token);
int8_t pascal_lexer_get_id(pascal_lexer_t * lexer, pascal_token_t ** token);


int8_t pascal_lexer_init(pascal_lexer_t * lexer, buffer_t * buffer) {
    if (buffer == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "buffer is null");

        return -1;
    }

    lexer->buffer = buffer;
    lexer->current_char = buffer_get_byte(lexer->buffer);

    return 0;
}

int8_t pascal_lexer_advance(pascal_lexer_t * lexer) {
    if (lexer->current_char != '\0') {
        lexer->current_char = buffer_get_byte(lexer->buffer);
    }

    return 0;
}

char_t pascal_lexer_peek(pascal_lexer_t * lexer) {
    return buffer_peek_byte(lexer->buffer);
}

int8_t pascal_lexer_skip_whitespace(pascal_lexer_t * lexer) {
    while (lexer->current_char != '\0' && isspace(lexer->current_char)) {
        pascal_lexer_advance(lexer);
    }

    return 0;
}

int8_t pascal_lexer_get_integer(pascal_lexer_t * lexer, int32_t * value) {
    *value = 0;

    while (isdigit(lexer->current_char)) {
        *value = *value * 10 + (lexer->current_char - '0');
        pascal_lexer_advance(lexer);
    }

    return 0;
}

int8_t pascal_lexer_get_id(pascal_lexer_t * lexer, pascal_token_t ** token) {
    buffer_t * buffer = buffer_new();

    if (buffer == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create id buffer");

        return -1;
    }

    while (isalnum(lexer->current_char)) {
        buffer_append_byte(buffer, lexer->current_char);
        pascal_lexer_advance(lexer);
    }

    buffer_append_byte(buffer, '\0');

    char_t* token_text = (char_t*)buffer_get_all_bytes(buffer, NULL);

    buffer_destroy(buffer);


    for (size_t i = 0; i < sizeof(reserved_tokens) / sizeof(pascal_token_t); i++) {
        if (strcmp(reserved_tokens[i].text, token_text) == 0) {
            *token = &reserved_tokens[i];

            memory_free((void*)token_text);

            return 0;
        }
    }


    pascal_token_t * new_token = memory_malloc(sizeof(pascal_token_t));

    if (new_token == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

        buffer_destroy(buffer);
        return -1;
    }

    new_token->type = PASCAL_TOKEN_TYPE_ID;
    new_token->text = token_text;
    new_token->not_free = false;


    *token = new_token;

    return 0;
}

int8_t pascal_lexer_get_next_token(pascal_lexer_t * lexer, pascal_token_t ** token) {
    while (lexer->current_char != '\0') {
        if (isspace(lexer->current_char)) {
            pascal_lexer_skip_whitespace(lexer);
            continue;
        }

        if(isalpha(lexer->current_char)) {
            return pascal_lexer_get_id(lexer, token);
        }

        if(lexer->current_char == ':') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            if(pascal_lexer_peek(lexer) == '=') {
                (*token)->type = PASCAL_TOKEN_TYPE_ASSIGN;
                pascal_lexer_advance(lexer);
                pascal_lexer_advance(lexer);
                return 0;
            } else {
                (*token)->type = PASCAL_TOKEN_TYPE_COLON;
                pascal_lexer_advance(lexer);
                return 0;
            }
        }

        if(lexer->current_char == ';') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_SEMI;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if(lexer->current_char == '.') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_DOT;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if(lexer->current_char == ',') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_COMMA;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if(lexer->current_char == '=') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_EQUAL;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if (isdigit(lexer->current_char)) {
            *token = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_INTEGER_CONST;
            pascal_lexer_get_integer(lexer, &(*token)->value);
            return 0;
        }

        if (lexer->current_char == '+') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_PLUS;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if (lexer->current_char == '-') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_MINUS;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if (lexer->current_char == '*') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_MULTIPLY;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if (lexer->current_char == '/') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_DIVIDE;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if (lexer->current_char == '(') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_LPAREN;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if (lexer->current_char == ')') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_RPAREN;
            pascal_lexer_advance(lexer);
            return 0;
        }

        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unknown token");

        return -1;
    }

    (*token) = memory_malloc(sizeof(pascal_token_t));

    if (*token == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

        return -1;
    }

    (*token)->type = PASCAL_TOKEN_TYPE_EOF;

    return 0;
}

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
} pascal_symbol_t;

int8_t pascal_symbol_destroyer(memory_heap_t* heap, void* symbol);

int8_t pascal_symbol_destroyer(memory_heap_t* heap, void* symbol) {
    if (symbol == NULL) {
        return -1;
    }

    pascal_symbol_t * l_symbol = (pascal_symbol_t*)symbol;

    if (l_symbol->name != NULL) {
        memory_free_ext(heap, (void*)l_symbol->name);
    }

    memory_free_ext(heap, symbol);

    return 0;
}

#define pascal_destroy_symbol_list(list) linkedlist_destroy_with_type(list, LINKEDLIST_DESTROY_WITH_DATA, pascal_symbol_destroyer)

typedef struct pascal_ast_node_t {
    pascal_ast_node_type_t     type;
    pascal_token_t*            token;
    struct pascal_ast_node_t * left;
    struct pascal_ast_node_t * right;
    linkedlist_t*              children;
} pascal_ast_node_t;

typedef struct pascal_ast_t {
    pascal_ast_node_t * root;
} pascal_ast_t;

int8_t pascal_ast_init(pascal_ast_t * ast);
int8_t pascal_ast_destroy(pascal_ast_t * ast);
int8_t pascal_ast_node_destroy(pascal_ast_node_t * node);

int8_t pascal_ast_init(pascal_ast_t * ast) {
    ast->root = NULL;
    return 0;
}

int8_t pascal_ast_node_destroyer(memory_heap_t* heap, void* node);

int8_t pascal_ast_node_destroyer(memory_heap_t* heap, void* node) {
    UNUSED(heap);

    if (node == NULL) {
        return -1;
    }

    pascal_ast_node_t * l_node = (pascal_ast_node_t*)node;

    return pascal_ast_node_destroy(l_node);
}

int8_t pascal_ast_node_destroy(pascal_ast_node_t * node) {
    if (node == NULL) {
        return -1;
    }

    if(node->left) {
        pascal_ast_node_destroy(node->left);
    }

    if(node->right) {
        pascal_ast_node_destroy(node->right);
    }

    if(node->type == PASCAL_AST_NODE_TYPE_VAR) {
        pascal_destroy_symbol_list(node->children);
    }

    if(node->type == PASCAL_AST_NODE_TYPE_DECLS || node->type == PASCAL_AST_NODE_TYPE_COMPOUND) {
        linkedlist_destroy_with_type(node->children, LINKEDLIST_DESTROY_WITH_DATA, pascal_ast_node_destroyer);
    }

    if(node->token != NULL && node->token->not_free == false) {
        if(node->token->text != NULL) {
            memory_free((void*)node->token->text);
        }

        memory_free(node->token);
    }

    memory_free(node);

    return 0;
}

int8_t pascal_ast_destroy(pascal_ast_t * ast) {
    // travels from root to leafs and frees nodes
    // in post-order traversal

    pascal_ast_node_t * node = ast->root;

    if (node == NULL) {
        return -1;
    }

    pascal_ast_node_destroy(node);

    return 0;
}

typedef struct pascal_parser_t {
    pascal_lexer_t * lexer;
    pascal_token_t * current_token;
} pascal_parser_t;

int8_t pascal_parser_init(pascal_parser_t * parser, pascal_lexer_t * lexer);
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



int8_t pascal_parser_parse(pascal_parser_t * parser, pascal_ast_t * ast);

int8_t pascal_parser_init(pascal_parser_t * parser, pascal_lexer_t * lexer) {
    if (lexer == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "lexer is null");

        return -1;
    }

    parser->lexer = lexer;
    parser->current_token = NULL;

    return pascal_lexer_get_next_token(parser->lexer, &parser->current_token);
}

int8_t pascal_parser_eat(pascal_parser_t * parser, pascal_token_type_t type, boolean_t need_free) {
    if (parser->current_token->type == type) {
        if (need_free && parser->current_token->not_free == false) {
            pascal_token_destroy(parser->current_token);
        }

        if(type == PASCAL_TOKEN_TYPE_EOF) {
            return 0;
        }

        return pascal_lexer_get_next_token(parser->lexer, &parser->current_token);
    }

    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unexpected token parser_eat. expected %d found %d", type, parser->current_token->type);

    return -1;
}

int8_t pascal_parser_factor(pascal_parser_t * parser, pascal_ast_node_t ** node) {
    if(parser->current_token->type == PASCAL_TOKEN_TYPE_MINUS) {
        pascal_token_t * token = parser->current_token;

        if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_MINUS, false) != 0) {
            return -1;
        }

        pascal_ast_node_t * new_node = memory_malloc(sizeof(pascal_ast_node_t));

        if (new_node == NULL) {
            return -1;
        }

        new_node->type = PASCAL_AST_NODE_TYPE_UNARY_OP;
        new_node->token = token;

        if(pascal_parser_factor(parser, &new_node->right) != 0) {
            return -1;
        }

        *node = new_node;

        return 0;
    } else if (parser->current_token->type == PASCAL_TOKEN_TYPE_PLUS) {
        pascal_token_t * token = parser->current_token;

        if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_PLUS, false) != 0) {
            return -1;
        }

        pascal_ast_node_t * new_node = memory_malloc(sizeof(pascal_ast_node_t));

        if (new_node == NULL) {
            return -1;
        }

        new_node->type = PASCAL_AST_NODE_TYPE_UNARY_OP;
        new_node->token = token;

        if(pascal_parser_factor(parser, &new_node->right) != 0) {
            return -1;
        }

        *node = new_node;

        return 0;
    } else if (parser->current_token->type == PASCAL_TOKEN_TYPE_INTEGER_CONST) {
        *node = memory_malloc(sizeof(pascal_ast_node_t));

        if (*node == NULL) {
            return -1;
        }

        (*node)->type = PASCAL_AST_NODE_TYPE_INTEGER_CONST;
        (*node)->token = parser->current_token;

        return pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_INTEGER_CONST, false);
    } else if (parser->current_token->type == PASCAL_TOKEN_TYPE_LPAREN) {
        if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_LPAREN, true) != 0) {
            return -1;
        }

        if(pascal_parser_expr(parser, node) != 0) {
            return -1;
        }

        return pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_RPAREN, true);
    } else if (parser->current_token->type == PASCAL_TOKEN_TYPE_ID) {
        return pascal_parser_var(parser, node);
    }

    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unexpected token parser_factor. found %d", parser->current_token->type);

    return -1;
}

int8_t pascal_parser_term(pascal_parser_t * parser, pascal_ast_node_t ** node) {
    pascal_ast_node_t * left = NULL;
    pascal_ast_node_t * right = NULL;

    if(pascal_parser_factor(parser, &left) != 0) {
        return -1;
    }

    while (parser->current_token->type == PASCAL_TOKEN_TYPE_MULTIPLY ||
           parser->current_token->type == PASCAL_TOKEN_TYPE_DIVIDE) {
        pascal_token_t * token = parser->current_token;

        if (token->type == PASCAL_TOKEN_TYPE_MULTIPLY) {
            if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_MULTIPLY, false) != 0) {
                pascal_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == PASCAL_TOKEN_TYPE_DIVIDE) {
            if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_DIVIDE, false) != 0) {
                pascal_ast_node_destroy(left);

                return -1;
            }
        }

        if(pascal_parser_factor(parser, &right) != 0) {
            pascal_ast_node_destroy(left);

            return -1;
        }

        pascal_ast_node_t * new_node = memory_malloc(sizeof(pascal_ast_node_t));

        if (new_node == NULL) {
            pascal_ast_node_destroy(left);
            pascal_ast_node_destroy(right);

            return -1;
        }

        new_node->type = PASCAL_AST_NODE_TYPE_BINARY_OP;
        new_node->token = token;
        new_node->left = left;
        new_node->right = right;

        left = new_node;
    }

    *node = left;

    return 0;
}

int8_t pascal_parser_expr(pascal_parser_t * parser, pascal_ast_node_t ** node) {
    pascal_ast_node_t * left = NULL;
    pascal_ast_node_t * right = NULL;

    if(pascal_parser_term(parser, &left) != 0) {
        return -1;
    }

    while (parser->current_token->type == PASCAL_TOKEN_TYPE_PLUS ||
           parser->current_token->type == PASCAL_TOKEN_TYPE_MINUS) {
        pascal_token_t * token = parser->current_token;

        if (token->type == PASCAL_TOKEN_TYPE_PLUS) {
            if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_PLUS, false) != 0) {
                pascal_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == PASCAL_TOKEN_TYPE_MINUS) {
            if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_MINUS, false) != 0) {
                pascal_ast_node_destroy(left);

                return -1;
            }
        }

        if(pascal_parser_term(parser, &right) != 0) {
            pascal_ast_node_destroy(left);

            return -1;
        }

        pascal_ast_node_t * new_node = memory_malloc(sizeof(pascal_ast_node_t));

        if (new_node == NULL) {
            pascal_ast_node_destroy(left);
            pascal_ast_node_destroy(right);

            return -1;
        }

        new_node->type = PASCAL_AST_NODE_TYPE_BINARY_OP;
        new_node->token = token;
        new_node->left = left;
        new_node->right = right;

        left = new_node;
    }

    *node = left;

    return 0;
}

int8_t pascal_parser_variable(pascal_parser_t * parser, pascal_ast_node_t ** node) {
    if(parser->current_token->type != PASCAL_TOKEN_TYPE_ID) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected id");
        return -1;
    }

    linkedlist_t* symbol_list = linkedlist_create_list();

    if (symbol_list == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create symbol list");
        return -1;
    }

    while (parser->current_token->type == PASCAL_TOKEN_TYPE_ID) {
        pascal_token_t * token = parser->current_token;

        pascal_symbol_t * symbol = memory_malloc(sizeof(pascal_symbol_t));

        if (symbol == NULL) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create symbol");
            pascal_destroy_symbol_list(symbol_list);
            return -1;
        }

        symbol->name = strdup(token->text);

        linkedlist_queue_push(symbol_list, symbol);

        if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_ID, true) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected id");
            pascal_destroy_symbol_list(symbol_list);
            return -1;
        }

        if(parser->current_token->type == PASCAL_TOKEN_TYPE_COMMA) {
            if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_COMMA, true) != 0) {
                pascal_destroy_symbol_list(symbol_list);
                return -1;
            }

            continue;
        } else {
            break;
        }
    }

    if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_COLON, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected :");
        pascal_destroy_symbol_list(symbol_list);
        return -1;
    }

    pascal_token_type_t need_token_type = PASCAL_TOKEN_TYPE_UNKNOWN;

    if(parser->current_token->type == PASCAL_TOKEN_TYPE_INTEGER) {

        if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_INTEGER, true) != 0) {
            pascal_destroy_symbol_list(symbol_list);
            return -1;
        }

        for(size_t i = 0; i < linkedlist_size(symbol_list); i++) {
            pascal_symbol_t * tmp_symbol = (pascal_symbol_t*)linkedlist_get_data_at_position(symbol_list, i);

            tmp_symbol->type = PASCAL_SYMBOL_TYPE_INTEGER;
            tmp_symbol->size = 4;
        }

        need_token_type = PASCAL_TOKEN_TYPE_INTEGER_CONST;
    }

    if(parser->current_token->type == PASCAL_TOKEN_TYPE_EQUAL) {
        if(need_token_type == PASCAL_TOKEN_TYPE_UNKNOWN) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected type");
            pascal_destroy_symbol_list(symbol_list);
            return -1;
        }

        if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_EQUAL, true) != 0) {
            pascal_destroy_symbol_list(symbol_list);
            return -1;
        }

        if(parser->current_token->type != need_token_type) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected %d found %d", need_token_type, parser->current_token->type);
            pascal_destroy_symbol_list(symbol_list);
            return -1;
        }

        size_t value_count = linkedlist_size(symbol_list);

        for(size_t i = 0; i < value_count; i++) {
            pascal_symbol_t * tmp_symbol = (pascal_symbol_t*)linkedlist_get_data_at_position(symbol_list, i);

            if(need_token_type == PASCAL_TOKEN_TYPE_INTEGER_CONST) {
                tmp_symbol->int_value = parser->current_token->value;
            }

            if(pascal_parser_eat(parser, need_token_type, true) != 0) {
                pascal_destroy_symbol_list(symbol_list);
                return -1;
            }

            if(i != value_count - 1) {
                if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_COMMA, true) != 0) {
                    pascal_destroy_symbol_list(symbol_list);
                    return -1;
                }
            }
        }
    }

    if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_SEMI, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected ;");
        pascal_destroy_symbol_list(symbol_list);
        return -1;
    }

    pascal_ast_node_t * new_node = memory_malloc(sizeof(pascal_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        pascal_destroy_symbol_list(symbol_list);
        return -1;
    }

    new_node->type = PASCAL_AST_NODE_TYPE_VAR;

    new_node->children = symbol_list;

    *node = new_node;

    return 0;
}

int8_t pascal_parser_variables(pascal_parser_t* parser, pascal_ast_node_t** node) {
    if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_VAR, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected var");
        return -1;
    }

    linkedlist_t* variables_list = linkedlist_create_list();

    while (parser->current_token->type == PASCAL_TOKEN_TYPE_ID) {
        pascal_ast_node_t * new_node = NULL;

        if(pascal_parser_variable(parser, &new_node) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected variable");
            return -1;
        }

        linkedlist_queue_push(variables_list, new_node);
    }

    pascal_ast_node_t* new_node = memory_malloc(sizeof(pascal_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    new_node->type = PASCAL_AST_NODE_TYPE_NO_OP;

    if(linkedlist_size(variables_list) > 0) {
        new_node->children = variables_list;
        new_node->type = PASCAL_AST_NODE_TYPE_DECLS;
    } else {
        linkedlist_destroy(variables_list);
    }

    *node = new_node;

    return 0;
}

int8_t pascal_parser_decls(pascal_parser_t * parser, pascal_ast_node_t ** node) {
    pascal_ast_node_t * new_node = NULL;

    if(parser->current_token->type == PASCAL_TOKEN_TYPE_VAR) {
        if(pascal_parser_variables(parser, &new_node) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected variables");
            pascal_ast_node_destroy(new_node);
            return -1;
        }
    } else {
        memory_malloc(sizeof(pascal_ast_node_t));

        if (new_node == NULL) {
            return -1;
        }

        new_node->type = PASCAL_AST_NODE_TYPE_NO_OP;
    }

    *node = new_node;

    return 0;
}

int8_t pascal_parser_var(pascal_parser_t * parser, pascal_ast_node_t ** node) {
    if(parser->current_token->type != PASCAL_TOKEN_TYPE_ID) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected id");
        return -1;
    }

    pascal_ast_node_t * new_node = memory_malloc(sizeof(pascal_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    new_node->type = PASCAL_AST_NODE_TYPE_VAR;
    new_node->token = parser->current_token;

    if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_ID, false) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected id");
        pascal_ast_node_destroy(new_node);
        return -1;
    }

    *node = new_node;

    return 0;
}

int8_t pascal_parser_assignment_statement(pascal_parser_t * parser, pascal_ast_node_t ** node) {
    pascal_ast_node_t * left = NULL;

    if(pascal_parser_var(parser, &left) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected lvar");
        return -1;
    }

    if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_ASSIGN, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected :=");
        pascal_ast_node_destroy(left);
        return -1;
    }

    pascal_ast_node_t * right = NULL;

    if(pascal_parser_expr(parser, &right) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected expression");
        pascal_ast_node_destroy(left);
        return -1;
    }

    pascal_ast_node_t * new_node = memory_malloc(sizeof(pascal_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        pascal_ast_node_destroy(left);
        pascal_ast_node_destroy(right);
        return -1;
    }

    new_node->type = PASCAL_AST_NODE_TYPE_ASSIGN;
    new_node->left = left;
    new_node->right = right;

    *node = new_node;

    return 0;
}

int8_t pascal_parser_statement(pascal_parser_t * parser, pascal_ast_node_t ** node) {
    if(parser->current_token->type == PASCAL_TOKEN_TYPE_BEGIN) {
        return pascal_parser_compound_statement(parser, node);
    } else if (parser->current_token->type == PASCAL_TOKEN_TYPE_ID) {
        return pascal_parser_assignment_statement(parser, node);
    }

    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unexpected token parser_statement. found %d", parser->current_token->type);

    return -1;
}

int8_t pascal_parser_compound_statement(pascal_parser_t * parser, pascal_ast_node_t ** node) {
    if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_BEGIN, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected begin");
        return -1;
    }

    linkedlist_t* children_list = linkedlist_create_list();

    if (children_list == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create children list");
        return -1;
    }

    while (parser->current_token->type != PASCAL_TOKEN_TYPE_END) {
        pascal_ast_node_t * new_node = NULL;

        if(pascal_parser_statement(parser, &new_node) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected statement");
            linkedlist_destroy_with_type(children_list, LINKEDLIST_DESTROY_WITH_DATA, pascal_ast_node_destroyer);
            return -1;
        }

        linkedlist_queue_push(children_list, new_node);

        if(parser->current_token->type != PASCAL_TOKEN_TYPE_END) {
            if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_SEMI, true) != 0) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected ;");
                linkedlist_destroy_with_type(children_list, LINKEDLIST_DESTROY_WITH_DATA, pascal_ast_node_destroyer);
                return -1;
            }
        }
    }

    if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_END, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected end");
        return -1;
    }

    pascal_ast_node_t * new_node = memory_malloc(sizeof(pascal_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    new_node->type = PASCAL_AST_NODE_TYPE_NO_OP;

    if(linkedlist_size(children_list) > 0) {
        new_node->children = children_list;
        new_node->type = PASCAL_AST_NODE_TYPE_COMPOUND;
    } else {
        linkedlist_destroy(children_list);
    }

    *node = new_node;

    return 0;
}

int8_t pascal_parser_block(pascal_parser_t * parser, pascal_ast_node_t ** node) {
    pascal_ast_node_t * new_node = memory_malloc(sizeof(pascal_ast_node_t));

    if (new_node == NULL) {
        return -1;
    }

    new_node->type = PASCAL_AST_NODE_TYPE_BLOCK;

    if(pascal_parser_decls(parser, &new_node->left) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected decls");
        pascal_ast_node_destroy(new_node);
        return -1;
    }

    if(pascal_parser_compound_statement(parser, &new_node->right) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected compound statement");
        pascal_ast_node_destroy(new_node);
        return -1;
    }

    *node = new_node;

    return 0;
}

int8_t pascal_parser_program(pascal_parser_t * parser, pascal_ast_node_t ** node) {
    if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_PROGRAM, true) != 0) {
        return -1;
    }

    pascal_ast_node_t * new_node = memory_malloc(sizeof(pascal_ast_node_t));

    if (new_node == NULL) {
        return -1;
    }

    new_node->type = PASCAL_AST_NODE_TYPE_PROGRAM;
    new_node->token = parser->current_token;

    if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_ID, false) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected program id");
        pascal_ast_node_destroy(new_node);
        return -1;
    }

    if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_SEMI, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected ;");
        pascal_ast_node_destroy(new_node);
        return -1;
    }

    if(pascal_parser_block(parser, &new_node->left) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected block");
        pascal_ast_node_destroy(new_node);
        return -1;
    }

    if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_DOT, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected .");
        pascal_ast_node_destroy(new_node);
        return -1;
    }

    if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_EOF, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected eof");
        pascal_ast_node_destroy(new_node);
        return -1;
    }

    *node = new_node;

    return 0;
}

int8_t pascal_parser_parse(pascal_parser_t * parser, pascal_ast_t * ast) {
    return pascal_parser_program(parser, &ast->root);
}


typedef struct pascal_vm_t {
    pascal_ast_t *   ast;
    const char_t*    program_name;
    pascal_symbol_t* program_name_symbol;
    buffer_t*        asm_buffer;
    buffer_t*        data_buffer;
    hashmap_t*       symbol_table;
} pascal_vm_t;

int8_t pascal_vm_init(pascal_vm_t * vm, pascal_ast_t * ast);
int8_t pascal_vm_destroy(pascal_vm_t * vm);
int8_t pascal_vm_execute_ast_node(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result);
int8_t pascal_vm_execure_unary_op(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result);
int8_t pascal_vm_execure_binary_op(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result);
int8_t pascal_vm_execute_block(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result);
int8_t pascal_vm_execute_compound(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result);
int8_t pascal_vm_execute_assign(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result);
int8_t pascal_vm_execute(pascal_vm_t * vm, int32_t * result);
int8_t pascal_vm_print_symbol_table(pascal_vm_t * vm);

int8_t pascal_vm_init(pascal_vm_t * vm, pascal_ast_t * ast) {
    if (ast == NULL) {
        return -1;
    }

    vm->asm_buffer = buffer_new();

    if (vm->asm_buffer == NULL) {
        return -1;
    }

    vm->data_buffer = buffer_new();

    if (vm->data_buffer == NULL) {
        buffer_destroy(vm->asm_buffer);
        return -1;
    }

    vm->symbol_table = hashmap_string(128);

    if (vm->symbol_table == NULL) {
        buffer_destroy(vm->asm_buffer);
        buffer_destroy(vm->data_buffer);
        return -1;
    }

    vm->ast = ast;
    return 0;
}

int8_t pascal_vm_print_symbol_table(pascal_vm_t * vm) {
    if (vm->symbol_table == NULL) {
        return -1;
    }

    iterator_t* iter = hashmap_iterator_create(vm->symbol_table);

    if (iter == NULL) {
        return -1;
    }

    while (iter->end_of_iterator(iter) != 0) {
        const pascal_symbol_t* symbol = iter->get_item(iter);

        if (symbol == NULL) {
            iter->destroy(iter);
            return -1;
        }

        PRINTLOG(COMPILER_PASCAL, LOG_INFO, "symbol %s type %d size %d value %lli", symbol->name, symbol->type, symbol->size, symbol->int_value);

        iter->next(iter);
    }

    iter->destroy(iter);

    return 0;
}

int8_t pascal_vm_destroy(pascal_vm_t * vm) {
    if (vm->asm_buffer != NULL) {
        buffer_destroy(vm->asm_buffer);
    }

    if (vm->data_buffer != NULL) {
        buffer_destroy(vm->data_buffer);
    }

    if(vm->symbol_table != NULL) {
        hashmap_destroy(vm->symbol_table);
    }

    memory_free(vm->program_name_symbol);

    pascal_ast_destroy(vm->ast);
    return 0;
}

int8_t pascal_vm_execute_block(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result) {
    if(node->left) {
        if(node->left->type == PASCAL_AST_NODE_TYPE_DECLS) {

            for(size_t i = 0; i < linkedlist_size(node->left->children); i++) {
                pascal_ast_node_t * tmp_node = (pascal_ast_node_t*)linkedlist_get_data_at_position(node->left->children, i);

                for(size_t j = 0; j < linkedlist_size(tmp_node->children); j++) {
                    pascal_symbol_t * tmp_symbol = (pascal_symbol_t*)linkedlist_get_data_at_position(tmp_node->children, j);

                    if(strcmp(vm->program_name, tmp_symbol->name) != 0 && hashmap_get(vm->symbol_table, tmp_symbol->name) != NULL) {
                        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "symbol %s already defined", tmp_symbol->name);
                        return -1;
                    }

                    if(tmp_symbol->type == PASCAL_SYMBOL_TYPE_INTEGER) {
                        buffer_printf(vm->data_buffer, ".align 8\n");
                        buffer_printf(vm->data_buffer, ".type %s,@object\n", tmp_symbol->name);
                        buffer_printf(vm->data_buffer, ".size %s,8\n", tmp_symbol->name);
                        buffer_printf(vm->data_buffer, "%s: .quad %lli\n", tmp_symbol->name, tmp_symbol->int_value);
                    }

                    hashmap_put(vm->symbol_table, tmp_symbol->name, tmp_symbol);
                }
            }
        } else {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected decls");
            return -1;
        }
    } else {
        PRINTLOG(COMPILER_PASCAL, LOG_INFO, "no decls");
    }

    return pascal_vm_execute_ast_node(vm, node->right, result);
}

int8_t pascal_vm_execure_unary_op(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result) {
    int32_t right = 0;

    if(pascal_vm_execute_ast_node(vm, node->right, &right) != 0) {
        return -1;
    }

    if (node->token->type == PASCAL_TOKEN_TYPE_PLUS) {
        *result = +right;
    } else if (node->token->type == PASCAL_TOKEN_TYPE_MINUS) {
        *result = -right;
        buffer_printf(vm->asm_buffer, "\tpop %%rax\n");
        buffer_printf(vm->asm_buffer, "\tneg %%rax\n");
        buffer_printf(vm->asm_buffer, "\tpush %%rax\n");
    } else {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unknown unary op");
        return -1;
    }

    return 0;
}

int8_t pascal_vm_execure_binary_op(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result) {
    int32_t left = 0;
    int32_t right = 0;

    if(pascal_vm_execute_ast_node(vm, node->left, &left) != 0) {
        return -1;
    }

    if(pascal_vm_execute_ast_node(vm, node->right, &right) != 0) {
        return -1;
    }

    if (node->token->type == PASCAL_TOKEN_TYPE_PLUS) {
        *result = left + right;

        buffer_printf(vm->asm_buffer, "\tpop %%rax\n");
        buffer_printf(vm->asm_buffer, "\tpop %%rbx\n");
        buffer_printf(vm->asm_buffer, "\tadd %%rbx, %%rax\n");
        buffer_printf(vm->asm_buffer, "\tpush %%rax\n");
    } else if (node->token->type == PASCAL_TOKEN_TYPE_MINUS) {
        *result = left - right;

        buffer_printf(vm->asm_buffer, "\tpop %%rax\n");
        buffer_printf(vm->asm_buffer, "\tpop %%rbx\n");
        buffer_printf(vm->asm_buffer, "\tsub %%rax, %%rbx\n");
        buffer_printf(vm->asm_buffer, "\tpush %%rbx\n");
    } else if (node->token->type == PASCAL_TOKEN_TYPE_MULTIPLY) {
        *result = left * right;

        buffer_printf(vm->asm_buffer, "\tpop %%rax\n");
        buffer_printf(vm->asm_buffer, "\tpop %%rbx\n");
        buffer_printf(vm->asm_buffer, "\timul %%rbx, %%rax\n");
        buffer_printf(vm->asm_buffer, "\tpush %%rax\n");
    } else if (node->token->type == PASCAL_TOKEN_TYPE_DIVIDE) {
        *result = left / right;

        buffer_printf(vm->asm_buffer, "\tpop %%rbx\n");
        buffer_printf(vm->asm_buffer, "\tpop %%rax\n");
        buffer_printf(vm->asm_buffer, "\txor %%rdx, %%rdx\n");
        buffer_printf(vm->asm_buffer, "\tidiv %%rbx\n");
        buffer_printf(vm->asm_buffer, "\tpush %%rax\n");
    } else {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unknown binary op");
        return -1;
    }

    return 0;
}

int8_t pascal_vm_execute_compound(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result) {
    if(node->children == NULL) {
        return 0;
    }

    for(size_t i = 0; i < linkedlist_size(node->children); i++) {
        pascal_ast_node_t * tmp_node = (pascal_ast_node_t*)linkedlist_get_data_at_position(node->children, i);

        if(pascal_vm_execute_ast_node(vm, tmp_node, result) != 0) {
            return -1;
        }
    }

    return 0;
}

int8_t pascal_vm_execute_assign(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result) {
    const pascal_symbol_t * symbol = hashmap_get(vm->symbol_table, node->left->token->text);

    if(symbol == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "symbol %s not found", node->left->token->text);
        return -1;
    }

    if(pascal_vm_execute_ast_node(vm, node->right, result) != 0) {
        return -1;
    }

    ((pascal_symbol_t*)symbol)->int_value = *result;

    buffer_printf(vm->asm_buffer, "\tpop %%rax\n");
    buffer_printf(vm->asm_buffer, "\tmov %%rax, %s\n", symbol->name);

    return 0;
}

int8_t pascal_vm_execute_ast_node(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result) {
    if (node == NULL) {
        return -1;
    }

    if(node->type == PASCAL_AST_NODE_TYPE_NO_OP) {
        return 0;
    } else if(node->type == PASCAL_AST_NODE_TYPE_PROGRAM) {
        pascal_symbol_t * symbol = memory_malloc(sizeof(pascal_symbol_t));

        if (symbol == NULL) {
            return -1;
        }

        symbol->name = node->token->text;
        symbol->type = PASCAL_SYMBOL_TYPE_INTEGER;
        symbol->size = 8;
        symbol->int_value = 0;

        hashmap_put(vm->symbol_table, symbol->name, symbol);

        vm->program_name = symbol->name;
        vm->program_name_symbol = symbol;

        return pascal_vm_execute_ast_node(vm, node->left, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_BLOCK) {
        return pascal_vm_execute_block(vm, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_COMPOUND) {
        return pascal_vm_execute_compound(vm, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_ASSIGN) {
        return pascal_vm_execute_assign(vm, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_VAR) {
        const pascal_symbol_t * symbol = hashmap_get(vm->symbol_table, node->token->text);

        if(symbol == NULL) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "symbol %s not found", node->token->text);
            return -1;
        }

        *result = symbol->int_value;

        buffer_printf(vm->asm_buffer, "\tpush %s\n", symbol->name);
    } else if (node->type == PASCAL_AST_NODE_TYPE_INTEGER_CONST) {
        *result = node->token->value;
        buffer_printf(vm->asm_buffer, "\tpush $%d\n", *result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_UNARY_OP) {
        return pascal_vm_execure_unary_op(vm, node, result);
    } else if (node->type == PASCAL_AST_NODE_TYPE_BINARY_OP) {
        return pascal_vm_execure_binary_op(vm, node, result);
    } else {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unknown node type %d", node->type);
        return -1;
    }

    return 0;
}

#define SYS_exit 60UL

int8_t pascal_vm_execute(pascal_vm_t * vm, int32_t * result) {
    pascal_ast_node_t * node = vm->ast->root;

    if (node == NULL) {
        return -1;
    }

    buffer_printf(vm->data_buffer, ".data\n");

    buffer_printf(vm->asm_buffer, ".text\n");
    buffer_printf(vm->asm_buffer, ".globl _start\n");
    buffer_printf(vm->asm_buffer, ".type _start, @function\n");
    buffer_printf(vm->asm_buffer, "_start:\n");

    if(pascal_vm_execute_ast_node(vm, node, result) != 0) {
        return -1;
    }

    buffer_printf(vm->data_buffer, ".align 8\n");
    buffer_printf(vm->data_buffer, ".type %s,@object\n", vm->program_name_symbol->name);
    buffer_printf(vm->data_buffer, ".size %s,8\n", vm->program_name_symbol->name);
    buffer_printf(vm->data_buffer, "%s: .quad %d\n", vm->program_name_symbol->name, 0);


    buffer_printf(vm->asm_buffer, "\tmov %s, %%rax\n", vm->program_name);
    buffer_printf(vm->asm_buffer, "\tmov %%rax, %%rdi\n");
    buffer_printf(vm->asm_buffer, "\tmov $0x%lx, %%rax\n", SYS_exit);
    buffer_printf(vm->asm_buffer, "\tsyscall\n");

    buffer_printf(vm->asm_buffer, ".size _start, .-_start\n");

    buffer_printf(vm->asm_buffer, "%s", buffer_get_view_at_position(vm->data_buffer, 0, buffer_get_length(vm->data_buffer)));

    return 0;
}



int32_t main(int32_t argc, char * argv[]) {
    if (argc != 3) {
        print_error("Usage: %s <in> <out>\n", argv[0]);
        return -1;
    }

    FILE* in = fopen(argv[1], "r");
    fseek(in, 0, SEEK_END);
    size_t size = ftell(in);
    fseek(in, 0, SEEK_SET);

    char_t* source = memory_malloc(size + 1);

    if (source == NULL) {
        print_error("Error: Failed to allocate memory for source\n");
        return -1;
    }

    fread(source, 1, size, in);

    source[size] = '\0';

    fclose(in);

    buffer_t * buffer = buffer_encapsulate((uint8_t*)source, size + 1);

    pascal_lexer_t lexer = {0};
    pascal_lexer_init(&lexer, buffer);

    pascal_ast_t ast = {0};
    pascal_ast_init(&ast);

    pascal_parser_t parser = {0};
    pascal_parser_init(&parser, &lexer);

    if(pascal_parser_parse(&parser, &ast) != 0) {
        print_error("Error: Parsing failed\n");
    }

    memory_free(source);
    buffer_destroy(buffer);

    pascal_vm_t vm = {0};

    if(pascal_vm_init(&vm, &ast) != 0) {
        print_error("Error: Failed to initialize vm\n");

        return -1;
    }

    int32_t result = 0;

    if(pascal_vm_execute(&vm, &result) != 0) {
        print_error("Error: Failed to execute vm\n");

        pascal_vm_print_symbol_table(&vm);

        pascal_vm_destroy(&vm);

        return -1;
    }

    pascal_vm_print_symbol_table(&vm);
    PRINTLOG(COMPILER_PASCAL, LOG_INFO, "Result: %d", result);

    FILE* out = fopen(argv[2], "w");

    if (out == NULL) {
        print_error("Error: Failed to open output file\n");

        pascal_vm_destroy(&vm);

        return -1;
    }

    size_t out_size = 0;

    uint8_t* out_bytes = buffer_get_all_bytes(vm.asm_buffer, &out_size);

    fwrite(out_bytes, 1, out_size, out);

    fclose(out);

    memory_free(out_bytes);

    pascal_vm_destroy(&vm);

    PRINTLOG(COMPILER_PASCAL, LOG_INFO, "Success");

    return 0;
}
