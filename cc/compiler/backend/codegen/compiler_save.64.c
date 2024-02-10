/**
 * @file compiler_save.64.c
 * @brief Implementation of the save to memory.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>

MODULE("turnstone.compiler.codegen");

int8_t compiler_execute_save(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {

    const compiler_symbol_t * symbol = NULL;

    boolean_t need_type_field = false;

    if(compiler->with_prefix) {
        symbol = compiler_find_symbol(compiler, compiler->with_prefix->text);
        need_type_field = true;
    } else if(node->left->next) {
        if(node->left->next->next) {
            PRINTLOG(COMPILER, LOG_ERROR, "too many levels");
            return -1;
        }

        need_type_field = true;
        symbol = compiler_find_symbol(compiler, node->left->token->text);
    } else {
        symbol = compiler_find_symbol(compiler, node->left->token->text);
    }

    if(symbol == NULL) {
        PRINTLOG(COMPILER, LOG_ERROR, "symbol %s not found", node->left->token->text);
        return -1;
    }

    int64_t dst_size = symbol->size;
    compiler_symbol_type_t dst_type = symbol->type;
    compiler_symbol_type_t dst_hidden_type = symbol->hidden_type;

    int64_t extra_offset = 0;

    if(need_type_field) {
        if(symbol->type != COMPILER_SYMBOL_TYPE_CUSTOM) {
            PRINTLOG(COMPILER, LOG_ERROR, "symbol %s is not a struct", node->left->token->text);
            return -1;
        }

        const compiler_type_t* type = hashmap_get(compiler->types_by_id, (void*)symbol->custom_type_id);

        if(type == NULL) {
            PRINTLOG(COMPILER, LOG_ERROR, "type by id %lli not found", symbol->custom_type_id);
            return -1;
        }

        const compiler_type_field_t* field = NULL;

        if(compiler->with_prefix) {
            field = hashmap_get(type->field_map, node->left->token->text);
        } else if(node->left->next) {
            field = hashmap_get(type->field_map, node->left->next->token->text);
        } else {
            PRINTLOG(COMPILER, LOG_ERROR, "field error");
            return -1;
        }

        if(field == NULL) {
            PRINTLOG(COMPILER, LOG_ERROR, "field %s not found in type %s", node->left->token->text, type->name);
            return -1;
        }

        extra_offset = field->offset;
        dst_size = field->symbol_size;
        dst_type = field->symbol_type;
        dst_hidden_type = field->symbol_hidden_type;
    }

    buffer_printf(compiler->text_buffer, "# begin save\n");
    buffer_printf(compiler->text_buffer, "# begin get right\n");

    if(compiler_execute_ast_node(compiler, node->right, result) != 0) {
        return -1;
    }

    boolean_t src_is_const = compiler->is_const;
    boolean_t src_is_at_reg = compiler->is_at_reg;
    int16_t src_reg = node->right->used_register;
    compiler_symbol_type_t src_type = compiler->computed_type;
    int64_t src_const_value = *result;

    buffer_printf(compiler->text_buffer, "# end get right\n");

    int64_t array_index = 0;
    int64_t array_index_reg = -1;

    if(node->left->is_array_subscript) {
        buffer_printf(compiler->text_buffer, "# begin array subscript\n");

        if(compiler_execute_ast_node(compiler, node->left->array_subscript, &array_index) != 0) {
            PRINTLOG(COMPILER, LOG_ERROR, "cannot execute array index");
            return -1;
        }

        if(compiler->is_at_reg) {
            array_index_reg = node->left->array_subscript->used_register;
            buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                          compiler_cast_reg_to_size(compiler_regs[array_index_reg], compiler->computed_size),
                          compiler_regs[array_index_reg]);
        }

        buffer_printf(compiler->text_buffer, "# end array subscript\n");
    }

    buffer_printf(compiler->text_buffer, "# begin left\n");

    int16_t reg = compiler_find_free_reg(compiler);

    if(reg == -1) {
        PRINTLOG(COMPILER, LOG_ERROR, "no free register");
        return -1;
    }

    buffer_printf(compiler->text_buffer, "# begin load address\n");
    if(!symbol->is_local) {
        buffer_printf(compiler->text_buffer, "\tmov $%s@GOT, %%%s\n", symbol->name, compiler_regs[reg]);
        buffer_printf(compiler->text_buffer, "\tmov (%%r15, %%%s), %%%s\n", compiler_regs[reg], compiler_regs[reg]);
    } else {
        if(dst_hidden_type == COMPILER_SYMBOL_TYPE_STRING) {
            buffer_printf(compiler->text_buffer, "\tmov -%d(%%rbp), %%%s\n", symbol->stack_offset, compiler_regs[reg]);
        } else {
            buffer_printf(compiler->text_buffer, "\tlea -%d(%%rbp), %%%s\n", symbol->stack_offset, compiler_regs[reg]);
        }
    }

    if(extra_offset != 0) {
        buffer_printf(compiler->text_buffer, "\tadd $%lli, %%%s\n", extra_offset, compiler_regs[reg]);
    }

    const char_t* dest = NULL;

    if(node->left->is_array_subscript) {
        if(array_index_reg != -1) {
            int8_t scale = 1;

            if(dst_type == COMPILER_SYMBOL_TYPE_INTEGER) {
                scale = dst_size / 8;
            } else if(dst_type == COMPILER_SYMBOL_TYPE_STRING) {
                scale = 1;
            }

            dest = sprintf("%lli(%%%s, %%%s, %d)", array_index, compiler_regs[reg], compiler_regs[array_index_reg], scale);
        } else {
            dest = sprintf("%lli(%%%s)", array_index, compiler_regs[reg]);
        }
    } else {
        dest = sprintf("(%%%s)", compiler_regs[reg]);
    }

    buffer_printf(compiler->text_buffer, "# end load address %s\n", dest);

    buffer_printf(compiler->text_buffer, "# dst_size: %lli\n", dst_size);

    if(src_is_const) {
        if(src_type == COMPILER_SYMBOL_TYPE_INTEGER) {
            buffer_printf(compiler->text_buffer, "\tmov%c $0x%llx, %s\n",
                          compiler_get_reg_suffix(dst_size),
                          src_const_value,
                          dest);
        } else {
            PRINTLOG(COMPILER, LOG_ERROR, "type %d not supported", src_type);
            return -1;
        }
    } else if(src_is_at_reg) {
        buffer_printf(compiler->text_buffer, "\tmov%c %%%s, %s\n",
                      compiler_get_reg_suffix(dst_size),
                      compiler_cast_reg_to_size(compiler_regs[src_reg], dst_size),
                      dest);
        compiler->busy_regs[src_reg] = false; // free register
    } else {
        PRINTLOG(COMPILER, LOG_ERROR, "source is not at reg or const");
        return -1;
    }



    memory_free((void*)dest);

    if(node->is_array_subscript) {
        if(array_index_reg != -1) {
            compiler->busy_regs[array_index_reg] = false; // free register
        }
    }

    compiler->busy_regs[reg] = false; // free register

    buffer_printf(compiler->text_buffer, "# end left\n");
    buffer_printf(compiler->text_buffer, "# end save\n");

    return 0;
}
