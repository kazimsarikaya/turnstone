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

int8_t pascal_parser_eat(pascal_parser_t * parser, compiler_token_type_t type, boolean_t need_free) {
    if (parser->current_token->type == type) {
        if (need_free && parser->current_token->not_free == false) {
            compiler_token_destroy(parser->current_token);
        }

        if(type == COMPILER_TOKEN_TYPE_EOF) {
            return 0;
        }

        return pascal_lexer_get_next_token(parser->lexer, &parser->current_token);
    }

    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unexpected token parser_eat. expected %d found %d", type, parser->current_token->type);
    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "buffer location: %lli", buffer_get_position(parser->lexer->buffer));

    return -1;
}

int8_t pascal_parser_factor(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    if(parser->current_token->type == COMPILER_TOKEN_TYPE_MINUS) {
        compiler_token_t * token = parser->current_token;

        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_MINUS, false) != 0) {
            return -1;
        }

        compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

        if (new_node == NULL) {
            return -1;
        }

        new_node->type = COMPILER_AST_NODE_TYPE_UNARY_OP;
        new_node->token = token;

        if(pascal_parser_factor(parser, &new_node->right) != 0) {
            return -1;
        }

        *node = new_node;

        return 0;
    } else if (parser->current_token->type == COMPILER_TOKEN_TYPE_PLUS) {
        compiler_token_t * token = parser->current_token;

        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_PLUS, false) != 0) {
            return -1;
        }

        compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

        if (new_node == NULL) {
            return -1;
        }

        new_node->type = COMPILER_AST_NODE_TYPE_UNARY_OP;
        new_node->token = token;

        if(pascal_parser_factor(parser, &new_node->right) != 0) {
            return -1;
        }

        *node = new_node;

        return 0;
    } else if (parser->current_token->type == COMPILER_TOKEN_TYPE_INTEGER_CONST) {
        *node = memory_malloc(sizeof(compiler_ast_node_t));

        if (*node == NULL) {
            return -1;
        }

        (*node)->type = COMPILER_AST_NODE_TYPE_INTEGER_CONST;
        (*node)->token = parser->current_token;

        return pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_INTEGER_CONST, false);
    } else if (parser->current_token->type == COMPILER_TOKEN_TYPE_LPAREN) {
        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_LPAREN, true) != 0) {
            return -1;
        }

        if(pascal_parser_expr(parser, node) != 0) {
            return -1;
        }

        return pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_RPAREN, true);
    } else if (parser->current_token->type == COMPILER_TOKEN_TYPE_ID) {
        return pascal_parser_var(parser, node);
    } else if(parser->current_token->type == COMPILER_TOKEN_TYPE_STRING_CONST) {
        *node = memory_malloc(sizeof(compiler_ast_node_t));

        if (*node == NULL) {
            return -1;
        }

        (*node)->type = COMPILER_AST_NODE_TYPE_STRING_CONST;
        (*node)->token = parser->current_token;

        return pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_STRING_CONST, false);
    } else if(parser->current_token->type == COMPILER_TOKEN_TYPE_NOT) {
        *node = memory_malloc(sizeof(compiler_ast_node_t));

        if (*node == NULL) {
            return -1;
        }

        (*node)->type = COMPILER_AST_NODE_TYPE_UNARY_OP;
        (*node)->token = parser->current_token;
        (*node)->token = parser->current_token;
    }


    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unexpected token parser_factor. found %d", parser->current_token->type);

    return -1;
}

int8_t pascal_parser_term(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    compiler_ast_node_t * left = NULL;
    compiler_ast_node_t * right = NULL;

    if(pascal_parser_factor(parser, &left) != 0) {
        return -1;
    }

    while (parser->current_token->type == COMPILER_TOKEN_TYPE_MULTIPLY ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_INTEGER_DIVIDE ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_REAL_DIVIDE ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_MOD ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_AND ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_SHL ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_SHR) {
        compiler_token_t * token = parser->current_token;

        if (token->type == COMPILER_TOKEN_TYPE_MULTIPLY) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_MULTIPLY, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == COMPILER_TOKEN_TYPE_INTEGER_DIVIDE) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_INTEGER_DIVIDE, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == COMPILER_TOKEN_TYPE_REAL_DIVIDE) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_REAL_DIVIDE, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == COMPILER_TOKEN_TYPE_MOD) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_MOD, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == COMPILER_TOKEN_TYPE_AND) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_AND, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == COMPILER_TOKEN_TYPE_SHL) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_SHL, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == COMPILER_TOKEN_TYPE_SHR) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_SHR, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        }

        if(pascal_parser_factor(parser, &right) != 0) {
            compiler_ast_node_destroy(left);

            return -1;
        }

        compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

        if (new_node == NULL) {
            compiler_ast_node_destroy(left);
            compiler_ast_node_destroy(right);

            return -1;
        }

        new_node->type = COMPILER_AST_NODE_TYPE_BINARY_OP;
        new_node->token = token;
        new_node->left = left;
        new_node->right = right;

        left = new_node;
    }

    *node = left;

    return 0;
}

int8_t pascal_parser_simple_expr(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    compiler_ast_node_t * left = NULL;
    compiler_ast_node_t * right = NULL;

    if(pascal_parser_term(parser, &left) != 0) {
        return -1;
    }

    while (parser->current_token->type == COMPILER_TOKEN_TYPE_PLUS ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_MINUS ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_OR ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_XOR) {
        compiler_token_t * token = parser->current_token;

        if (token->type == COMPILER_TOKEN_TYPE_PLUS) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_PLUS, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == COMPILER_TOKEN_TYPE_MINUS) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_MINUS, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == COMPILER_TOKEN_TYPE_OR) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_OR, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        }

        if(pascal_parser_term(parser, &right) != 0) {
            compiler_ast_node_destroy(left);

            return -1;
        }

        compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

        if (new_node == NULL) {
            compiler_ast_node_destroy(left);
            compiler_ast_node_destroy(right);

            return -1;
        }

        new_node->type = COMPILER_AST_NODE_TYPE_BINARY_OP;
        new_node->token = token;
        new_node->left = left;
        new_node->right = right;

        left = new_node;
    }

    *node = left;

    return 0;
}



int8_t pascal_parser_expr(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    compiler_ast_node_t * left = NULL;
    compiler_ast_node_t * right = NULL;

    if(pascal_parser_simple_expr(parser, &left) != 0) {
        return -1;
    }

    while (parser->current_token->type == COMPILER_TOKEN_TYPE_EQUAL ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_NOT_EQUAL ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_LESS_THAN ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_LESS_THAN_OR_EQUAL ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_GREATER_THAN ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_GREATER_THAN_OR_EQUAL ||
           parser->current_token->type == COMPILER_TOKEN_TYPE_IN) {
        compiler_token_t * token = parser->current_token;

        if (token->type == COMPILER_TOKEN_TYPE_EQUAL) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_EQUAL, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == COMPILER_TOKEN_TYPE_NOT_EQUAL) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_NOT_EQUAL, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == COMPILER_TOKEN_TYPE_LESS_THAN) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_LESS_THAN, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == COMPILER_TOKEN_TYPE_LESS_THAN_OR_EQUAL) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_LESS_THAN_OR_EQUAL, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == COMPILER_TOKEN_TYPE_GREATER_THAN) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_GREATER_THAN, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == COMPILER_TOKEN_TYPE_GREATER_THAN_OR_EQUAL) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_GREATER_THAN_OR_EQUAL, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        } else if (token->type == COMPILER_TOKEN_TYPE_IN) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_IN, false) != 0) {
                compiler_ast_node_destroy(left);

                return -1;
            }
        }

        if(pascal_parser_simple_expr(parser, &right) != 0) {
            compiler_ast_node_destroy(left);

            return -1;
        }

        compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

        if (new_node == NULL) {
            compiler_ast_node_destroy(left);
            compiler_ast_node_destroy(right);

            return -1;
        }

        new_node->type = COMPILER_AST_NODE_TYPE_RELATIONAL_OP;
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
int8_t pascal_parser_variable(pascal_parser_t * parser, compiler_ast_node_t ** node, boolean_t is_const, boolean_t is_local) {
    if(parser->current_token->type != COMPILER_TOKEN_TYPE_ID) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected id");
        return -1;
    }

    linkedlist_t* symbol_list = linkedlist_create_list();

    if (symbol_list == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create symbol list");
        return -1;
    }

    while (parser->current_token->type == COMPILER_TOKEN_TYPE_ID) {
        compiler_token_t * token = parser->current_token;

        compiler_symbol_t * symbol = memory_malloc(sizeof(compiler_symbol_t));

        if (symbol == NULL) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create symbol");
            compiler_destroy_symbol_list(symbol_list);
            return -1;
        }

        symbol->is_local = is_local;
        symbol->is_const = is_const;
        symbol->name = strdup(token->text);

        linkedlist_queue_push(symbol_list, symbol);

        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_ID, true) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected id");
            compiler_destroy_symbol_list(symbol_list);
            return -1;
        }

        if(parser->current_token->type == COMPILER_TOKEN_TYPE_COMMA) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_COMMA, true) != 0) {
                compiler_destroy_symbol_list(symbol_list);
                return -1;
            }

            continue;
        } else {
            break;
        }
    }

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_COLON, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected :");
        compiler_destroy_symbol_list(symbol_list);
        return -1;
    }

    compiler_token_type_t need_token_type = COMPILER_TOKEN_TYPE_UNKNOWN;

    boolean_t is_array = false;

    size_t array_size = 0;

    if(parser->current_token->type == COMPILER_TOKEN_TYPE_INTEGER) {

        int64_t symbol_size = parser->current_token->size;

        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_INTEGER, true) != 0) {
            compiler_destroy_symbol_list(symbol_list);
            return -1;
        }

        if(parser->current_token->type == COMPILER_TOKEN_TYPE_LBRACKET) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_LBRACKET, true) != 0) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected [");
                compiler_destroy_symbol_list(symbol_list);
                return -1;
            }

            if(parser->current_token->type != COMPILER_TOKEN_TYPE_INTEGER_CONST) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected integer");
                compiler_destroy_symbol_list(symbol_list);
                return -1;
            }

            is_array = true;
            array_size = parser->current_token->value;

            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_INTEGER_CONST, true) != 0) {
                compiler_destroy_symbol_list(symbol_list);
                return -1;
            }

            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_RBRACKET, true) != 0) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected ]");
                compiler_destroy_symbol_list(symbol_list);
                return -1;
            }
        }

        for(size_t i = 0; i < linkedlist_size(symbol_list); i++) {
            compiler_symbol_t * tmp_symbol = (compiler_symbol_t*)linkedlist_get_data_at_position(symbol_list, i);

            tmp_symbol->type = COMPILER_SYMBOL_TYPE_INTEGER;
            tmp_symbol->size = symbol_size;
            tmp_symbol->is_array = is_array;
            tmp_symbol->array_size = array_size;
        }

        need_token_type = COMPILER_TOKEN_TYPE_INTEGER_CONST;
    }

    if(parser->current_token->type == COMPILER_TOKEN_TYPE_EQUAL) {
        if(need_token_type == COMPILER_TOKEN_TYPE_UNKNOWN) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected type");
            compiler_destroy_symbol_list(symbol_list);
            return -1;
        }

        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_EQUAL, true) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected =");
            compiler_destroy_symbol_list(symbol_list);
            return -1;
        }

        size_t value_count = linkedlist_size(symbol_list);

        for(size_t i = 0; i < value_count; i++) {
            compiler_symbol_t * tmp_symbol = (compiler_symbol_t*)linkedlist_get_data_at_position(symbol_list, i);

            tmp_symbol->initilized = true;

            if(is_array) {
                if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_LPAREN, true) != 0) {
                    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected (");
                    compiler_destroy_symbol_list(symbol_list);
                    return -1;
                }

                tmp_symbol->int_array_value = memory_malloc(sizeof(int64_t) * array_size);

                if (tmp_symbol->int_array_value == NULL) {
                    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create array");
                    compiler_destroy_symbol_list(symbol_list);
                    return -1;
                }

                for(size_t j = 0; j < array_size; j++) {
                    if(parser->current_token->type != need_token_type) {
                        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected %d found %d", need_token_type, parser->current_token->type);
                        compiler_destroy_symbol_list(symbol_list);
                        return -1;
                    }

                    if(need_token_type == COMPILER_TOKEN_TYPE_INTEGER_CONST) {
                        tmp_symbol->int_array_value[j] = parser->current_token->value;
                    }

                    if(pascal_parser_eat(parser, need_token_type, true) != 0) {
                        compiler_destroy_symbol_list(symbol_list);
                        return -1;
                    }

                    if(j != array_size - 1) {
                        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_COMMA, true) != 0) {
                            compiler_destroy_symbol_list(symbol_list);
                            return -1;
                        }
                    }
                }

                if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_RPAREN, true) != 0) {
                    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected )");
                    compiler_destroy_symbol_list(symbol_list);
                    return -1;
                }

            } else {
                if(parser->current_token->type != need_token_type) {
                    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected %d found %d", need_token_type, parser->current_token->type);
                    compiler_destroy_symbol_list(symbol_list);
                    return -1;
                }

                if(need_token_type == COMPILER_TOKEN_TYPE_INTEGER_CONST) {
                    tmp_symbol->int_value = parser->current_token->value;
                }

                if(pascal_parser_eat(parser, need_token_type, true) != 0) {
                    compiler_destroy_symbol_list(symbol_list);
                    return -1;
                }
            }





            if(i != value_count - 1) {
                if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_COMMA, true) != 0) {
                    compiler_destroy_symbol_list(symbol_list);
                    return -1;
                }
            }
        }
    }

    if(!is_local && pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_SEMI, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected ;");
        compiler_destroy_symbol_list(symbol_list);
        return -1;
    }

    compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        compiler_destroy_symbol_list(symbol_list);
        return -1;
    }

    new_node->type = COMPILER_AST_NODE_TYPE_VAR;

    new_node->children = symbol_list;

    *node = new_node;

    return 0;
}
#pragma GCC diagnostic pop

int8_t pascal_parser_variables(pascal_parser_t* parser, compiler_ast_node_t** node, boolean_t is_const, boolean_t is_local) {
    if(!is_const) {
        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_VAR, true) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected var");
            return -1;
        }
    } else {
        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_CONST, true) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected var");
            return -1;
        }
    }

    linkedlist_t* variables_list = linkedlist_create_list();

    while (parser->current_token->type == COMPILER_TOKEN_TYPE_ID) {
        compiler_ast_node_t * new_node = NULL;

        if(pascal_parser_variable(parser, &new_node, is_const, is_local) != 0) {
            linkedlist_destroy_with_type(variables_list, LINKEDLIST_DESTROY_WITH_DATA, compiler_ast_node_destroyer);
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected variable");
            return -1;
        }

        linkedlist_queue_push(variables_list, new_node);
    }

    compiler_ast_node_t * new_node = *node;

    if(linkedlist_size(variables_list) > 0) {
        if(new_node->children != NULL) {
            int8_t merge_res = linkedlist_merge(new_node->children, variables_list);

            linkedlist_destroy(variables_list);

            if(merge_res != 0) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot merge lists");
                linkedlist_destroy_with_type(new_node->children, LINKEDLIST_DESTROY_WITH_DATA, compiler_ast_node_destroyer);
                return -1;
            }
        } else {
            new_node->children = variables_list;
        }

        new_node->type = COMPILER_AST_NODE_TYPE_DECLS;
    } else {
        linkedlist_destroy(variables_list);
    }

    return 0;
}

int8_t pascal_parser_decls(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    compiler_ast_node_t* new_node = memory_malloc(sizeof(compiler_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    new_node->type = COMPILER_AST_NODE_TYPE_NO_OP;

    while(true) {
        if(parser->current_token->type == COMPILER_TOKEN_TYPE_VAR) {
            if(pascal_parser_variables(parser, &new_node, false, false) != 0) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected variables");
                compiler_ast_node_destroy(new_node);
                return -1;
            }
        } else if(parser->current_token->type == COMPILER_TOKEN_TYPE_CONST) {
            if(pascal_parser_variables(parser, &new_node, true, false) != 0) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected constants");
                compiler_ast_node_destroy(new_node);
                return -1;
            }
        } else {
            break;
        }
    }

    *node = new_node;

    return 0;
}

int8_t pascal_parser_var(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    if(parser->current_token->type != COMPILER_TOKEN_TYPE_ID) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected id");
        return -1;
    }

    compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    new_node->type = COMPILER_AST_NODE_TYPE_VAR;
    new_node->token = parser->current_token;

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_ID, false) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected id");
        compiler_ast_node_destroy(new_node);
        return -1;
    }

    if(parser->current_token->type == COMPILER_TOKEN_TYPE_LBRACKET) {
        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_LBRACKET, true) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected [");
            compiler_ast_node_destroy(new_node);
            return -1;
        }

        if(pascal_parser_expr(parser, &new_node->array_subscript) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected expression");
            compiler_ast_node_destroy(new_node);
            return -1;
        }

        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_RBRACKET, true) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected ]");
            compiler_ast_node_destroy(new_node);
            return -1;
        }

        new_node->is_array_subscript = true;
    }

    *node = new_node;

    return 0;
}

int8_t pascal_parser_function_call(pascal_parser_t* parser, compiler_ast_node_t** node) {
    if(parser->current_token->type != COMPILER_TOKEN_TYPE_LPAREN) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected (");
        return -1;
    }

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_LPAREN, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected (");
        return -1;
    }

    linkedlist_t* children_list = linkedlist_create_list();

    if (children_list == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create children list");
        return -1;
    }

    while (parser->current_token->type != COMPILER_TOKEN_TYPE_RPAREN) {
        compiler_ast_node_t * new_node = NULL;

        if(pascal_parser_expr(parser, &new_node) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected expression");
            linkedlist_destroy_with_type(children_list, LINKEDLIST_DESTROY_WITH_DATA, compiler_ast_node_destroyer);
            return -1;
        }

        linkedlist_queue_push(children_list, new_node);

        if(parser->current_token->type != COMPILER_TOKEN_TYPE_RPAREN) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_COMMA, true) != 0) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected ,");
                linkedlist_destroy_with_type(children_list, LINKEDLIST_DESTROY_WITH_DATA, compiler_ast_node_destroyer);
                return -1;
            }
        }
    }

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_RPAREN, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected )");
        return -1;
    }

    compiler_ast_node_t* new_node = *node;

    new_node->type = COMPILER_AST_NODE_TYPE_FUNCTION_CALL;
    new_node->children = children_list;
    new_node->token = (*node)->token;

    return 0;
}

int8_t pascal_parser_assignment_statement(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    compiler_ast_node_t * left = NULL;

    if(pascal_parser_var(parser, &left) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected lvar");
        return -1;
    }

    if(parser->current_token->type == COMPILER_TOKEN_TYPE_LPAREN) {
        *node = left;
        return pascal_parser_function_call(parser, node);
    }

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_ASSIGN, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected :=");
        compiler_token_destroy(parser->current_token);
        compiler_ast_node_destroy(left);
        return -1;
    }

    compiler_ast_node_t * right = NULL;

    if(pascal_parser_expr(parser, &right) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected expression");
        compiler_ast_node_destroy(left);
        return -1;
    }

    compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        compiler_ast_node_destroy(left);
        compiler_ast_node_destroy(right);
        return -1;
    }

    new_node->type = COMPILER_AST_NODE_TYPE_ASSIGN;
    new_node->left = left;
    new_node->right = right;

    *node = new_node;

    return 0;
}

int8_t pascal_parser_statement(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    if(parser->current_token->type == COMPILER_TOKEN_TYPE_BEGIN) {
        return pascal_parser_compound_statement(parser, node);
    } else if (parser->current_token->type == COMPILER_TOKEN_TYPE_ID) {
        return pascal_parser_assignment_statement(parser, node);
    } else if(parser->current_token->type == COMPILER_TOKEN_TYPE_VAR) {
        *node = memory_malloc(sizeof(compiler_ast_node_t));

        if (*node == NULL) {
            return -1;
        }

        (*node)->type = COMPILER_AST_NODE_TYPE_DECLS;

        return pascal_parser_variables(parser, node, false, true);
    } else if(parser->current_token->type == COMPILER_TOKEN_TYPE_CONST) {
        *node = memory_malloc(sizeof(compiler_ast_node_t));

        if (*node == NULL) {
            return -1;
        }

        (*node)->type = COMPILER_AST_NODE_TYPE_DECLS;

        return pascal_parser_variables(parser, node, true, true);
    } else if(parser->current_token->type == COMPILER_TOKEN_TYPE_IF) {
        return pascal_parser_if_statement(parser, node);
    } else if(parser->current_token->type == COMPILER_TOKEN_TYPE_WHILE) {
        return pascal_parser_while_statement(parser, node);
    } else if(parser->current_token->type == COMPILER_TOKEN_TYPE_REPEAT) {
        return pascal_parser_repeat_statement(parser, node);
    } else if(parser->current_token->type == COMPILER_TOKEN_TYPE_FOR) {
        return pascal_parser_for_statement(parser, node);
    }

    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unexpected token parser_statement. found %d", parser->current_token->type);

    return -1;
}

int8_t pascal_parser_while_statement(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_WHILE, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected while");
        return -1;
    }

    compiler_ast_node_t * condition = NULL;

    if(pascal_parser_expr(parser, &condition) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected condition expression");
        return -1;
    }

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_DO, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected do");
        return -1;
    }

    compiler_ast_node_t * while_body_statement = NULL;

    if(pascal_parser_statement(parser, &while_body_statement) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected statement");
        return -1;
    }

    compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    new_node->type = COMPILER_AST_NODE_TYPE_WHILE;
    new_node->condition = condition;
    new_node->left = while_body_statement;

    *node = new_node;

    return 0;
}

int8_t pascal_parser_repeat_statement(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_REPEAT, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected repeat");
        return -1;
    }

    compiler_ast_node_t * repeat_body_statement = NULL;

    if(pascal_parser_statement(parser, &repeat_body_statement) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected statement");
        return -1;
    }

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_UNTIL, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected until");
        return -1;
    }

    compiler_ast_node_t * condition = NULL;

    if(pascal_parser_expr(parser, &condition) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected condition expression");
        return -1;
    }

    compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    new_node->type = COMPILER_AST_NODE_TYPE_REPEAT;
    new_node->condition = condition;
    new_node->left = repeat_body_statement;

    *node = new_node;

    return 0;
}

int8_t pascal_parser_for_statement(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_FOR, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected for");
        return -1;
    }

    compiler_ast_node_t* for_assignment = NULL;

    if(pascal_parser_assignment_statement(parser, &for_assignment) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected var");
        return -1;
    }

    compiler_ast_node_t* for_init_var = memory_malloc(sizeof(compiler_ast_node_t));

    if (for_init_var == NULL) {
        compiler_ast_node_destroy(for_assignment);
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    for_init_var->type = COMPILER_AST_NODE_TYPE_VAR;
    for_init_var->token = memory_malloc(sizeof(compiler_token_t));

    if (for_init_var->token == NULL) {
        memory_free(for_init_var);
        compiler_ast_node_destroy(for_assignment);
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");
        return -1;
    }

    for_init_var->token->type = COMPILER_TOKEN_TYPE_ID;
    for_init_var->token->text = strdup(for_assignment->left->token->text);

    compiler_ast_node_t* for_step_var_left = memory_malloc(sizeof(compiler_ast_node_t));

    if (for_step_var_left == NULL) {
        compiler_ast_node_destroy(for_assignment);
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    for_step_var_left->type = COMPILER_AST_NODE_TYPE_VAR;
    for_step_var_left->token = memory_malloc(sizeof(compiler_token_t));

    if (for_step_var_left->token == NULL) {
        memory_free(for_step_var_left);
        compiler_ast_node_destroy(for_init_var);
        compiler_ast_node_destroy(for_assignment);
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");
        return -1;
    }

    for_step_var_left->token->type = COMPILER_TOKEN_TYPE_ID;
    for_step_var_left->token->text = strdup(for_assignment->left->token->text);

    compiler_ast_node_t* for_step_var_right = memory_malloc(sizeof(compiler_ast_node_t));

    if (for_step_var_right == NULL) {
        compiler_ast_node_destroy(for_assignment);
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    for_step_var_right->type = COMPILER_AST_NODE_TYPE_VAR;
    for_step_var_right->token = memory_malloc(sizeof(compiler_token_t));

    if (for_step_var_right->token == NULL) {
        memory_free(for_step_var_right);
        compiler_ast_node_destroy(for_step_var_left);
        compiler_ast_node_destroy(for_init_var);
        compiler_ast_node_destroy(for_assignment);
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");
        return -1;
    }

    for_step_var_right->token->type = COMPILER_TOKEN_TYPE_ID;
    for_step_var_right->token->text = strdup(for_assignment->left->token->text);

    boolean_t is_to = false;

    if(parser->current_token->type == COMPILER_TOKEN_TYPE_TO) {
        is_to = true;

        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_TO, true) != 0) {
            compiler_ast_node_destroy(for_assignment);
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected to");
            return -1;
        }
    } else if(parser->current_token->type == COMPILER_TOKEN_TYPE_DOWNTO) {
        is_to = false;

        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_DOWNTO, true) != 0) {
            compiler_ast_node_destroy(for_assignment);
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected downto");
            return -1;
        }
    } else {
        compiler_ast_node_destroy(for_assignment);
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected to or downto");
        return -1;
    }

    compiler_ast_node_t* for_final_expr = NULL;

    if(pascal_parser_expr(parser, &for_final_expr) != 0) {
        compiler_ast_node_destroy(for_assignment);
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected final expression");
        return -1;
    }

    compiler_ast_node_t* for_condition = memory_malloc(sizeof(compiler_ast_node_t));

    if (for_condition == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    for_condition->type = COMPILER_AST_NODE_TYPE_RELATIONAL_OP;
    for_condition->token = memory_malloc(sizeof(compiler_token_t));

    if (for_condition->token == NULL) {
        memory_free(for_condition);
        compiler_ast_node_destroy(for_final_expr);
        compiler_ast_node_destroy(for_assignment);
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");
        return -1;
    }

    for_condition->token->type = is_to ? COMPILER_TOKEN_TYPE_LESS_THAN_OR_EQUAL : COMPILER_TOKEN_TYPE_GREATER_THAN_OR_EQUAL;

    for_condition->left = for_init_var;
    for_condition->right = for_final_expr;


    compiler_ast_node_t* for_step_expr = NULL;

    if(parser->current_token->type == COMPILER_TOKEN_TYPE_STEP) {
        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_STEP, true) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected step");
            return -1;
        }

        if(pascal_parser_expr(parser, &for_step_expr) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected step expression");
            return -1;
        }
    } else {
        for_step_expr = memory_malloc(sizeof(compiler_ast_node_t));

        if (for_step_expr == NULL) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
            return -1;
        }

        for_step_expr->type = COMPILER_AST_NODE_TYPE_INTEGER_CONST;
        for_step_expr->token = memory_malloc(sizeof(compiler_token_t));

        if (for_step_expr->token == NULL) {
            memory_free(for_step_expr);
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");
            return -1;
        }

        for_step_expr->token->type = COMPILER_TOKEN_TYPE_INTEGER_CONST;
        for_step_expr->token->value = 1;
    }


    compiler_ast_node_t* for_step_binary_op = memory_malloc(sizeof(compiler_ast_node_t));

    if (for_step_binary_op == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    for_step_binary_op->type = COMPILER_AST_NODE_TYPE_BINARY_OP;
    for_step_binary_op->token = memory_malloc(sizeof(compiler_token_t));

    if (for_step_binary_op->token == NULL) {
        memory_free(for_step_binary_op);
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");
        return -1;
    }

    for_step_binary_op->token->type = is_to ? COMPILER_TOKEN_TYPE_PLUS : COMPILER_TOKEN_TYPE_MINUS;
    for_step_binary_op->left = for_step_var_right;
    for_step_binary_op->right = for_step_expr;

    compiler_ast_node_t * for_step_assign = memory_malloc(sizeof(compiler_ast_node_t));

    if (for_step_assign == NULL) {
        compiler_ast_node_destroy(for_step_expr);
        compiler_ast_node_destroy(for_condition);
        compiler_ast_node_destroy(for_final_expr);
        compiler_ast_node_destroy(for_assignment);
        compiler_ast_node_destroy(for_step_var_right);
        compiler_ast_node_destroy(for_step_var_left);
        compiler_ast_node_destroy(for_init_var);
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    for_step_assign->type = COMPILER_AST_NODE_TYPE_ASSIGN;
    for_step_assign->left = for_step_var_left;
    for_step_assign->right = for_step_binary_op;

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_DO, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected do");
        return -1;
    }

    compiler_ast_node_t * for_body_statement = NULL;

    if(pascal_parser_statement(parser, &for_body_statement) != 0) {
        compiler_ast_node_destroy(for_assignment);
        compiler_ast_node_destroy(for_condition);
        compiler_ast_node_destroy(for_step_assign);
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected statement");
        return -1;
    }

    linkedlist_t* for_body_list = linkedlist_create_list();

    if (for_body_list == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create list");
        return -1;
    }

    linkedlist_queue_push(for_body_list, for_body_statement);
    linkedlist_queue_push(for_body_list, for_step_assign);

    compiler_ast_node_t* for_body_compound = memory_malloc(sizeof(compiler_ast_node_t));

    if (for_body_compound == NULL) {
        linkedlist_destroy(for_body_list);
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    for_body_compound->type = COMPILER_AST_NODE_TYPE_COMPOUND;
    for_body_compound->children = for_body_list;

    compiler_ast_node_t * while_wrapper = memory_malloc(sizeof(compiler_ast_node_t));

    if (while_wrapper == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    while_wrapper->type = COMPILER_AST_NODE_TYPE_WHILE;
    while_wrapper->condition = for_condition;
    while_wrapper->left = for_body_compound;


    linkedlist_t* for_wrapper_list = linkedlist_create_list();

    if (for_wrapper_list == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create list");
        return -1;
    }

    linkedlist_queue_push(for_wrapper_list, for_assignment);
    linkedlist_queue_push(for_wrapper_list, while_wrapper);

    compiler_ast_node_t* for_wrapper_compound = memory_malloc(sizeof(compiler_ast_node_t));

    if (for_wrapper_compound == NULL) {
        linkedlist_destroy(for_wrapper_list);
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    for_wrapper_compound->type = COMPILER_AST_NODE_TYPE_COMPOUND;
    for_wrapper_compound->children = for_wrapper_list;

    *node = for_wrapper_compound;

    return 0;
}

int8_t pascal_parser_if_statement(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_IF, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected if");
        return -1;
    }

    compiler_ast_node_t * condition = NULL;

    if(pascal_parser_expr(parser, &condition) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected condition expression");
        return -1;
    }

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_THEN, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected then");
        return -1;
    }

    compiler_ast_node_t * if_statement = NULL;

    if(pascal_parser_statement(parser, &if_statement) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected statement");
        return -1;
    }

    compiler_ast_node_t * else_statement = NULL;

    if(parser->current_token->type == COMPILER_TOKEN_TYPE_ELSE) {
        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_ELSE, true) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected else");
            return -1;
        }

        if(pascal_parser_statement(parser, &else_statement) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected statement");
            return -1;
        }
    }

    compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    new_node->type = COMPILER_AST_NODE_TYPE_IF;
    new_node->condition = condition;
    new_node->left = if_statement;
    new_node->right = else_statement;

    *node = new_node;

    return 0;
}

int8_t pascal_parser_compound_statement(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_BEGIN, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected begin");
        return -1;
    }

    linkedlist_t* children_list = linkedlist_create_list();

    if (children_list == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create children list");
        return -1;
    }

    while (parser->current_token->type != COMPILER_TOKEN_TYPE_END) {
        compiler_ast_node_t * new_node = NULL;

        if(pascal_parser_statement(parser, &new_node) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected statement");
            linkedlist_destroy_with_type(children_list, LINKEDLIST_DESTROY_WITH_DATA, compiler_ast_node_destroyer);
            return -1;
        }

        linkedlist_queue_push(children_list, new_node);

        if(parser->current_token->type != COMPILER_TOKEN_TYPE_END) {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_SEMI, true) != 0) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected ;");
                linkedlist_destroy_with_type(children_list, LINKEDLIST_DESTROY_WITH_DATA, compiler_ast_node_destroyer);
                return -1;
            }
        }
    }

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_END, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected end");
        return -1;
    }

    compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    new_node->type = COMPILER_AST_NODE_TYPE_NO_OP;

    if(linkedlist_size(children_list) > 0) {
        new_node->children = children_list;
        new_node->type = COMPILER_AST_NODE_TYPE_COMPOUND;
    } else {
        linkedlist_destroy(children_list);
    }

    *node = new_node;

    return 0;
}

int8_t pascal_parser_block(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

    if (new_node == NULL) {
        return -1;
    }

    new_node->type = COMPILER_AST_NODE_TYPE_BLOCK;

    if(pascal_parser_decls(parser, &new_node->left) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected decls");
        compiler_ast_node_destroy(new_node);
        return -1;
    }

    if(pascal_parser_compound_statement(parser, &new_node->right) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected compound statement");
        compiler_ast_node_destroy(new_node);
        return -1;
    }

    *node = new_node;

    return 0;
}

int8_t pascal_parser_program(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_PROGRAM, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected program");
        compiler_token_destroy(parser->current_token);
        return -1;
    }

    compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    new_node->type = COMPILER_AST_NODE_TYPE_PROGRAM;
    new_node->token = parser->current_token;

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_ID, false) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected program id");
        compiler_ast_node_destroy(new_node);
        return -1;
    }

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_SEMI, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected ;");
        compiler_ast_node_destroy(new_node);
        return -1;
    }

    if(pascal_parser_block(parser, &new_node->left) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected block");
        compiler_ast_node_destroy(new_node);
        return -1;
    }

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_DOT, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected .");
        compiler_ast_node_destroy(new_node);
        return -1;
    }

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_EOF, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected eof");
        compiler_ast_node_destroy(new_node);
        return -1;
    }

    *node = new_node;

    return 0;
}

int8_t pascal_parser_parse(pascal_parser_t * parser, compiler_ast_t * ast) {
    return pascal_parser_program(parser, &ast->root);
}


