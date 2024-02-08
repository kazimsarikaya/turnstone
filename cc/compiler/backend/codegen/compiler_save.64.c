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

    const compiler_symbol_t * symbol = compiler_find_symbol(compiler, node->left->token->text);

    if(symbol == NULL) {
        PRINTLOG(COMPILER, LOG_ERROR, "symbol %s not found", node->left->token->text);
        return -1;
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
    int64_t src_size = compiler->computed_size;
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
        if(symbol->hidden_type == COMPILER_SYMBOL_TYPE_STRING) {
            buffer_printf(compiler->text_buffer, "\tmov -%d(%%rbp), %%%s\n", symbol->stack_offset, compiler_regs[reg]);
        } else {
            buffer_printf(compiler->text_buffer, "\tlea -%d(%%rbp), %%%s\n", symbol->stack_offset, compiler_regs[reg]);
        }
    }

    const char_t* dest = NULL;

    if(node->left->is_array_subscript) {
        if(array_index_reg != -1) {
            int8_t scale = 1;

            if(symbol->type == COMPILER_SYMBOL_TYPE_INTEGER) {
                scale = symbol->size / 8;
            } else if(symbol->type == COMPILER_SYMBOL_TYPE_STRING) {
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

    buffer_printf(compiler->text_buffer, "# src_size: %lli\n", src_size);

    if(src_is_const) {
        if(src_type == COMPILER_SYMBOL_TYPE_INTEGER) {
            buffer_printf(compiler->text_buffer, "\tmov%c $0x%llx, %s\n",
                          compiler_get_reg_suffix(src_size),
                          src_const_value,
                          dest);
        } else {
            PRINTLOG(COMPILER, LOG_ERROR, "type %d not supported", src_type);
            return -1;
        }
    } else if(src_is_at_reg) {
        buffer_printf(compiler->text_buffer, "\tmov%c %%%s, %s\n",
                      compiler_get_reg_suffix(src_size),
                      compiler_cast_reg_to_size(compiler_regs[src_reg], src_size),
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
