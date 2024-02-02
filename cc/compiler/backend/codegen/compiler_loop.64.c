/**
 * @file compiler_loop.64.c
 * @brief Implementation of the while and repeat statements.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>

MODULE("turnstone.compiler.codegen");

int8_t compiler_execute_while(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    int64_t condition = 0;

    compiler->loop_depth++;

    char_t* while_label = sprintf(".L%d", compiler->next_label_id++);
    char_t* end_label = sprintf(".L%d", compiler->next_label_id++);

    buffer_printf(compiler->text_buffer, "# begin while %lli\n", compiler->loop_depth);
    buffer_printf(compiler->text_buffer, "%s:\n", while_label);
    buffer_printf(compiler->text_buffer, "# begin while cond %lli\n", compiler->loop_depth);


    linkedlist_stack_push(compiler->cond_label_stack, end_label);

    compiler->is_cond_eval = true;

    if(compiler_execute_ast_node(compiler, node->condition, &condition) != 0) {
        memory_free(end_label);
        memory_free(while_label);

        return -1;
    }

    compiler->is_cond_eval = false;

    linkedlist_stack_push(compiler->cond_label_stack, end_label);

    if(node->condition->type != COMPILER_AST_NODE_TYPE_RELATIONAL_OP) {
        if(compiler->is_const) {
            if(!condition) {
                buffer_printf(compiler->text_buffer, "\tjmp %s\n", end_label);
            }
        } else if(compiler->is_at_reg) {
            if(compiler->computed_type == COMPILER_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tjz %s\n", end_label);
            } else {
                buffer_printf(compiler->text_buffer, "\tcmp $0, %%%s\n", compiler_regs[node->condition->used_register]);
                buffer_printf(compiler->text_buffer, "\tje %s\n", end_label);
            }

            compiler->busy_regs[node->condition->used_register] = false;
            compiler->is_at_reg = false;
        } else if(compiler->is_at_stack) {
            if(compiler->computed_type == COMPILER_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tjz %s\n", end_label);
            } else {
                buffer_printf(compiler->text_buffer, "\tcmp%c $0, -%lli(%%rbp)\n",
                              compiler_get_reg_suffix(compiler->computed_size),
                              compiler->at_stack_offset);
                buffer_printf(compiler->text_buffer, "\tje %s\n", end_label);
            }
        } else {
            PRINTLOG(COMPILER, LOG_ERROR, "need inspect");
            return -1;
        }
    }

    buffer_printf(compiler->text_buffer, "# end while cond %lli\n", compiler->cond_depth);
    buffer_printf(compiler->text_buffer, "# begin while body %lli\n", compiler->cond_depth);

    linkedlist_stack_push(compiler->loop_label_stack, end_label);

    if(compiler_execute_ast_node(compiler, node->left, result) != 0) {
        memory_free(end_label);
        memory_free(while_label);

        return -1;
    }

    linkedlist_stack_pop(compiler->loop_label_stack);

    buffer_printf(compiler->text_buffer, "\tjmp %s\n", while_label);
    buffer_printf(compiler->text_buffer, "# end while body %lli\n", compiler->loop_depth);
    buffer_printf(compiler->text_buffer, "%s:\n", end_label);

    memory_free(end_label);
    memory_free(while_label);

    compiler->loop_depth--;

    return 0;
}

int8_t compiler_execute_repeat(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    int64_t condition = 0;

    compiler->loop_depth++;

    char_t* repeat_label = sprintf(".L%d", compiler->next_label_id++);
    char_t* end_label = sprintf(".L%d", compiler->next_label_id++);

    buffer_printf(compiler->text_buffer, "# begin repeat %lli\n", compiler->loop_depth);
    buffer_printf(compiler->text_buffer, "%s:\n", repeat_label);


    buffer_printf(compiler->text_buffer, "# begin repeat body %lli\n", compiler->cond_depth);

    linkedlist_stack_push(compiler->loop_label_stack, end_label);

    if(compiler_execute_ast_node(compiler, node->left, result) != 0) {
        memory_free(end_label);
        memory_free(repeat_label);

        return -1;
    }

    linkedlist_stack_push(compiler->loop_label_stack, end_label);

    buffer_printf(compiler->text_buffer, "# end repeat body %lli\n", compiler->loop_depth);


    buffer_printf(compiler->text_buffer, "# begin repeat cond %lli\n", compiler->loop_depth);


    linkedlist_stack_push(compiler->cond_label_stack, end_label);

    compiler->is_cond_eval = true;
    compiler->is_cond_reverse = true;

    if(compiler_execute_ast_node(compiler, node->condition, &condition) != 0) {
        memory_free(end_label);
        memory_free(repeat_label);

        return -1;
    }

    compiler->is_cond_reverse = false;
    compiler->is_cond_eval = false;

    linkedlist_stack_pop(compiler->cond_label_stack);

    if(node->condition->type != COMPILER_AST_NODE_TYPE_RELATIONAL_OP) {
        if(compiler->is_const) {
            if(!condition) {
                buffer_printf(compiler->text_buffer, "\tjmp %s\n", end_label);
            }
        } else if(compiler->is_at_reg) {
            if(compiler->computed_type == COMPILER_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tjz %s\n", end_label);
            } else {
                buffer_printf(compiler->text_buffer, "\tcmp $0, %%%s\n", compiler_regs[node->condition->used_register]);
                buffer_printf(compiler->text_buffer, "\tje %s\n", end_label);
            }

            compiler->busy_regs[node->condition->used_register] = false;
            compiler->is_at_reg = false;
        } else if(compiler->is_at_stack) {
            if(compiler->computed_type == COMPILER_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tjz %s\n", end_label);
            } else {
                buffer_printf(compiler->text_buffer, "\tcmp%c $0, -%lli(%%rbp)\n",
                              compiler_get_reg_suffix(compiler->computed_size),
                              compiler->at_stack_offset);
                buffer_printf(compiler->text_buffer, "\tje %s\n", end_label);
            }
        } else {
            PRINTLOG(COMPILER, LOG_ERROR, "need inspect");
            return -1;
        }
    }

    buffer_printf(compiler->text_buffer, "# end repeat cond %lli\n", compiler->cond_depth);



    buffer_printf(compiler->text_buffer, "\tjmp %s\n", repeat_label);
    buffer_printf(compiler->text_buffer, "%s:\n", end_label);

    memory_free(end_label);
    memory_free(repeat_label);

    compiler->loop_depth--;

    return 0;
}


