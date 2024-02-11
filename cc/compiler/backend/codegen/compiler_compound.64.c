/**
 * @file compiler_compound.64.c
 * @brief Compound statement code generation
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

    compiler_symbol_table_t* symbol_table = memory_malloc(sizeof(compiler_symbol_table_t));

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
                            if(tmp_symbol->hidden_type == COMPILER_SYMBOL_TYPE_STRING) {
                                compiler->next_stack_offset += sizeof(char_t*);
                                compiler->stack_size += sizeof(char_t*);

                                size_t array_size = tmp_symbol->array_size;

                                compiler_define_symbol(compiler, tmp_symbol, array_size);

                                if(tmp_symbol->initilized) {
                                    if(tmp_symbol->is_const) {
                                        buffer_printf(compiler->rodata_buffer, "\t.string \"%s\"\n\n\n", tmp_symbol->string_value);
                                    } else {
                                        buffer_printf(compiler->data_buffer, "\t.string \"%s\"\n\n\n", tmp_symbol->string_value);
                                    }
                                } else {
                                    buffer_printf(compiler->bss_buffer, "\t.zero %lli\n\n\n", array_size + 1);
                                }

                            } else {
                                compiler->next_stack_offset += tmp_symbol->size / 8 * tmp_symbol->array_size;
                                compiler->stack_size += tmp_symbol->size / 8 * tmp_symbol->array_size;
                            }

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
                    } else if(tmp_symbol->type == COMPILER_SYMBOL_TYPE_CUSTOM) {
                        int64_t symbol_custom_type_id = tmp_symbol->custom_type_id;

                        const compiler_type_t* custom_type = hashmap_get(compiler->types_by_id, (void*)symbol_custom_type_id);

                        if(custom_type == NULL) {
                            PRINTLOG(COMPILER, LOG_ERROR, "custom type %lli not found", symbol_custom_type_id);
                            return -1;
                        }

                        tmp_symbol->size = custom_type->size;

                        int64_t symbol_size = tmp_symbol->size;

                        if(tmp_symbol->is_array) {
                            symbol_size = tmp_symbol->array_size * symbol_size;
                        }

                        tmp_symbol->stack_offset = compiler->next_stack_offset + symbol_size - 8;

                        compiler->next_stack_offset += symbol_size;
                        compiler->stack_size += symbol_size;

                        if(compiler->next_stack_offset % 8) {
                            compiler->next_stack_offset += 8 - compiler->next_stack_offset % 8;
                            compiler->stack_size += 8 - compiler->stack_size % 8;
                        }

                    } else {
                        PRINTLOG(COMPILER, LOG_ERROR, "unknown symbol type %d", tmp_symbol->type);
                        return -1;
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

        if(tmp_node->is_at_reg) {
            buffer_printf(compiler->text_buffer, "# free register %s\n", compiler_regs[tmp_node->used_register]);
            compiler->busy_regs[tmp_node->used_register] = false;
        }
    }

    compiler->current_symbol_table = compiler->current_symbol_table->parent;

    hashmap_destroy(symbol_table->symbols);
    memory_free(symbol_table);

    return 0;
}


