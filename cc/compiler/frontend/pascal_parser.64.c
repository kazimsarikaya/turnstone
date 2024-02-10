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
    parser->next_custom_type_id = 1000;
    parser->custom_types = hashmap_string(128);

    if(parser->custom_types == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create custom types hashmap");
        return -1;
    }

    return pascal_lexer_get_next_token(parser->lexer, &parser->current_token);
}

int8_t pascal_parser_destroy(pascal_parser_t * parser) {
    if (parser == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "parser is null");

        return -1;
    }

    if (parser->current_token != NULL) {
        compiler_token_destroy(parser->current_token);
    }

    hashmap_destroy(parser->custom_types);

    return 0;
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

        size_t string_size = strlen(parser->current_token->text);

        if(string_size == 0) {
            memory_free((void*)parser->current_token->text);
            memory_free((void*)parser->current_token);
            memory_free((void*)*node);
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "string size is 0");
            return -1;
        } else if(string_size == 1) { // char
            (*node)->type = COMPILER_AST_NODE_TYPE_INTEGER_CONST;
            (*node)->token = parser->current_token;
            (*node)->token->value = parser->current_token->text[0];
            (*node)->token->size = 8;
        } else { // string
            (*node)->type = COMPILER_AST_NODE_TYPE_STRING_CONST;
            (*node)->token = parser->current_token;
        }

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
    int64_t custom_type_id = -1;

    boolean_t is_array = false;

    size_t array_size = 0;
    int64_t symbol_size = 0;

    if(parser->current_token->type == COMPILER_TOKEN_TYPE_INTEGER) {

        symbol_size = parser->current_token->size;

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

        need_token_type = COMPILER_TOKEN_TYPE_INTEGER_CONST;
    } else if(parser->current_token->type == COMPILER_TOKEN_TYPE_STRING) {
        need_token_type = COMPILER_TOKEN_TYPE_STRING_CONST;

        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_STRING, true) != 0) {
            compiler_destroy_symbol_list(symbol_list);
            return -1;
        }

        is_array = true;
        symbol_size = parser->current_token->size;
    } else if(parser->current_token->type == COMPILER_TOKEN_TYPE_ID) {
        const compiler_type_t * type = hashmap_get(parser->custom_types, parser->current_token->text);

        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_ID, true) != 0) {
            compiler_destroy_symbol_list(symbol_list);
            return -1;
        }

        if(type == NULL) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unknown type %s", parser->current_token->text);
            compiler_destroy_symbol_list(symbol_list);
            return -1;
        }

        custom_type_id = type->id;
    } else {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected type");
        compiler_destroy_symbol_list(symbol_list);
        return -1;
    }

    for(size_t i = 0; i < linkedlist_size(symbol_list); i++) {
        compiler_symbol_t * tmp_symbol = (compiler_symbol_t*)linkedlist_get_data_at_position(symbol_list, i);

        if(need_token_type == COMPILER_TOKEN_TYPE_INTEGER_CONST) {
            tmp_symbol->type = COMPILER_SYMBOL_TYPE_INTEGER;
        } else if(need_token_type == COMPILER_TOKEN_TYPE_STRING_CONST) {
            tmp_symbol->type = COMPILER_SYMBOL_TYPE_STRING;
        } else if(custom_type_id != -1) {
            tmp_symbol->type = COMPILER_SYMBOL_TYPE_CUSTOM;
            tmp_symbol->custom_type_id = custom_type_id;
        } else {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected type got %d", need_token_type);
            compiler_destroy_symbol_list(symbol_list);
            return -1;
        }

        tmp_symbol->size = symbol_size;
        tmp_symbol->is_array = is_array;
        tmp_symbol->array_size = array_size;
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

            if(need_token_type == COMPILER_TOKEN_TYPE_STRING_CONST) {
                if(parser->current_token->type != need_token_type) {
                    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected %d found %d", need_token_type, parser->current_token->type);
                    compiler_destroy_symbol_list(symbol_list);
                    return -1;
                }

                tmp_symbol->type = COMPILER_SYMBOL_TYPE_INTEGER;
                tmp_symbol->hidden_type = COMPILER_SYMBOL_TYPE_STRING;
                tmp_symbol->size = 8;

                size_t string_size = strlen(parser->current_token->text);

                if(string_size == 1) {
                    tmp_symbol->int_value = parser->current_token->text[0];
                    tmp_symbol->is_array = false;
                    tmp_symbol->array_size = 0;
                } else {
                    tmp_symbol->string_value = strdup(parser->current_token->text);
                    tmp_symbol->array_size = strlen(parser->current_token->text) + 1;
                }

                if(pascal_parser_eat(parser, need_token_type, true) != 0) {
                    compiler_destroy_symbol_list(symbol_list);
                    return -1;
                }
            } else if(is_array) {
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
int8_t pascal_parser_type(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_TYPE, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected type");
        return -1;
    }

    if(parser->current_token->type != COMPILER_TOKEN_TYPE_ID) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected id");
        return -1;
    }

    const char_t * type_name = strdup(parser->current_token->text);

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_ID, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected id");
        return -1;
    }

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_EQUAL, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected =");
        memory_free((void*)type_name);
        return -1;
    }

    boolean_t is_packed = false;

    if(parser->current_token->type == COMPILER_TOKEN_TYPE_PACKED) {
        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_PACKED, true) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected packed");
            return -1;
        }

        is_packed = true;
    }

    compiler_token_type_t type_token_type = parser->current_token->type;

    if(type_token_type != COMPILER_TOKEN_TYPE_RECORD &&
       type_token_type != COMPILER_TOKEN_TYPE_STRING &&
       type_token_type != COMPILER_TOKEN_TYPE_INTEGER &&
       type_token_type != COMPILER_TOKEN_TYPE_REAL &&
       type_token_type != COMPILER_TOKEN_TYPE_BOOLEAN &&
       type_token_type != COMPILER_TOKEN_TYPE_CHAR) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected type got %d", type_token_type);

        memory_free((void*)type_name);
        return -1;
    }

    if(pascal_parser_eat(parser, type_token_type, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected type");
        memory_free((void*)type_name);
        return -1;
    }

    linkedlist_t* variables_list = linkedlist_create_list();

    if(type_token_type == COMPILER_TOKEN_TYPE_RECORD) {

        while (parser->current_token->type == COMPILER_TOKEN_TYPE_ID) {
            compiler_ast_node_t * new_node = NULL;

            if(pascal_parser_variable(parser, &new_node, false, false) != 0) {
                linkedlist_destroy_with_type(variables_list, LINKEDLIST_DESTROY_WITH_DATA, compiler_ast_node_destroyer);
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected variable");
                return -1;
            }

            linkedlist_queue_push(variables_list, new_node);
        }


        if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_END, true) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected end");
            memory_free((void*)type_name);
            linkedlist_destroy(variables_list);
            return -1;
        }
    }

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_SEMI, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected ;");
        memory_free((void*)type_name);
        linkedlist_destroy_with_type(variables_list, LINKEDLIST_DESTROY_WITH_DATA, compiler_ast_node_destroyer);
        return -1;
    }

    compiler_type_t * type = memory_malloc(sizeof(compiler_type_t));

    if (type == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create type");
        memory_free((void*)type_name);
        linkedlist_destroy_with_type(variables_list, LINKEDLIST_DESTROY_WITH_DATA, compiler_ast_node_destroyer);
        return -1;
    }

    type->name = type_name;
    type->id = parser->next_custom_type_id++;
    type->is_packed = is_packed;
    type->type = type_token_type;
    type->fields = variables_list;

    hashmap_put(parser->custom_types, type->name, type);

    compiler_ast_node_t * type_node = memory_malloc(sizeof(compiler_ast_node_t));

    type_node->type = COMPILER_AST_NODE_TYPE_TYPE;
    type_node->type_data = type;

    compiler_ast_node_t * new_node = *node;

    if(new_node->children == NULL) {
        new_node->children = linkedlist_create_list();
    }

    linkedlist_queue_push(new_node->children, type_node);

    new_node->type = COMPILER_AST_NODE_TYPE_DECLS;

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
        } else if(parser->current_token->type == COMPILER_TOKEN_TYPE_TYPE) {
            if(pascal_parser_type(parser, &new_node) != 0) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected type definition");
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
    compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        return -1;
    }

    *node = new_node;


    if(parser->current_token->type != COMPILER_TOKEN_TYPE_ID) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected id");
        return -1;
    }

    boolean_t end_of_var = false;

    do {
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

        if(parser->current_token->type != COMPILER_TOKEN_TYPE_DOT) {
            end_of_var = true;
        } else {
            if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_DOT, true) != 0) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected .");
                compiler_ast_node_destroy(new_node);
                return -1;
            }

            if(parser->current_token->type != COMPILER_TOKEN_TYPE_ID) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected id");
                compiler_ast_node_destroy(new_node);
                return -1;
            }

            new_node->next = memory_malloc(sizeof(compiler_ast_node_t));

            if (new_node->next == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
                return -1;
            }

            new_node = new_node->next;
        }

    } while(!end_of_var);

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

int8_t pascal_parser_with_statement(pascal_parser_t * parser, compiler_ast_node_t ** node) {
    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_WITH, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected with");
        return -1;
    }

    if(parser->current_token->type != COMPILER_TOKEN_TYPE_ID) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected id");
        return -1;
    }

    compiler_token_t * token = parser->current_token;

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_ID, false) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected id");
        return -1;
    }

    if(pascal_parser_eat(parser, COMPILER_TOKEN_TYPE_DO, true) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected do");
        compiler_token_destroy(token);
        return -1;
    }

    compiler_ast_node_t * statement = NULL;

    if(pascal_parser_statement(parser, &statement) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected statement");
        compiler_token_destroy(token);
        return -1;
    }

    compiler_ast_node_t * new_node = memory_malloc(sizeof(compiler_ast_node_t));

    if (new_node == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create node");
        compiler_token_destroy(token);
        compiler_ast_node_destroy(statement);
        return -1;
    }

    new_node->type = COMPILER_AST_NODE_TYPE_WITH;
    new_node->left = statement;
    new_node->token = token;

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
    } else if(parser->current_token->type == COMPILER_TOKEN_TYPE_WITH) {
        return pascal_parser_with_statement(parser, node);
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

    parser->current_token = NULL;

    *node = new_node;

    return 0;
}

int8_t pascal_parser_parse(pascal_parser_t * parser, compiler_ast_t * ast) {
    return pascal_parser_program(parser, &ast->root);
}


