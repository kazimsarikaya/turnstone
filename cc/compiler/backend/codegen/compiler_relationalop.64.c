/**
 * @file compiler_relationalop.64.c
 * @brief Implementation of the relational operators.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>
#include <utils.h>

MODULE("turnstone.compiler.codegen");


int8_t compiler_execute_relational_op(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    int64_t left = 0;
    int64_t right = 0;

    buffer_printf(compiler->text_buffer, "# begin relational op\n");

    if(compiler_execute_ast_node(compiler, node->left, &left) != 0) {
        return -1;
    }

    boolean_t left_is_const = node->left->is_const;
    int16_t left_at_reg = node->left->used_register;
    int64_t left_size = node->left->computed_size;

    if(compiler_execute_ast_node(compiler, node->right, &right) != 0) {
        return -1;
    }

    boolean_t right_is_const = node->right->is_const;
    int16_t right_at_reg = node->right->used_register;
    int64_t right_size = node->right->computed_size;

    if(left_is_const && right_is_const) {
        node->is_const = true;
    } else {
        node->is_const = false;
    }

    int64_t max_size = MAX(left_size, right_size);

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

    if(!node->is_const) {
        if(left_is_const) {
            buffer_printf(compiler->text_buffer, "\tcmp%c $0x%llx, %%%s\n", reg_suffix, left, right_reg);
            node->used_register = right_at_reg;
        } else if(right_is_const) {
            buffer_printf(compiler->text_buffer, "\tcmp%c $0x%llx, %%%s\n", reg_suffix, right, left_reg);
            node->used_register = left_at_reg;
        } else {
            buffer_printf(compiler->text_buffer, "\tcmp%c %%%s, %%%s\n", reg_suffix, left_reg, right_reg);
            node->used_register = right_at_reg;
            compiler->busy_regs[left_at_reg] = false;
        }

        node->is_at_reg = true;
    }

    node->computed_size = 1;
    node->computed_type = COMPILER_SYMBOL_TYPE_BOOLEAN;

    const char_t* used_reg = compiler_cast_reg_to_size(compiler_regs[node->used_register], node->computed_size);

    if (node->token->type == COMPILER_TOKEN_TYPE_EQUAL) {
        *result = left == right;

        if(!node->is_const) {
            buffer_printf(compiler->text_buffer, "\tseteb %%%s\n", used_reg);
        }

    } else if (node->token->type == COMPILER_TOKEN_TYPE_NOT_EQUAL) {
        *result = left != right;

        if(!node->is_const) {
            buffer_printf(compiler->text_buffer, "\tsetne %%%s\n", used_reg);
        }

    } else if (node->token->type == COMPILER_TOKEN_TYPE_LESS_THAN) {
        *result = left < right;

        if(!node->is_const) {
            buffer_printf(compiler->text_buffer, "\tsetl %%%s\n", used_reg);
        }

    } else if (node->token->type == COMPILER_TOKEN_TYPE_LESS_THAN_OR_EQUAL) {
        *result = left <= right;

        if(!node->is_const) {
            buffer_printf(compiler->text_buffer, "\tsetle %%%s\n", used_reg);
        }

    } else if (node->token->type == COMPILER_TOKEN_TYPE_GREATER_THAN) {
        *result = left > right;

        if(!node->is_const) {
            buffer_printf(compiler->text_buffer, "\tsetg %%%s\n", used_reg);
        }

    } else if (node->token->type == COMPILER_TOKEN_TYPE_GREATER_THAN_OR_EQUAL) {
        *result = left >= right;

        if(!node->is_const) {
            buffer_printf(compiler->text_buffer, "\tsetge %%%s\n", used_reg);
        }

    } else {
        PRINTLOG(COMPILER, LOG_ERROR, "unknown relational op");
        return -1;
    }

    if(node->is_const) {
        // if not zero then set to 1
        if(*result) {
            *result = 1;
        }
    }

    buffer_printf(compiler->text_buffer, "# end relational op\n");

    return 0;
}


