/**
 * @file compiler_binaryop.64.c
 * @brief Implementation of the binary operators.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>
#include <utils.h>

MODULE("turnstone.compiler.codegen");


int8_t compiler_execute_binary_op(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    int64_t left = 0;
    int64_t right = 0;

    buffer_printf(compiler->text_buffer, "# begin binary op\n");

    if(compiler_execute_ast_node(compiler, node->left, &left) != 0) {
        return -1;
    }

    boolean_t left_is_const = node->left->is_const;
    int16_t left_at_reg = node->left->used_register;
    int64_t left_size = node->left->computed_size;
    compiler_symbol_type_t left_type = node->left->computed_type;

    if(compiler_execute_ast_node(compiler, node->right, &right) != 0) {
        return -1;
    }

    boolean_t right_is_const = node->right->is_const;
    int16_t right_at_reg = node->right->used_register;
    int64_t right_size = node->right->computed_size;
    compiler_symbol_type_t right_type = node->right->computed_type;

    if(left_is_const && right_is_const) {
        node->is_const = true;
    }

    int64_t max_size = MAX(left_size, right_size);

    node->computed_size = max_size;
    node->computed_type = left_type; // TODO: check if types are compatible

    char_t reg_suffix = compiler_get_reg_suffix(max_size);

    const char_t* left_reg = compiler_cast_reg_to_size(compiler_regs[left_at_reg], max_size);
    const char_t* right_reg = compiler_cast_reg_to_size(compiler_regs[right_at_reg], max_size);

    if(!left_is_const && left_size < max_size) {
        buffer_printf(compiler->text_buffer, "# cast left %lli to %lli\n", left_size, max_size);
        buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                      compiler_cast_reg_to_size(compiler_regs[left_at_reg], left_size),
                      left_reg);
    }

    if(!right_is_const && right_size < max_size) {
        buffer_printf(compiler->text_buffer, "# cast right %lli to %lli\n", right_size, max_size);
        buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                      compiler_cast_reg_to_size(compiler_regs[right_at_reg], right_size),
                      right_reg);
    }

    if (node->token->type == COMPILER_TOKEN_TYPE_PLUS) {
        *result = left + right;
        if(!node->is_const) {
            if(left_is_const) {
                buffer_printf(compiler->text_buffer, "\tadd%c $0x%llx, %%%s\n", reg_suffix, left, right_reg);
                node->used_register = right_at_reg;
            } else if(right_is_const) {
                buffer_printf(compiler->text_buffer, "\tadd%c $0x%llx, %%%s\n", reg_suffix, right, left_reg);
                node->used_register = left_at_reg;
            } else {
                buffer_printf(compiler->text_buffer, "\tadd%c %%%s, %%%s\n", reg_suffix, left_reg, right_reg);
                node->used_register = right_at_reg;
                compiler->busy_regs[left_at_reg] = false;
            }
            node->is_at_reg = true;
        }
    } else if (node->token->type == COMPILER_TOKEN_TYPE_MINUS) {
        *result = left - right;

        if(!node->is_const) {
            if(left_is_const) {
                buffer_printf(compiler->text_buffer, "\tsub%c $0x%llx, %%%s\n", reg_suffix, left, right_reg);
                node->used_register = right_at_reg;
            } else if(right_is_const) {
                buffer_printf(compiler->text_buffer, "\tsub%c $0x%llx, %%%s\n", reg_suffix, right, left_reg);
                node->used_register = left_at_reg;
            } else {
                buffer_printf(compiler->text_buffer, "\tsub%c %%%s, %%%s\n", reg_suffix, right_reg, left_reg);
                node->used_register = left_at_reg;
                compiler->busy_regs[right_at_reg] = false;
            }
            node->is_at_reg = true;
        }
    } else if (node->token->type == COMPILER_TOKEN_TYPE_MULTIPLY) {
        *result = left * right;

        if(!node->is_const) {
            if(left_is_const) {
                buffer_printf(compiler->text_buffer, "\timul%c $0x%llx, %%%s\n", reg_suffix, left, right_reg);
                node->used_register = right_at_reg;
            } else if(right_is_const) {
                buffer_printf(compiler->text_buffer, "\timul%c $0x%llx, %%%s\n", reg_suffix, right, left_reg);
                node->used_register = left_at_reg;
            } else {
                buffer_printf(compiler->text_buffer, "\timul%c %%%s, %%%s\n", reg_suffix, left_reg, right_reg);
                node->used_register = right_at_reg;
                compiler->busy_regs[left_at_reg] = false;
            }
            node->is_at_reg = true;
        }
    } else if (node->token->type == COMPILER_TOKEN_TYPE_INTEGER_DIVIDE) {
        *result = left / right;

        if(!node->is_const) {
            boolean_t need_swap = false;
            int16_t swap_reg = -1;

            if(left_at_reg != 0) {
                if(compiler->busy_regs[0]) {
                    need_swap = true;
                    swap_reg = compiler_find_free_reg(compiler);

                    if(swap_reg == -1) {
                        buffer_printf(compiler->text_buffer, "\tpush %%rax\n");
                    } else {
                        buffer_printf(compiler->text_buffer, "\tmov %%rax, %%%s\n", compiler_regs[swap_reg]);
                    }
                }
            }

            if(left_is_const) {
                buffer_printf(compiler->text_buffer, "\tmov%c $0x%llx, %%%s\n", reg_suffix, left, compiler_cast_reg_to_size("rax", max_size));
                buffer_printf(compiler->text_buffer, "\tcqo\n");
                buffer_printf(compiler->text_buffer, "\tidiv%c %%%s\n", reg_suffix, right_reg);
                node->used_register = 0;
            } else if(right_is_const) {
                boolean_t old_rdx_busy = compiler->busy_regs[3];
                compiler->busy_regs[3] = true;
                int16_t const_swap_reg = compiler_find_free_reg(compiler);
                compiler->busy_regs[3] = old_rdx_busy;

                if(const_swap_reg == -1) {
                    const_swap_reg = 8;
                    buffer_printf(compiler->text_buffer, "\tpush %%%s\n", compiler_regs[right_at_reg]);
                }

                buffer_printf(compiler->text_buffer, "\tmov%c $0x%llx, %%%s\n", reg_suffix, right, compiler_cast_reg_to_size(compiler_regs[const_swap_reg], max_size));


                buffer_printf(compiler->text_buffer, "\tmov%c %%%s, %%%s\n",
                              reg_suffix,
                              compiler_cast_reg_to_size(compiler_regs[left_at_reg], max_size),
                              compiler_cast_reg_to_size("rax", max_size));
                buffer_printf(compiler->text_buffer, "\tcqo\n");
                buffer_printf(compiler->text_buffer, "\tidiv%c %%%s\n", reg_suffix, compiler_cast_reg_to_size(compiler_regs[const_swap_reg], max_size));
                compiler->busy_regs[left_at_reg] = false;
                node->used_register = 0;

                if(const_swap_reg == -1) {
                    buffer_printf(compiler->text_buffer, "\tpop %%%s\n", compiler_regs[const_swap_reg]);
                } else {
                    compiler->busy_regs[const_swap_reg] = false;
                }
            } else {
                buffer_printf(compiler->text_buffer, "\tmov%c %%%s, %%rax\n", reg_suffix, compiler_cast_reg_to_size(compiler_regs[left_at_reg], max_size));
                buffer_printf(compiler->text_buffer, "\tcqo\n");
                buffer_printf(compiler->text_buffer, "\tidiv%c %%%s\n", reg_suffix, compiler_cast_reg_to_size(compiler_regs[right_at_reg], max_size));
                node->used_register = 0;
                compiler->busy_regs[left_at_reg] = false;
                compiler->busy_regs[right_at_reg] = false;
            }

            compiler->busy_regs[0] = true;
            node->is_at_reg = true;

            if(need_swap) {
                if(swap_reg == -1) {
                    buffer_printf(compiler->text_buffer, "\txchg %%rax, (%%rsp)\n");
                    buffer_printf(compiler->text_buffer, "\tpop %%rax\n");
                    node->is_at_reg = false;
                } else {
                    buffer_printf(compiler->text_buffer, "\txchg %%%s, %%rax\n", compiler_regs[swap_reg]);
                    node->used_register = swap_reg;
                    node->is_at_reg = true;
                }
            }


        }
    } else if(node->token->type == COMPILER_TOKEN_TYPE_AND) {
        if(left_type != COMPILER_SYMBOL_TYPE_BOOLEAN || right_type != COMPILER_SYMBOL_TYPE_BOOLEAN) {
            node->computed_type = COMPILER_SYMBOL_TYPE_INTEGER;
            *result = left & right;
        } else {
            node->computed_type = COMPILER_SYMBOL_TYPE_BOOLEAN;
            node->computed_size = 1;
            *result = left && right;
        }

        buffer_printf(compiler->text_buffer, "# and left is 0x%llx right is 0x%llx\n", left, right);

        if(!node->is_const) {
            if(left_is_const) {
                buffer_printf(compiler->text_buffer, "\tand%c $0x%llx, %%%s\n", reg_suffix, left, right_reg);
                node->used_register = right_at_reg;
            } else if(right_is_const) {
                buffer_printf(compiler->text_buffer, "\tand%c $0x%llx, %%%s\n", reg_suffix, right, left_reg);
                node->used_register = left_at_reg;
            } else {
                buffer_printf(compiler->text_buffer, "\tand%c %%%s, %%%s\n", reg_suffix, left_reg, right_reg);
                node->used_register = right_at_reg;
                compiler->busy_regs[left_at_reg] = false;
            }

            if(node->computed_type == COMPILER_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tsetnz %%%s\n", compiler_cast_reg_to_size(compiler_regs[node->used_register], node->computed_size));
            }

            node->is_at_reg = true;
        }
        buffer_printf(compiler->text_buffer, "# and result is 0x%llx\n", *result);

    } else if(node->token->type == COMPILER_TOKEN_TYPE_OR) {
        if(left_type != COMPILER_SYMBOL_TYPE_BOOLEAN || right_type != COMPILER_SYMBOL_TYPE_BOOLEAN) {
            node->computed_type = COMPILER_SYMBOL_TYPE_INTEGER;
            *result = left | right;
        } else {
            node->computed_type = COMPILER_SYMBOL_TYPE_BOOLEAN;
            node->computed_size = 1;
            *result = left || right;
        }

        if(!node->is_const) {
            if(left_is_const) {
                buffer_printf(compiler->text_buffer, "\tor%c $0x%llx, %%%s\n", reg_suffix, left, right_reg);
                node->used_register = right_at_reg;
            } else if(right_is_const) {
                buffer_printf(compiler->text_buffer, "\tor%c $0x%llx, %%%s\n", reg_suffix, right, left_reg);
                node->used_register = left_at_reg;
            } else {
                buffer_printf(compiler->text_buffer, "\tor%c %%%s, %%%s\n", reg_suffix, left_reg, right_reg);
                node->used_register = right_at_reg;
                compiler->busy_regs[left_at_reg] = false;
            }

            if(node->computed_type == COMPILER_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tsetnz %%%s\n", compiler_cast_reg_to_size(compiler_regs[node->used_register], node->computed_size));
            }

            node->is_at_reg = true;
        }
    } else if(node->token->type == COMPILER_TOKEN_TYPE_XOR) {
        if(left_type != COMPILER_SYMBOL_TYPE_BOOLEAN || right_type != COMPILER_SYMBOL_TYPE_BOOLEAN) {
            node->computed_type = COMPILER_SYMBOL_TYPE_INTEGER;
            *result = left ^ right;
        } else {
            node->computed_type = COMPILER_SYMBOL_TYPE_BOOLEAN;
            node->computed_size = 1;
            *result = !left != !right;
        }

        if(!node->is_const) {
            if(left_is_const) {
                buffer_printf(compiler->text_buffer, "\txor%c $0x%llx, %%%s\n", reg_suffix, left, right_reg);
                node->used_register = right_at_reg;
            } else if(right_is_const) {
                buffer_printf(compiler->text_buffer, "\txor%c $0x%llx, %%%s\n", reg_suffix, right, left_reg);
                node->used_register = left_at_reg;
            } else {
                buffer_printf(compiler->text_buffer, "\txor%c %%%s, %%%s\n", reg_suffix, left_reg, right_reg);
                node->used_register = right_at_reg;
                compiler->busy_regs[left_at_reg] = false;
            }

            if(node->computed_type == COMPILER_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tsetnz %%%s\n", compiler_cast_reg_to_size(compiler_regs[node->used_register], node->computed_size));
            }

            node->is_at_reg = true;
        }
    } else if(node->token->type == COMPILER_TOKEN_TYPE_SHL) {
        *result = left << right;

        if(!node->is_const) {
            if(left_is_const) {
                left_at_reg = compiler_find_free_reg(compiler);

                if(left_at_reg == -1) {
                    PRINTLOG(COMPILER, LOG_ERROR, "no free registers");
                    return -1;
                }

                left_reg = compiler_cast_reg_to_size(compiler_regs[left_at_reg], max_size);

                buffer_printf(compiler->text_buffer, "\tmov%c $0x%llx, %%%s\n", reg_suffix, left, left_reg);
                buffer_printf(compiler->text_buffer, "\tshl%c $0x%llx, %%%s\n", reg_suffix, right, left_reg);
                node->used_register = left_at_reg;
            } else if(right_is_const) {
                buffer_printf(compiler->text_buffer, "\tshl%c $0x%llx, %%%s\n", reg_suffix, right, left_reg);
                node->used_register = left_at_reg;
            } else {
                buffer_printf(compiler->text_buffer, "\tshl%c %%%s, %%%s\n", reg_suffix, right_reg, left_reg);
                node->used_register = left_at_reg;
                compiler->busy_regs[right_at_reg] = false;
            }

            node->is_at_reg = true;
        }
    }  else if(node->token->type == COMPILER_TOKEN_TYPE_SHR) {
        *result = left >> right;

        if(!node->is_const) {
            if(left_is_const) {
                left_at_reg = compiler_find_free_reg(compiler);

                if(left_at_reg == -1) {
                    PRINTLOG(COMPILER, LOG_ERROR, "no free registers");
                    return -1;
                }

                left_reg = compiler_cast_reg_to_size(compiler_regs[left_at_reg], max_size);

                buffer_printf(compiler->text_buffer, "\tmov%c $0x%llx, %%%s\n", reg_suffix, left, left_reg);
                buffer_printf(compiler->text_buffer, "\tshr%c $0x%llx, %%%s\n", reg_suffix, right, left_reg);
                node->used_register = left_at_reg;
            } else if(right_is_const) {
                buffer_printf(compiler->text_buffer, "\tshr%c $0x%llx, %%%s\n", reg_suffix, right, left_reg);
                node->used_register = left_at_reg;
            } else {
                buffer_printf(compiler->text_buffer, "\tshr%c %%%s, %%%s\n", reg_suffix, right_reg, left_reg);
                node->used_register = left_at_reg;
                compiler->busy_regs[right_at_reg] = false;
            }

            node->is_at_reg = true;
        }
    }else {
        PRINTLOG(COMPILER, LOG_ERROR, "unknown binary op: %d", node->token->type);
        return -1;
    }

    buffer_printf(compiler->text_buffer, "# end binary op\n");

    return 0;
}


