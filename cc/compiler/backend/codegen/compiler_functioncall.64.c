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

int8_t compiler_execute_function_call(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    buffer_printf(compiler->text_buffer, "# function call %s\n", node->token->text);

    if(strcmp(node->token->text, "write") == 0) {
        if(linkedlist_size(node->children) != 1) {
            PRINTLOG(COMPILER, LOG_ERROR, "write expects 1 argument");
            return -1;
        }

        uint64_t stack_push_size = 0;

        boolean_t rdi_pushed = false;
        boolean_t rsi_pushed = false;
        boolean_t rdx_pushed = false;
        boolean_t rcx_pushed = false;

        boolean_t stack_need_align = false;



        if(compiler->busy_regs[COMPILER_VM_REG_RCX]) {
            buffer_printf(compiler->text_buffer, "\tpush %%rcx\n");
            rcx_pushed = true;
            stack_push_size += 8;
        }

        if(compiler->busy_regs[COMPILER_VM_REG_RDI]) {
            buffer_printf(compiler->text_buffer, "\tpush %%rdi\n");
            rdi_pushed = true;
            stack_push_size += 8;
        }

        if(compiler->busy_regs[COMPILER_VM_REG_RSI]) {
            buffer_printf(compiler->text_buffer, "\tpush %%rsi\n");
            rsi_pushed = true;
            stack_push_size += 8;
        }

        if(compiler->busy_regs[COMPILER_VM_REG_RDX]) {
            buffer_printf(compiler->text_buffer, "\tpush %%rdx\n");
            rdx_pushed = true;
            stack_push_size += 8;
        }

        if(stack_push_size % 16) {
            stack_need_align = true;
        }

        buffer_printf(compiler->text_buffer, "\tmov $1, %%rdi\n");

        compiler_ast_node_t* tmp_node = (compiler_ast_node_t*)linkedlist_get_data_at_position(node->children, 0);

        if(compiler_execute_ast_node(compiler, tmp_node, result) != 0) {
            PRINTLOG(COMPILER, LOG_ERROR, "cannot execute write");
            return -1;
        }

        if(tmp_node->type == COMPILER_AST_NODE_TYPE_STRING_CONST) {
            buffer_printf(compiler->text_buffer, "\tmov $%s@GOT, %%rsi\n", tmp_node->symbol->name);
            buffer_printf(compiler->text_buffer, "\tmov (%%r15, %%rsi), %%rsi\n");

            buffer_printf(compiler->text_buffer, "\tmov $%lli, %%rdx\n", tmp_node->symbol->size);
        } else if(tmp_node->type == COMPILER_AST_NODE_TYPE_VAR) {
            const compiler_symbol_t * symbol = compiler_find_symbol(compiler, tmp_node->token->text);

            if(symbol == NULL) {
                PRINTLOG(COMPILER, LOG_ERROR, "symbol %s not found", tmp_node->token->text);
                return -1;
            }


            // TODO: check type -> integer probably pointer to string
            if(symbol->type == COMPILER_SYMBOL_TYPE_INTEGER) {
                if(symbol->is_local) {
                    if(symbol->is_array) {
                        buffer_printf(compiler->text_buffer, "\tlea -%d(%%rbp), %%rsi\n", symbol->stack_offset);
                    } else {
                        buffer_printf(compiler->text_buffer, "\tmov -%d(%%rbp), %%rsi\n", symbol->stack_offset);
                    }
                } else {
                    buffer_printf(compiler->text_buffer, "\tmov $%s@GOT, %%rsi\n", symbol->name);
                    buffer_printf(compiler->text_buffer, "\tmov (%%r15, %%rsi), %%rsi\n");
                    buffer_printf(compiler->text_buffer, "\tmov (%%rsi), %%rsi\n");
                }

                if(symbol->is_array) {
                    buffer_printf(compiler->text_buffer, "\tmov $%lli, %%rdx\n", symbol->array_size * symbol->size / 8);
                } else {
                    buffer_printf(compiler->text_buffer, "\tmov $1, %%rdx\n");
                }
            } else {
                PRINTLOG(COMPILER, LOG_ERROR, "cannot write symbol %s type %d", symbol->name, symbol->type);
                return -1;
            }
        } else {
            PRINTLOG(COMPILER, LOG_ERROR, "cannot write node type %d", tmp_node->type);
            return -1;
        }

        if(stack_need_align) {
            buffer_printf(compiler->text_buffer, "\tsub $8, %%rsp\n");
        }

        buffer_printf(compiler->text_buffer, "\tmov $0x%llx, %%rax\n", SYS_write);
        buffer_printf(compiler->text_buffer, "\tsyscall\n");

        if(stack_need_align) {
            buffer_printf(compiler->text_buffer, "\tadd $8, %%rsp\n");
        }

        if(rdx_pushed) {
            buffer_printf(compiler->text_buffer, "\tpop %%rdx\n");
        }

        if(rsi_pushed) {
            buffer_printf(compiler->text_buffer, "\tpop %%rsi\n");
        }

        if(rdi_pushed) {
            buffer_printf(compiler->text_buffer, "\tpop %%rdi\n");
        }

        if(rcx_pushed) {
            buffer_printf(compiler->text_buffer, "\tpop %%rcx\n");
        }

        compiler->is_at_reg = true;
        node->used_register = COMPILER_VM_REG_RAX;
        compiler->busy_regs[COMPILER_VM_REG_RAX] = true;


        return 0;
    }

    return -1;
}



