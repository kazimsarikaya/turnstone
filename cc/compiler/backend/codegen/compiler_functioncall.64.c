/**
 * @file compiler_functioncall.64.c
 * @brief Implementation of the function call.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>
#include <utils.h>

MODULE("turnstone.compiler.codegen");

int8_t compiler_execute_function_call(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    buffer_printf(compiler->text_buffer, "# function call %s\n", node->token->text);

    int64_t stack_push_size = 0;
    boolean_t pushed_registers[7] = {false}; // rax, rdi, rsi, rdx, rcx, r8, r9
    int16_t pushed_reg_ids[] = {COMPILER_VM_REG_RAX, COMPILER_VM_REG_RDI, COMPILER_VM_REG_RSI, COMPILER_VM_REG_RDX, COMPILER_VM_REG_RCX, COMPILER_VM_REG_R8, COMPILER_VM_REG_R9};

    size_t children_size = linkedlist_size(node->children);

    if(children_size > 6) {
        PRINTLOG(COMPILER, LOG_ERROR, "function call %s expects 6 arguments", node->token->text);
        return -1;
    }

    buffer_printf(compiler->text_buffer, "# function call %s argument count %lli\n", node->token->text, children_size);

    if(compiler->busy_regs[pushed_reg_ids[0]]) {
        buffer_printf(compiler->text_buffer, "\tpush %%rax\n");
        pushed_registers[0] = true;
        stack_push_size += 8;
    }

    for(size_t i = 1; i <= children_size; i++) {
        compiler_ast_node_t* tmp_node = (compiler_ast_node_t*)linkedlist_get_data_at_position(node->children, i - 1);

        buffer_printf(compiler->text_buffer, "# function call %s argument %lli\n", node->token->text, i - 1);

        if(compiler_execute_ast_node(compiler, tmp_node, result) != 0) {
            PRINTLOG(COMPILER, LOG_ERROR, "cannot execute function call %s", node->token->text);
            return -1;
        }

        if(compiler->is_at_reg && tmp_node->used_register != pushed_reg_ids[i] && compiler->busy_regs[pushed_reg_ids[i]]) {
            buffer_printf(compiler->text_buffer, "\tpush %%%s\n", compiler_regs[pushed_reg_ids[i]]);
            pushed_registers[i] = true;
            stack_push_size += 8;
        }

        if(compiler->is_const) {
            buffer_printf(compiler->text_buffer, "\tmov $%lli, %%%s\n", *result, compiler_regs[pushed_reg_ids[i]]);
        } else if(compiler->is_at_reg) {
            if(tmp_node->used_register != pushed_reg_ids[i]) {
                if(compiler->computed_type == COMPILER_SYMBOL_TYPE_INTEGER && compiler->computed_size != 64) {
                    buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                                  compiler_cast_reg_to_size(compiler_regs[tmp_node->used_register], compiler->computed_size),
                                  compiler_regs[pushed_reg_ids[i]]);
                } else {
                    buffer_printf(compiler->text_buffer, "\tmov %%%s, %%%s\n",
                                  compiler_regs[tmp_node->used_register],
                                  compiler_regs[pushed_reg_ids[i]]);
                }

                compiler->busy_regs[node->used_register] = false;
            } else {
                if(compiler->computed_type == COMPILER_SYMBOL_TYPE_INTEGER && compiler->computed_size != 64) {
                    buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                                  compiler_cast_reg_to_size(compiler_regs[pushed_reg_ids[i]], compiler->computed_size),
                                  compiler_regs[pushed_reg_ids[i]]);
                }
            }

            compiler->busy_regs[pushed_reg_ids[i]] = true;
        } else {
            PRINTLOG(COMPILER, LOG_ERROR, "cannot execute function call %s", node->token->text);
            return -1;
        }
    }

    if(compiler->busy_regs[COMPILER_VM_REG_RBX]) {
        buffer_printf(compiler->text_buffer, "\tpush %%rbx\n");
        stack_push_size += 8;
    }

    buffer_printf(compiler->text_buffer, "\tmov $%s@GOT, %%rbx\n", node->token->text);
    buffer_printf(compiler->text_buffer, "\tmov (%%r15, %%rbx), %%rbx\n");

    if(stack_push_size % 16) {
        buffer_printf(compiler->text_buffer, "\tsub $8, %%rsp\n");
    }

    buffer_printf(compiler->text_buffer, "\txor %%rax, %%rax\n");
    buffer_printf(compiler->text_buffer, "\tcall *%%rbx\n");

    if(stack_push_size % 16) {
        buffer_printf(compiler->text_buffer, "\tadd $8, %%rsp\n");
    }

    if(compiler->busy_regs[COMPILER_VM_REG_RBX]) {
        buffer_printf(compiler->text_buffer, "\tpop %%rbx\n");
    }

    // restore registers reversed
    for(size_t i = children_size; i > 0; i--) {
        if(pushed_registers[i]) {
            buffer_printf(compiler->text_buffer, "\tpop %%%s\n", compiler_regs[pushed_reg_ids[i]]);
        }

        compiler->busy_regs[pushed_reg_ids[i]] = false;
    }

    int16_t reg_id = compiler_find_free_reg(compiler);

    if(reg_id < 0) {
        PRINTLOG(COMPILER, LOG_ERROR, "cannot find free register");
        return -1;
    }

    buffer_printf(compiler->text_buffer, "\tmov %%rax, %%%s\n", compiler_regs[reg_id]);

    compiler->is_const = false;
    compiler->is_at_reg = true;
    node->used_register = reg_id;

    if(pushed_registers[0]) {
        buffer_printf(compiler->text_buffer, "\tpop %%rax\n");
    }

    return 0;
}



