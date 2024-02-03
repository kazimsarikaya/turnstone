/**
 * @file 1213.c
 * @brief
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>
#include <utils.h>

MODULE("turnstone.compiler.codegen");

int8_t compiler_execute_compound(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    if(node->children == NULL) {
        return 0;
    }

    symbol_table_t* symbol_table = memory_malloc(sizeof(symbol_table_t));

    if (symbol_table == NULL) {
        return -1;
    }

    symbol_table->symbols = hashmap_string(128);

    if (symbol_table->symbols == NULL) {
        memory_free(symbol_table);
        return -1;
    }

    symbol_table->parent = compiler->current_symbol_table;
    compiler->current_symbol_table = symbol_table;

    for(size_t i = 0; i < linkedlist_size(node->children); i++) {
        compiler_ast_node_t * tmp_node = (compiler_ast_node_t*)linkedlist_get_data_at_position(node->children, i);

        if(tmp_node->type == COMPILER_AST_NODE_TYPE_DECLS) {
            for(size_t j = 0; j < linkedlist_size(tmp_node->children); j++) {
                compiler_ast_node_t * tmp_var_node = (compiler_ast_node_t*)linkedlist_get_data_at_position(tmp_node->children, j);

                for(size_t k = 0; k < linkedlist_size(tmp_var_node->children); k++) {
                    compiler_symbol_t * tmp_symbol = (compiler_symbol_t*)linkedlist_get_data_at_position(tmp_var_node->children, k);

                    if(strcmp(compiler->program_name, tmp_symbol->name) != 0 && hashmap_get(compiler->current_symbol_table->symbols, tmp_symbol->name) != NULL) {
                        PRINTLOG(COMPILER, LOG_ERROR, "symbol %s already defined", tmp_symbol->name);
                        return -1;
                    }

                    if(tmp_symbol->type == COMPILER_SYMBOL_TYPE_INTEGER) {
                        PRINTLOG(COMPILER, LOG_INFO, "local symbol %s type %d size %lli value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);

                        if(tmp_symbol->is_array) {
                            compiler->next_stack_offset += tmp_symbol->size / 8 * tmp_symbol->array_size;
                            compiler->stack_size += tmp_symbol->size / 8 * tmp_symbol->array_size;

                            if(compiler->next_stack_offset % 8) {
                                compiler->next_stack_offset += 8 - compiler->next_stack_offset % 8;
                                compiler->stack_size += 8 - compiler->stack_size % 8;
                            }

                            tmp_symbol->stack_offset = compiler->next_stack_offset - 8;
                        } else {
                            tmp_symbol->stack_offset = compiler->next_stack_offset;
                            compiler->next_stack_offset += 8; // tmp_symbol->size;
                            compiler->stack_size += 8; // tmp_symbol->size;

                        }
                    }

                    hashmap_put(compiler->current_symbol_table->symbols, tmp_symbol->name, tmp_symbol);
                }

            }
        }
    }

    if(compiler->stack_size % 16) {
        compiler->stack_size += 8 - compiler->stack_size % 16;
    }

    if(compiler_build_stack(compiler) != 0) {
        PRINTLOG(COMPILER, LOG_ERROR, "cannot build stack");
        return -1;
    }

    for(size_t i = 0; i < linkedlist_size(node->children); i++) {
        compiler_ast_node_t * tmp_node = (compiler_ast_node_t*)linkedlist_get_data_at_position(node->children, i);

        if(tmp_node->type != COMPILER_AST_NODE_TYPE_DECLS && compiler_execute_ast_node(compiler, tmp_node, result) != 0) {
            return -1;
        }

        if(compiler->is_at_reg) {
            buffer_printf(compiler->text_buffer, "# free register %s\n", compiler_regs[tmp_node->used_register]);
            compiler->busy_regs[tmp_node->used_register] = false;
        }
    }

    compiler->current_symbol_table = compiler->current_symbol_table->parent;

    hashmap_destroy(symbol_table->symbols);
    memory_free(symbol_table);

    return 0;
}


