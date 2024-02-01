/**
 * @file pascal_ast.64.c
 * @brief Pascal AST implementation
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

int8_t pascal_ast_init(pascal_ast_t * ast) {
    ast->root = NULL;
    return 0;
}


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

    if(node->condition) {
        pascal_ast_node_destroy(node->condition);
    }

    if(node->type == PASCAL_AST_NODE_TYPE_VAR) {
        pascal_destroy_symbol_list(node->children);
    }

    if(node->type == PASCAL_AST_NODE_TYPE_STRING_CONST) {
        if(node->symbol) {
            memory_free((void*)node->symbol->name);
            memory_free(node->symbol);
        }
    }

    if(node->type == PASCAL_AST_NODE_TYPE_FOR) {
        pascal_ast_node_destroy(node->for_condition->step_expr);
        pascal_ast_node_destroy(node->for_condition->final_expr);
        pascal_ast_node_destroy(node->for_condition->init_expr);
        pascal_token_destroy(node->for_condition->var_token);
        memory_free(node->for_condition);
    }

    if(node->type == PASCAL_AST_NODE_TYPE_DECLS ||
       node->type == PASCAL_AST_NODE_TYPE_COMPOUND ||
       node->type == PASCAL_AST_NODE_TYPE_FUNCTION_CALL) {
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
