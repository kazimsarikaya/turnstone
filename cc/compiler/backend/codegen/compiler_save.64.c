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

int8_t compiler_save_to_mem(compiler_t* compiler, const char_t* reg, const compiler_symbol_t* symbol) {
    int16_t swap_reg = compiler_find_free_reg(compiler);
    boolean_t need_swap = false;

    if(swap_reg == -1) {
        need_swap = true;
        swap_reg = 8;
        buffer_printf(compiler->text_buffer, "\tpush %%%s\n", compiler_regs[swap_reg]);
    }

    buffer_printf(compiler->text_buffer, "\tmov $%s@GOT, %%%s\n", symbol->name, compiler_regs[swap_reg]);
    buffer_printf(compiler->text_buffer, "\tmov (%%r15, %%%s), %%%s\n", compiler_regs[swap_reg], compiler_regs[swap_reg]);

    buffer_printf(compiler->text_buffer, "# save %s to %s cs %lli ss %lli\n", reg, symbol->name, compiler->computed_size, symbol->size);

    if(compiler->computed_size != symbol->size) {
        buffer_printf(compiler->text_buffer, "# cast %s to %lli\n", reg, symbol->size);
        buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                      compiler_cast_reg_to_size(reg, compiler->computed_size),
                      compiler_cast_reg_to_size(reg, symbol->size));
    }

    buffer_printf(compiler->text_buffer, "\tmov%c %%%s, (%%%s)\n",
                  compiler_get_reg_suffix(symbol->size),
                  compiler_cast_reg_to_size(reg, symbol->size),
                  compiler_regs[swap_reg]);

    if(need_swap) {
        buffer_printf(compiler->text_buffer, "\tpop %%%s\n", compiler_regs[swap_reg]);
    } else {
        compiler->busy_regs[swap_reg] = false;
    }

    return 0;
}

int8_t compiler_save_const_int_to_mem(compiler_t* compiler, int64_t value, const compiler_symbol_t* symbol) {
    int16_t swap_reg = compiler_find_free_reg(compiler);
    boolean_t need_swap = false;

    if(swap_reg == -1) {
        need_swap = true;
        swap_reg = 8;
        buffer_printf(compiler->text_buffer, "\tpush %%%s\n", compiler_regs[swap_reg]);
    }

    buffer_printf(compiler->text_buffer, "\tmov $%s@GOT, %%%s\n", symbol->name, compiler_regs[swap_reg]);
    buffer_printf(compiler->text_buffer, "\tmov (%%r15, %%%s), %%%s\n", compiler_regs[swap_reg], compiler_regs[swap_reg]);
    buffer_printf(compiler->text_buffer, "\tmov%c $0x%llx, (%%%s)\n",
                  compiler_get_reg_suffix(symbol->size),
                  value,
                  compiler_regs[swap_reg]);

    if(need_swap) {
        buffer_printf(compiler->text_buffer, "\tpop %%%s\n", compiler_regs[swap_reg]);
    } else {
        compiler->busy_regs[swap_reg] = false;
    }

    return 0;
}

int8_t compiler_execute_assign(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    const compiler_symbol_t * symbol = compiler_find_symbol(compiler, node->left->token->text);

    if(symbol == NULL) {
        PRINTLOG(COMPILER, LOG_ERROR, "symbol %s not found", node->left->token->text);
        return -1;
    }

    buffer_printf(compiler->text_buffer, "# begin assign %s\n", symbol->name);

    const char_t* dest = NULL;
    int16_t subscript_reg = -1;

    if(node->left->is_array_subscript) {
        buffer_printf(compiler->text_buffer, "# begin get subscript\n");

        int64_t subscript = 0;

        if(compiler_execute_ast_node(compiler, node->left->array_subscript, &subscript) != 0) {
            PRINTLOG(COMPILER, LOG_ERROR, "cannot get subscript");
            return -1;
        }

        if(compiler->is_at_reg) {
            subscript_reg = node->left->array_subscript->used_register;

            if(compiler->computed_size != 64) {
                buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                              compiler_cast_reg_to_size(compiler_regs[subscript_reg], compiler->computed_size),
                              compiler_regs[subscript_reg]);
            }

            dest = sprintf("-%lli(%%rbp, %%%s, %lli)", symbol->stack_offset, compiler_regs[subscript_reg], symbol->size / 8);

        } else if(compiler->is_const) {
            dest = sprintf("-%lli(%%rbp)", symbol->stack_offset - subscript * symbol->size / 8);
        } else if(compiler->is_at_stack) {
            subscript_reg = compiler_find_free_reg(compiler);

            if(subscript_reg == -1) {
                PRINTLOG(COMPILER, LOG_ERROR, "no free registers");
                return -1;
            }

            buffer_printf(compiler->text_buffer, "\tmov%c -%lli(%%rbp), %%%s\n",
                          compiler_get_reg_suffix(compiler->computed_size),
                          compiler->at_stack_offset,
                          compiler_cast_reg_to_size(compiler_regs[subscript_reg], compiler->computed_size));

            if(compiler->computed_size != 64) {
                buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                              compiler_cast_reg_to_size(compiler_regs[subscript_reg], compiler->computed_size),
                              compiler_regs[subscript_reg]);
            }

            dest = sprintf("-%lli(%%rbp, %%%s, %lli)", symbol->stack_offset, compiler_regs[subscript_reg], symbol->size / 8);
        } else {
            PRINTLOG(COMPILER, LOG_ERROR, "subscript is not at reg or const");
            return -1;
        }

        buffer_printf(compiler->text_buffer, "# end get subscript\n");

    } else {
        dest = sprintf("-%lli(%%rbp)", symbol->stack_offset);
    }

    buffer_printf(compiler->text_buffer, "# begin get right\n");

    if(compiler_execute_ast_node(compiler, node->right, result) != 0) {
        return -1;
    }

    buffer_printf(compiler->text_buffer, "# end get right\n");

    ((compiler_symbol_t*)symbol)->int_value = *result;

    if(compiler->is_at_mem) {
        if(symbol->is_local) {
            buffer_printf(compiler->text_buffer, "\tmov%c %s, %s\n",
                          compiler_get_reg_suffix(symbol->size),
                          node->right->token->text,
                          dest);
        } else {
            int16_t swap_reg = compiler_find_free_reg(compiler);
            boolean_t need_swap = false;

            if(swap_reg == -1) {
                need_swap = true;
                swap_reg = 8;
                buffer_printf(compiler->text_buffer, "\tpush %%%s\n", compiler_regs[swap_reg]);
            }

            buffer_printf(compiler->text_buffer, "\tmov%c %s, %%%s\n",
                          compiler_get_reg_suffix(symbol->size),
                          node->right->token->text,
                          compiler_cast_reg_to_size(compiler_regs[swap_reg], symbol->size));

            compiler_save_to_mem(compiler, compiler_regs[swap_reg], symbol);

            if(need_swap) {
                buffer_printf(compiler->text_buffer, "\tpop %%%s\n", compiler_regs[swap_reg]);
            } else {
                compiler->busy_regs[swap_reg] = false;
            }
        }

    } else if(compiler->is_at_reg) {
        if(symbol->is_local) {
            buffer_printf(compiler->text_buffer, "\tmov%c %%%s, %s\n",
                          compiler_get_reg_suffix(symbol->size),
                          compiler_cast_reg_to_size(compiler_regs[node->right->used_register], symbol->size),
                          dest);
        } else {
            compiler_save_to_mem(compiler, compiler_regs[node->right->used_register], symbol);
        }

        compiler->is_at_reg = false;
        compiler->busy_regs[node->right->used_register] = false;
    } else if(compiler->is_const) {
        if(symbol->is_local) {
            buffer_printf(compiler->text_buffer, "\tmov%c $0x%llx, %s\n",
                          compiler_get_reg_suffix(symbol->size),
                          *result,
                          dest);
        } else {
            compiler_save_const_int_to_mem(compiler, *result, symbol);
        }

        compiler->is_const = false;
    } else if(compiler->is_at_stack) {
        int16_t swap_reg = compiler_find_free_reg(compiler);
        boolean_t need_swap = false;

        if(swap_reg == -1) {
            need_swap = true;
            swap_reg = 8;
            buffer_printf(compiler->text_buffer, "\tpush %%%s\n", compiler_regs[swap_reg]);
        }

        buffer_printf(compiler->text_buffer, "\tmov%c -%lli(%%rbp), %%%s\n",
                      compiler_get_reg_suffix(symbol->size),
                      compiler->at_stack_offset,
                      compiler_cast_reg_to_size(compiler_regs[swap_reg], symbol->size));

        if(symbol->is_local) {
            buffer_printf(compiler->text_buffer, "\tmov%c %%%s, %s\n",
                          compiler_get_reg_suffix(symbol->size),
                          compiler_cast_reg_to_size(compiler_regs[swap_reg], symbol->size),
                          dest);
        } else {
            compiler_save_to_mem(compiler, compiler_regs[swap_reg], symbol);
        }

        if(need_swap) {
            buffer_printf(compiler->text_buffer, "\tpop %%%s\n", compiler_regs[swap_reg]);
        } else {
            compiler->busy_regs[swap_reg] = false;
        }


        compiler->is_at_stack = false;
    } else {
        int16_t swap_reg = compiler_find_free_reg(compiler);
        boolean_t need_swap = false;

        if(swap_reg == -1) {
            need_swap = true;
            swap_reg = 8;
            buffer_printf(compiler->text_buffer, "\tpush %%%s\n", compiler_regs[swap_reg]);
        }

        buffer_printf(compiler->text_buffer, "\tpop %%%s\n", compiler_regs[swap_reg]);

        if(symbol->is_local) {
            buffer_printf(compiler->text_buffer, "\tmov%c %%%s, %s\n",
                          compiler_get_reg_suffix(symbol->size),
                          compiler_cast_reg_to_size(compiler_regs[swap_reg], symbol->size),
                          dest);
        } else {
            compiler_save_to_mem(compiler, compiler_regs[swap_reg], symbol);
        }

        if(need_swap) {
            buffer_printf(compiler->text_buffer, "\tpop %%%s\n", compiler_regs[swap_reg]);
        } else {
            compiler->busy_regs[swap_reg] = false;
        }

    }

    if(subscript_reg != -1) {
        compiler->busy_regs[subscript_reg] = false;
    }

    memory_free((void*)dest);

    buffer_printf(compiler->text_buffer, "# end assign %s\n", symbol->name);

    return 0;
}


