/**
 * @file compiler_ast.64.c
 * @brief Pascal AST implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <utils.h>
#include <strings.h>

MODULE("turnstone.compiler");

int8_t compiler_ast_init(compiler_ast_t * ast) {
    ast->root = NULL;
    return 0;
}


int8_t compiler_ast_node_destroyer(memory_heap_t* heap, void* node) {
    UNUSED(heap);

    if (node == NULL) {
        return -1;
    }

    compiler_ast_node_t * l_node = (compiler_ast_node_t*)node;

    return compiler_ast_node_destroy(l_node);
}

int8_t compiler_ast_node_destroy(compiler_ast_node_t * node) {
    if (node == NULL) {
        return -1;
    }

    if(node->left) {
        compiler_ast_node_destroy(node->left);
    }

    if(node->right) {
        compiler_ast_node_destroy(node->right);
    }

    if(node->condition) {
        compiler_ast_node_destroy(node->condition);
    }

    if(node->type == COMPILER_AST_NODE_TYPE_VAR) {
        compiler_destroy_symbol_list(node->children);
    }

    if(node->type == COMPILER_AST_NODE_TYPE_STRING_CONST) {
        if(node->symbol) {
            memory_free((void*)node->symbol->name);
            memory_free(node->symbol);
        }
    }

    if(node->type == COMPILER_AST_NODE_TYPE_TYPE) {
        if(node->type_data) {
            memory_free((void*)node->type_data->name);

            if(node->type_data->fields) {
                list_destroy_with_type(node->type_data->fields, LIST_DESTROY_WITH_DATA, compiler_ast_node_destroyer);
            }

            if(node->type_data->field_map) {
                iterator_t * it = hashmap_iterator_create(node->type_data->field_map);

                while(it->end_of_iterator(it) != 0) {
                    compiler_type_field_t * field = (compiler_type_field_t*)it->get_item(it);

                    memory_free((void*)field->name);
                    memory_free((void*)field);

                    it = it->next(it);
                }

                it->destroy(it);

                hashmap_destroy(node->type_data->field_map);
            }

            memory_free(node->type_data);
        }
    }

    if(node->type == COMPILER_AST_NODE_TYPE_DECLS ||
       node->type == COMPILER_AST_NODE_TYPE_COMPOUND ||
       node->type == COMPILER_AST_NODE_TYPE_FUNCTION_CALL) {
        list_destroy_with_type(node->children, LIST_DESTROY_WITH_DATA, compiler_ast_node_destroyer);
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

int8_t compiler_ast_destroy(compiler_ast_t * ast) {
    // travels from root to leafs and frees nodes
    // in post-order traversal

    compiler_ast_node_t * node = ast->root;

    if (node == NULL) {
        return -1;
    }

    compiler_ast_node_destroy(node);

    return 0;
}
