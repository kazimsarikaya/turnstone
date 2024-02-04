/**
 * @file compiler_unaryop.64.c
 * @brief compiler unary op functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>

MODULE("turnstone.compiler.codegen");

int8_t compiler_execute_unary_op(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    int64_t right = 0;

    if(compiler_execute_ast_node(compiler, node->right, &right) != 0) {
        return -1;
    }

    if (node->token->type == COMPILER_TOKEN_TYPE_PLUS) {
        *result = +right;
    } else if (node->token->type == COMPILER_TOKEN_TYPE_MINUS) {
        buffer_printf(compiler->text_buffer, "# unary minus\n");

        *result = -right;

        if(compiler->is_at_reg) {
            buffer_printf(compiler->text_buffer, "\tneg%c %%%s\n",
                          compiler_get_reg_suffix(compiler->computed_size),
                          compiler_cast_reg_to_size(compiler_regs[node->right->used_register], compiler->computed_size));
            node->used_register = node->right->used_register;
        } else if(compiler->is_const) {
            *result = -*result;
        } else {
            PRINTLOG(COMPILER, LOG_ERROR, "unsupported");
            return -1;
        }

    } else if(node->token->type == COMPILER_TOKEN_TYPE_NOT) {
        *result = !right;

        if(compiler->is_at_reg) {
            buffer_printf(compiler->text_buffer, "\tnot%c %%%s\n",
                          compiler_get_reg_suffix(compiler->computed_size),
                          compiler_cast_reg_to_size(compiler_regs[node->right->used_register], compiler->computed_size));
            node->used_register = node->right->used_register;
        } else if(compiler->is_const) {
            *result = !*result;
        } else {
            PRINTLOG(COMPILER, LOG_ERROR, "unsupported");

            return -1;
        }
    } else {
        PRINTLOG(COMPILER, LOG_ERROR, "unknown unary op");
        return -1;
    }

    return 0;
}





