/**
 * @file 1005.c
 * @brief
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define ___COMPILER_PASCAL_IMPLEMENTATION 0

#include <compiler/pascal.h>
#include <logging.h>
#include <utils.h>
#include <strings.h>

MODULE("turnstone.compiler.pascal");

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
    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "buffer location: %lli", buffer_get_position(parser->lexer->buffer));

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
    } else if(parser->current_token->type == PASCAL_TOKEN_TYPE_STRING_CONST) {
        *node = memory_malloc(sizeof(pascal_ast_node_t));

        if (*node == NULL) {
            return -1;
        }

        (*node)->type = PASCAL_AST_NODE_TYPE_STRING_CONST;
        (*node)->token = parser->current_token;

        return pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_STRING_CONST, false);
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
           parser->current_token->type == PASCAL_TOKEN_TYPE_INTEGER_DIVIDE ||
           parser->current_token->type == PASCAL_TOKEN_TYPE_REAL_DIVIDE ||
           parser->current_token->type == PASCAL_TOKEN_TYPE_MOD ||
           parser->current_token->type == PASCAL_TOKEN_TYPE_AND) {
        pascal_token_t * token = parser->current_token;

        if (token->type == PASCAL_TOKEN_TYPE_MULTIPLY) {
            if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_MULTIPLY, false) != 0) {
                pascal_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == PASCAL_TOKEN_TYPE_INTEGER_DIVIDE) {
            if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_INTEGER_DIVIDE, false) != 0) {
                pascal_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == PASCAL_TOKEN_TYPE_REAL_DIVIDE) {
            if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_REAL_DIVIDE, false) != 0) {
                pascal_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == PASCAL_TOKEN_TYPE_MOD) {
            if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_MOD, false) != 0) {
                pascal_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == PASCAL_TOKEN_TYPE_AND) {
            if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_AND, false) != 0) {
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
        } else if (token->type == PASCAL_TOKEN_TYPE_OR) {
            if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_OR, false) != 0) {
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t pascal_parser_variable(pascal_parser_t * parser, pascal_ast_node_t ** node, boolean_t is_const, boolean_t is_local) {
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

        symbol->is_local = is_local;
        symbol->is_const = is_const;
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

    if(!is_local && pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_SEMI, true) != 0) {
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
#pragma GCC diagnostic pop

int8_t pascal_parser_variables(pascal_parser_t* parser, pascal_ast_node_t** node, boolean_t is_const, boolean_t is_local) {
    if(!is_const) {
        if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_VAR, true) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected var");
            return -1;
        }
    } else {
        if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_CONST, true) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected var");
            return -1;
        }
    }

    linkedlist_t* variables_list = linkedlist_create_list();

    while (parser->current_token->type == PASCAL_TOKEN_TYPE_ID) {
        pascal_ast_node_t * new_node = NULL;

        if(pascal_parser_variable(parser, &new_node, is_const, is_local) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected variable");
            return -1;
        }

        linkedlist_queue_push(variables_list, new_node);
    }

    pascal_ast_node_t * new_node = *node;

    if(linkedlist_size(variables_list) > 0) {
        if(new_node->children != NULL) {
            int8_t merge_res = linkedlist_merge(new_node->children, variables_list);

            linkedlist_destroy(variables_list);

            if(merge_res != 0) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot merge lists");
                linkedlist_destroy_with_type(new_node->children, LINKEDLIST_DESTROY_WITH_DATA, pascal_ast_node_destroyer);
                return -1;
            }
        } else {
            new_node->children = variables_list;
        }

        new_node->type = PASCAL_AST_NODE_TYPE_DECLS;
    } else {
        linkedlist_destroy(variables_list);
    }

    return 0;
}

int8_t pascal_parser_decls(pascal_parser_t * parser, pascal_ast_node_t ** node) {
    pascal_ast_node_t* new_node = memory_malloc(sizeof(pascal_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    new_node->type = PASCAL_AST_NODE_TYPE_NO_OP;

    while(true) {
        if(parser->current_token->type == PASCAL_TOKEN_TYPE_VAR) {
            if(pascal_parser_variables(parser, &new_node, false, false) != 0) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected variables");
                pascal_ast_node_destroy(new_node);
                return -1;
            }
        } else if(parser->current_token->type == PASCAL_TOKEN_TYPE_CONST) {
            if(pascal_parser_variables(parser, &new_node, true, false) != 0) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected constants");
                pascal_ast_node_destroy(new_node);
                return -1;
            }
        } else {
            break;
        }
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

int8_t pascal_parser_function_call(pascal_parser_t* parser, pascal_ast_node_t** node) {
    if(parser->current_token->type != PASCAL_TOKEN_TYPE_LPAREN) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected (");
        return -1;
    }

    if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_LPAREN, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected (");
        return -1;
    }

    linkedlist_t* children_list = linkedlist_create_list();

    if (children_list == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create children list");
        return -1;
    }

    printf("parser->current_token->type %d\n", parser->current_token->type);

    while (parser->current_token->type != PASCAL_TOKEN_TYPE_RPAREN) {
        pascal_ast_node_t * new_node = NULL;

        if(pascal_parser_expr(parser, &new_node) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected expression");
            linkedlist_destroy_with_type(children_list, LINKEDLIST_DESTROY_WITH_DATA, pascal_ast_node_destroyer);
            return -1;
        }

        linkedlist_queue_push(children_list, new_node);

        if(parser->current_token->type != PASCAL_TOKEN_TYPE_RPAREN) {
            if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_COMMA, true) != 0) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected ,");
                linkedlist_destroy_with_type(children_list, LINKEDLIST_DESTROY_WITH_DATA, pascal_ast_node_destroyer);
                return -1;
            }
        }
    }

    if(pascal_parser_eat(parser, PASCAL_TOKEN_TYPE_RPAREN, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected )");
        return -1;
    }

    pascal_ast_node_t* new_node = *node;

    new_node->type = PASCAL_AST_NODE_TYPE_FUNCTION_CALL;
    new_node->children = children_list;
    new_node->token = (*node)->token;

    return 0;
}

int8_t pascal_parser_assignment_statement(pascal_parser_t * parser, pascal_ast_node_t ** node) {
    pascal_ast_node_t * left = NULL;

    if(pascal_parser_var(parser, &left) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected lvar");
        return -1;
    }

    if(parser->current_token->type == PASCAL_TOKEN_TYPE_LPAREN) {
        *node = left;
        return pascal_parser_function_call(parser, node);
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
    } else if(parser->current_token->type == PASCAL_TOKEN_TYPE_VAR) {
        *node = memory_malloc(sizeof(pascal_ast_node_t));

        if (*node == NULL) {
            return -1;
        }

        (*node)->type = PASCAL_AST_NODE_TYPE_DECLS;

        return pascal_parser_variables(parser, node, false, true);
    } else if(parser->current_token->type == PASCAL_TOKEN_TYPE_CONST) {
        *node = memory_malloc(sizeof(pascal_ast_node_t));

        if (*node == NULL) {
            return -1;
        }

        (*node)->type = PASCAL_AST_NODE_TYPE_DECLS;

        return pascal_parser_variables(parser, node, true, true);
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
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected program");
        pascal_token_destroy(parser->current_token);
        return -1;
    }

    pascal_ast_node_t * new_node = memory_malloc(sizeof(pascal_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
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


