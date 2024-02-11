/**
 * @file compiler_if.64.c
 * @brief Implementation of the if statement.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>

MODULE("turnstone.compiler.codegen");


int8_t compiler_execute_if(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    int64_t condition = 0;

    compiler->cond_depth++;

    char_t* label = sprintf(".L%d", compiler->next_label_id++);

    linkedlist_stack_push(compiler->cond_label_stack, label);

    buffer_printf(compiler->text_buffer, "# begin if cond %lli\n", compiler->cond_depth);

    compiler->is_cond_eval = true;

    if(compiler_execute_ast_node(compiler, node->condition, &condition) != 0) {
        memory_free(label);

        return -1;
    }

    compiler->is_cond_eval = false;

    if(node->condition->is_const) {
        if(!condition) {
            buffer_printf(compiler->text_buffer, "\tjmp %s\n", label);
        }
    } else if(node->condition->is_at_reg) {
        const char_t* res_reg = compiler_cast_reg_to_size(compiler_regs[node->condition->used_register], node->condition->computed_size);

        if(node->condition->computed_type == COMPILER_SYMBOL_TYPE_BOOLEAN) {
            buffer_printf(compiler->text_buffer, "\ttest%c %%%s, %%%s\n", compiler_get_reg_suffix(node->condition->computed_size),
                          res_reg, res_reg);
            buffer_printf(compiler->text_buffer, "\tjz %s\n", label);
        } else {
            buffer_printf(compiler->text_buffer, "\tcmp $0, %%%s\n", res_reg);
            buffer_printf(compiler->text_buffer, "\tjz %s\n", label);
        }

        compiler->busy_regs[node->condition->used_register] = false;
        node->is_at_reg = false;
    } else {
        PRINTLOG(COMPILER, LOG_ERROR, "unsupported condition type\n");
        return -1;
    }

    buffer_printf(compiler->text_buffer, "# end if cond %lli\n", compiler->cond_depth);
    buffer_printf(compiler->text_buffer, "# begin if body %lli\n", compiler->cond_depth);

    if(compiler_execute_ast_node(compiler, node->left, result) != 0) {
        memory_free(label);

        return -1;
    }


    linkedlist_stack_pop(compiler->cond_label_stack);

    if(node->right) {
        char_t* label2 = sprintf(".L%d", compiler->next_label_id++);
        buffer_printf(compiler->text_buffer, "\tjmp %s\n", label2);
        buffer_printf(compiler->text_buffer, "# end if body %lli\n", compiler->cond_depth);

        buffer_printf(compiler->text_buffer, "%s:\n", label);

        buffer_printf(compiler->text_buffer, "# begin else body %lli\n", compiler->cond_depth);

        if(compiler_execute_ast_node(compiler, node->right, result) != 0) {
            return -1;
        }



        buffer_printf(compiler->text_buffer, "# end else body %lli\n", compiler->cond_depth);

        buffer_printf(compiler->text_buffer, "%s:\n", label2);

        memory_free(label2);
    } else {
        buffer_printf(compiler->text_buffer, "# end if body %lli\n", compiler->cond_depth);
        buffer_printf(compiler->text_buffer, "%s:\n", label);
    }

    memory_free(label);

    compiler->cond_depth--;

    return 0;
}


