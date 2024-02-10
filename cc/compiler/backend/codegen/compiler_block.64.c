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

int8_t compiler_execute_block_var(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t compiler_execute_block_type(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t compiler_execute_block_var_int(compiler_t* compiler, compiler_symbol_t* symbol, int64_t* result);
int8_t compiler_execute_block_var_custom_type(compiler_t* compiler, compiler_symbol_t* symbol, int64_t* result);

int8_t compiler_execute_block_type(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    UNUSED(result);

    if(node->type != COMPILER_AST_NODE_TYPE_TYPE) {
        PRINTLOG(COMPILER, LOG_ERROR, "expected type");
        return -1;
    }

    int total_size = 0;

    node->type_data->field_map = hashmap_string(128);

    if(node->type_data->field_map == NULL) {
        PRINTLOG(COMPILER, LOG_ERROR, "failed to create field map");
        return -1;
    }

    if(compiler->types_by_id == NULL) {
        compiler->types_by_id = hashmap_integer(128);

        if(compiler->types_by_id == NULL) {
            PRINTLOG(COMPILER, LOG_ERROR, "failed to create types_by_id map");
            return -1;
        }
    }

    if(compiler->types_by_name == NULL) {
        compiler->types_by_name = hashmap_string(128);

        if(compiler->types_by_name == NULL) {
            PRINTLOG(COMPILER, LOG_ERROR, "failed to create types_by_name map");
            return -1;
        }
    }

    if(hashmap_get(compiler->types_by_id, (void*)node->type_data->id) != NULL) {
        PRINTLOG(COMPILER, LOG_ERROR, "type id %lli already defined", node->type_data->id);
        return -1;
    }

    if(hashmap_get(compiler->types_by_name, node->type_data->name) != NULL) {
        PRINTLOG(COMPILER, LOG_ERROR, "type %s already defined", node->type_data->name);
        return -1;
    }

    hashmap_put(compiler->types_by_name, node->type_data->name, node->type_data);
    hashmap_put(compiler->types_by_id, (void*)node->type_data->id, node->type_data);

    while(linkedlist_size(node->type_data->fields)) {
        compiler_ast_node_t * field_node = (compiler_ast_node_t*)linkedlist_queue_pop(node->type_data->fields);

        for(size_t i = 0; i < linkedlist_size(field_node->children); i++) {
            compiler_symbol_t * tmp_symbol = (compiler_symbol_t*)linkedlist_get_data_at_position(field_node->children, i);

            if(hashmap_get(node->type_data->field_map, tmp_symbol->name) != NULL) {
                PRINTLOG(COMPILER, LOG_ERROR, "field %s already defined", tmp_symbol->name);
                return -1;
            }

            compiler_type_field_t* field = memory_malloc(sizeof(compiler_type_field_t));

            if(field == NULL) {
                PRINTLOG(COMPILER, LOG_ERROR, "failed to allocate field");
                return -1;
            }

            field->name = strdup(tmp_symbol->name);
            field->symbol_size = tmp_symbol->size;
            field->symbol_type = tmp_symbol->type;
            field->symbol_hidden_type = tmp_symbol->hidden_type;
            field->offset = total_size;

            if(tmp_symbol->type == COMPILER_SYMBOL_TYPE_INTEGER) {
                if(tmp_symbol->is_array) {
                    total_size += tmp_symbol->size / 8 * tmp_symbol->array_size;
                    field->size = tmp_symbol->size / 8 * tmp_symbol->array_size;
                } else {
                    total_size += tmp_symbol->size / 8;
                    field->size = tmp_symbol->size / 8;
                }

                if(!node->type_data->is_packed && total_size % 8 != 0) {
                    total_size += 8 - (total_size % 8);
                }

                if(!node->type_data->is_packed && field->size % 8 != 0) {
                    field->size += 8 - (field->size % 8);
                }

            } else {
                PRINTLOG(COMPILER, LOG_ERROR, "expected integer");
                return -1;
            }

            hashmap_put(node->type_data->field_map, field->name, field);
        }

        compiler_ast_node_destroy(field_node);
    }


    node->type_data->size = total_size;

    return 0;
}

int8_t compiler_execute_block_var_custom_type(compiler_t* compiler, compiler_symbol_t* symbol, int64_t* result) {
    UNUSED(result);

    if(symbol->type != COMPILER_SYMBOL_TYPE_CUSTOM) {
        PRINTLOG(COMPILER, LOG_ERROR, "expected custom type");
        return -1;
    }

    int64_t symbol_custom_type_id = symbol->custom_type_id;

    const compiler_type_t* custom_type = hashmap_get(compiler->types_by_id, (void*)symbol_custom_type_id);

    if(custom_type == NULL) {
        PRINTLOG(COMPILER, LOG_ERROR, "custom type %lli not found", symbol_custom_type_id);
        return -1;
    }

    symbol->size = custom_type->size;

    int64_t symbol_size = symbol->size;

    if(symbol->is_array) {
        symbol_size = symbol->array_size * symbol_size;
    }

    compiler_define_symbol(compiler, symbol, symbol_size);
    buffer_printf(compiler->bss_buffer, "\t.zero %lli\n", symbol_size);

    return 0;
}

int8_t compiler_execute_block_var_int(compiler_t* compiler, compiler_symbol_t* symbol, int64_t* result) {
    UNUSED(result);

    int64_t symbol_size = symbol->size / 8;
    size_t array_size = symbol->array_size;
    int64_t symbol_type_size = symbol_size;

    if(symbol->is_array) {
        symbol_size = array_size * symbol_size;

        if(symbol->string_value) {
            symbol_size = strlen(symbol->string_value) + 1;
            symbol_type_size = 1;
            array_size++;
        }
    }

    const char_t* symbol_type = "int";

    switch(symbol_type_size) {
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

    if(symbol->is_local){
        PRINTLOG(COMPILER, LOG_ERROR, "local symbol %s type %d size %lli value %lli", symbol->name, symbol->type, symbol->size, symbol->int_value);
        // symbol->stack_offset = compiler->next_stack_offset;
        // compiler->next_stack_offset += symbol->size;
        // compiler->stack_size += symbol->size;
    } else {
        symbol->stack_offset = 0;

        if(symbol->is_const){
            PRINTLOG(COMPILER, LOG_INFO, "rodata symbol %s type %d size %lli value %lli", symbol->name, symbol->type, symbol->size, symbol->int_value);
            compiler_define_symbol(compiler, symbol, symbol_size);

            if(symbol->is_array) {
                if(symbol->string_value) {
                    buffer_printf(compiler->rodata_buffer, "\t.string \"%s\"\n", symbol->string_value);
                } else {
                    for(size_t item = 0; item < array_size; item++) {
                        buffer_printf(compiler->rodata_buffer, "\t.%s %lli\n", symbol_type, symbol->int_array_value[item]);
                    }
                }
            } else {
                buffer_printf(compiler->rodata_buffer, "\t.%s %lli\n", symbol_type, symbol->int_value);
            }

            buffer_printf(compiler->rodata_buffer, "\n\n");
        } else {
            if(!symbol->initilized) {
                PRINTLOG(COMPILER, LOG_INFO, "bss symbol %s type %d size %lli value %lli", symbol->name, symbol->type, symbol->size, symbol->int_value);

                compiler_define_symbol(compiler, symbol, symbol_size);

                if(symbol->is_array) {
                    for(size_t item = 0; item < array_size; item++) {
                        UNUSED(item);
                        buffer_printf(compiler->bss_buffer, "\t.%s 0\n", symbol_type);
                    }
                } else {
                    buffer_printf(compiler->bss_buffer, "\t.%s 0\n", symbol_type);
                }

                buffer_printf(compiler->bss_buffer, "\n\n");
            } else {
                PRINTLOG(COMPILER, LOG_INFO, "data symbol %s type %d size %lli value %lli", symbol->name, symbol->type, symbol->size, symbol->int_value);
                compiler_define_symbol(compiler, symbol, symbol_size);

                if(symbol->is_array) {
                    if(symbol->string_value) {
                        buffer_printf(compiler->data_buffer, "\t.string \"%s\"\n", symbol->string_value);
                    } else {
                        for(size_t item = 0; item < array_size; item++) {
                            buffer_printf(compiler->data_buffer, "\t.%s %lli\n", symbol_type, symbol->int_array_value[item]);
                        }
                    }
                } else {
                    buffer_printf(compiler->data_buffer, "\t.%s %lli\n", symbol_type, symbol->int_value);
                }

                buffer_printf(compiler->data_buffer, "\n\n");
            }
        }
    }

    return 0;
}

int8_t compiler_execute_block_var(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    UNUSED(result);

    for(size_t j = 0; j < linkedlist_size(node->children); j++) {
        compiler_symbol_t * tmp_symbol = (compiler_symbol_t*)linkedlist_get_data_at_position(node->children, j);

        if(strcmp(compiler->program_name, tmp_symbol->name) != 0 && hashmap_get(compiler->current_symbol_table->symbols, tmp_symbol->name) != NULL) {
            PRINTLOG(COMPILER, LOG_ERROR, "symbol %s already defined", tmp_symbol->name);
            return -1;
        }

        if(tmp_symbol->type == COMPILER_SYMBOL_TYPE_INTEGER) {
            if(compiler_execute_block_var_int(compiler, tmp_symbol, result) != 0) {
                PRINTLOG(COMPILER, LOG_ERROR, "failed to execute block var int");
                return -1;
            }
        } else if(tmp_symbol->type == COMPILER_SYMBOL_TYPE_CUSTOM){
            if(compiler_execute_block_var_custom_type(compiler, tmp_symbol, result) != 0) {
                PRINTLOG(COMPILER, LOG_ERROR, "failed to execute block var custom type");
                return -1;
            }
        } else {
            PRINTLOG(COMPILER, LOG_ERROR, "expected integer");
            return -1;

        }

        hashmap_put(compiler->current_symbol_table->symbols, tmp_symbol->name, tmp_symbol);
    }

    return 0;
}

int8_t compiler_execute_block(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    if(node->left) {
        if(node->left->type == COMPILER_AST_NODE_TYPE_DECLS) {

            for(size_t i = 0; i < linkedlist_size(node->left->children); i++) {
                compiler_ast_node_t * tmp_node = (compiler_ast_node_t*)linkedlist_get_data_at_position(node->left->children, i);

                int8_t ret = -1;

                if(tmp_node->type == COMPILER_AST_NODE_TYPE_VAR) {
                    ret = compiler_execute_block_var(compiler, tmp_node, result);
                } else if(tmp_node->type == COMPILER_AST_NODE_TYPE_TYPE) {
                    ret = compiler_execute_block_type(compiler, tmp_node, result);
                } else {
                    PRINTLOG(COMPILER, LOG_ERROR, "expected var, type, got %d", tmp_node->type);
                    return -1;
                }

                if(ret != 0) {
                    PRINTLOG(COMPILER, LOG_ERROR, "failed to execute block");

                    return -1;
                }

            }
        } else if(node->left->type != COMPILER_AST_NODE_TYPE_NO_OP) {
            PRINTLOG(COMPILER, LOG_ERROR, "expected decls: %d", node->left->type);
            return -1;
        }
    } else {
        PRINTLOG(COMPILER, LOG_INFO, "no decls");
    }

    return compiler_execute_ast_node(compiler, node->right, result);
}


