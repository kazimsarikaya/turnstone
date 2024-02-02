/**
 * @file compiler_block.64.c
 * @brief compiler block functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>

MODULE("turnstone.compiler.codegen");

int8_t compiler_execute_block(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    if(node->left) {
        if(node->left->type == COMPILER_AST_NODE_TYPE_DECLS) {

            for(size_t i = 0; i < linkedlist_size(node->left->children); i++) {
                compiler_ast_node_t * tmp_node = (compiler_ast_node_t*)linkedlist_get_data_at_position(node->left->children, i);

                for(size_t j = 0; j < linkedlist_size(tmp_node->children); j++) {
                    compiler_symbol_t * tmp_symbol = (compiler_symbol_t*)linkedlist_get_data_at_position(tmp_node->children, j);

                    if(strcmp(compiler->program_name, tmp_symbol->name) != 0 && hashmap_get(compiler->current_symbol_table->symbols, tmp_symbol->name) != NULL) {
                        PRINTLOG(COMPILER, LOG_ERROR, "symbol %s already defined", tmp_symbol->name);
                        return -1;
                    }

                    if(tmp_symbol->type == COMPILER_SYMBOL_TYPE_INTEGER) {

                        int64_t symbol_size = tmp_symbol->size / 8;

                        if(tmp_symbol->is_array) {
                            symbol_size = tmp_symbol->array_size * symbol_size;
                        }

                        const char_t* symbol_type = "int";

                        switch(symbol_size) {
                        case 1:
                            symbol_type = "byte";
                            break;
                        case 2:
                            symbol_type = "word";
                            break;
                        case 4:
                            symbol_type = "long";
                            break;
                        case 8:
                            symbol_type = "quad";
                            break;
                        default:
                            break;
                        }

                        if(tmp_symbol->is_local){
                            PRINTLOG(COMPILER, LOG_ERROR, "local symbol %s type %d size %lli value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);
                            // tmp_symbol->stack_offset = compiler->next_stack_offset;
                            // compiler->next_stack_offset += tmp_symbol->size;
                            // compiler->stack_size += tmp_symbol->size;
                        } else {
                            tmp_symbol->stack_offset = 0;

                            if(tmp_symbol->is_const){
                                PRINTLOG(COMPILER, LOG_INFO, "rodata symbol %s type %d size %lli value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);
                                buffer_printf(compiler->rodata_buffer, ".section .rodata.%s\n", tmp_symbol->name);
                                buffer_printf(compiler->rodata_buffer, ".align 8\n");
                                buffer_printf(compiler->rodata_buffer, ".local %s\n", tmp_symbol->name);
                                buffer_printf(compiler->rodata_buffer, ".type %s, @object\n", tmp_symbol->name);
                                buffer_printf(compiler->rodata_buffer, ".size %s, %lli\n", tmp_symbol->name, symbol_size);
                                buffer_printf(compiler->rodata_buffer, "%s:\n", tmp_symbol->name);

                                if(tmp_symbol->is_array) {
                                    for(size_t item = 0; item < tmp_symbol->array_size; item++) {
                                        buffer_printf(compiler->rodata_buffer, "\t.%s %lli\n", symbol_type, tmp_symbol->int_array_value[item]);
                                    }
                                } else {
                                    buffer_printf(compiler->rodata_buffer, "\t.%s %lli\n", symbol_type, tmp_symbol->int_value);
                                }

                                buffer_printf(compiler->rodata_buffer, "\n\n\n");
                            } else {
                                if(!tmp_symbol->initilized) {
                                    PRINTLOG(COMPILER, LOG_INFO, "bss symbol %s type %d size %lli value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);
                                    buffer_printf(compiler->bss_buffer, ".section .bss.%s\n", tmp_symbol->name);
                                    buffer_printf(compiler->bss_buffer, ".align 8\n");
                                    buffer_printf(compiler->bss_buffer, ".local %s\n", tmp_symbol->name);
                                    buffer_printf(compiler->bss_buffer, ".type %s, @object\n", tmp_symbol->name);
                                    buffer_printf(compiler->bss_buffer, ".size %s, %lli\n", tmp_symbol->name, symbol_size);
                                    buffer_printf(compiler->bss_buffer, "%s:\n", tmp_symbol->name);

                                    if(tmp_symbol->is_array) {
                                        for(size_t item = 0; item < tmp_symbol->array_size; item++) {
                                            UNUSED(item);
                                            buffer_printf(compiler->bss_buffer, "\t.%s 0\n", symbol_type);
                                        }
                                    } else {
                                        buffer_printf(compiler->bss_buffer, "\t.%s 0\n", symbol_type);
                                    }

                                    buffer_printf(compiler->bss_buffer, "\n\n\n");
                                } else {
                                    PRINTLOG(COMPILER, LOG_INFO, "data symbol %s type %d size %lli value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);
                                    buffer_printf(compiler->data_buffer, ".section .data.%s\n", tmp_symbol->name);
                                    buffer_printf(compiler->data_buffer, ".align 8\n");
                                    buffer_printf(compiler->data_buffer, ".local %s\n", tmp_symbol->name);
                                    buffer_printf(compiler->data_buffer, ".type %s, @object\n", tmp_symbol->name);
                                    buffer_printf(compiler->data_buffer, ".size %s, %lli\n", tmp_symbol->name, symbol_size);
                                    buffer_printf(compiler->data_buffer, "%s:\n", tmp_symbol->name);

                                    if(tmp_symbol->is_array) {
                                        for(size_t item = 0; item < tmp_symbol->array_size; item++) {
                                            buffer_printf(compiler->data_buffer, "\t.%s %lli\n", symbol_type, tmp_symbol->int_array_value[item]);
                                        }
                                    } else {
                                        buffer_printf(compiler->data_buffer, "\t.%s %lli\n", symbol_type, tmp_symbol->int_value);
                                    }

                                    buffer_printf(compiler->data_buffer, "\n\n\n");
                                }
                            }
                        }
                    }

                    hashmap_put(compiler->current_symbol_table->symbols, tmp_symbol->name, tmp_symbol);
                }
            }
        } else {
            PRINTLOG(COMPILER, LOG_ERROR, "expected decls");
            return -1;
        }
    } else {
        PRINTLOG(COMPILER, LOG_INFO, "no decls");
    }

    return compiler_execute_ast_node(compiler, node->right, result);
}


