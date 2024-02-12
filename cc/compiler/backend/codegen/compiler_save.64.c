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
    buffer_printf(compiler->text_buffer, "# begin save\n");
    buffer_printf(compiler->text_buffer, "# begin get left\n");

    if(compiler_var_resolver(compiler, node->left, result) != 0) {
        PRINTLOG(COMPILER, LOG_ERROR, "cannot resolve var");
        return -1;
    }

    if(node->computed_is_scalar) {
        PRINTLOG(COMPILER, LOG_ERROR, "cannot save to scalar");
        return -1;
    }

    buffer_printf(compiler->text_buffer, "# end get left\n");

    buffer_printf(compiler->text_buffer, "# begin save\n");
    buffer_printf(compiler->text_buffer, "# begin get right\n");

    if(node->right->type != COMPILER_AST_NODE_TYPE_NO_OP) {

        if(compiler_execute_ast_node(compiler, node->right, result) != 0) {
            return -1;
        }

        buffer_printf(compiler->text_buffer, "# end get right\n");

        if(node->right->is_const) {
            buffer_printf(compiler->text_buffer, "\tmov%c $%lli, (%%%s)\n",
                          compiler_get_reg_suffix(node->left->computed_size),
                          *result,
                          compiler_regs[node->left->used_register]);

            return 0;
        } else {
            if(node->right->computed_size < node->left->computed_size) {
                buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                              compiler_cast_reg_to_size(compiler_regs[node->right->used_register], node->right->computed_size),
                              compiler_cast_reg_to_size(compiler_regs[node->right->used_register], node->left->computed_size));
            }

            buffer_printf(compiler->text_buffer, "\tmov%c %%%s, (%%%s)\n",
                          compiler_get_reg_suffix(node->left->computed_size),
                          compiler_cast_reg_to_size(compiler_regs[node->right->used_register], node->left->computed_size),
                          compiler_regs[node->left->used_register]);
            compiler->busy_regs[node->right->used_register] = false;
        }

    }

    compiler->busy_regs[node->left->used_register] = false;

    buffer_printf(compiler->text_buffer, "# end save\n");

    return 0;
}
