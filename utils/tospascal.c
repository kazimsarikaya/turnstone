/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAM_SIZE 0x1000000 // 16 MB
#include "setup.h"
#include <compiler/pascal.h>
#include <strings.h>
#include <logging.h>
#include <utils.h>
#include <hashmap.h>
#include <linkedlist.h>


int32_t main(int32_t argc, char * argv[]);

const char_t* pascal_vm_regs[] = {
    "rax",
    "rbx",
    "rcx",
    "rdx",
    "rsi",
    "rdi",
    "r8",
    "r9",
    "r10",
    "r11",
    "r12",
    "r13",
    "r14",
};

_Static_assert(sizeof(pascal_vm_regs) / sizeof(pascal_vm_regs[0]) == PASCAL_VM_REG_COUNT, "invalid register count");

int8_t                 pascal_vm_init(pascal_vm_t * vm, pascal_ast_t * ast);
int8_t                 pascal_vm_destroy(pascal_vm_t * vm);
int8_t                 pascal_vm_execute_ast_node(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result);
int8_t                 pascal_vm_execure_unary_op(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result);
int8_t                 pascal_vm_execure_binary_op(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result);
int8_t                 pascal_vm_execute_block(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result);
int8_t                 pascal_vm_execute_compound(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result);
int8_t                 pascal_vm_execute_assign(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result);
int8_t                 pascal_vm_execute(pascal_vm_t * vm, int32_t * result);
int8_t                 pascal_vm_print_symbol_table(pascal_vm_t * vm);
int8_t                 pascal_vm_destroy_symbol_table(pascal_vm_t * vm);
int8_t                 pascal_vm_build_stack(pascal_vm_t* vm);
int8_t                 pascal_vm_find_free_reg(pascal_vm_t* vm);
const pascal_symbol_t* pascal_vm_find_symbol(pascal_vm_t* vm, const char_t* name);
int8_t                 pascal_vm_save_to_mem(pascal_vm_t* vm, const char_t* reg, const char_t* mem);
int8_t                 pascal_vm_save_const_int_to_mem(pascal_vm_t* vm, int32_t value, const char_t* mem);

const pascal_symbol_t* pascal_vm_find_symbol(pascal_vm_t* vm, const char_t* name) {
    symbol_table_t* symbol_table = vm->current_symbol_table;

    while(symbol_table) {
        const pascal_symbol_t* symbol = hashmap_get(symbol_table->symbols, name);

        if(symbol != NULL) {
            return symbol;
        }

        symbol_table = symbol_table->parent;
    }

    return NULL;
}

int8_t pascal_vm_find_free_reg(pascal_vm_t* vm) {
    for(int32_t i = PASCAL_VM_REG_COUNT - 1; i >= 0; i--) {
        if(vm->busy_regs[i] == false) {
            vm->busy_regs[i] = true;
            return i;
        }
    }

    return -1;
}

int8_t pascal_vm_init(pascal_vm_t * vm, pascal_ast_t * ast) {
    if (ast == NULL) {
        return -1;
    }

    vm->text_buffer = buffer_new();

    if (vm->text_buffer == NULL) {
        return -1;
    }

    vm->data_buffer = buffer_new();

    if (vm->data_buffer == NULL) {
        buffer_destroy(vm->text_buffer);
        return -1;
    }

    vm->rodata_buffer = buffer_new();

    if (vm->rodata_buffer == NULL) {
        buffer_destroy(vm->text_buffer);
        buffer_destroy(vm->data_buffer);
        return -1;
    }

    vm->bss_buffer = buffer_new();

    if (vm->bss_buffer == NULL) {
        buffer_destroy(vm->text_buffer);
        buffer_destroy(vm->data_buffer);
        buffer_destroy(vm->rodata_buffer);
        return -1;
    }

    symbol_table_t* symbol_table = memory_malloc(sizeof(symbol_table_t));

    if (symbol_table == NULL) {
        buffer_destroy(vm->text_buffer);
        buffer_destroy(vm->data_buffer);
        buffer_destroy(vm->rodata_buffer);
        buffer_destroy(vm->bss_buffer);
        return -1;
    }

    symbol_table->symbols = hashmap_string(128);

    if (symbol_table->symbols == NULL) {
        buffer_destroy(vm->text_buffer);
        buffer_destroy(vm->data_buffer);
        buffer_destroy(vm->rodata_buffer);
        buffer_destroy(vm->bss_buffer);
        memory_free(symbol_table);
        return -1;
    }

    vm->main_symbol_table = symbol_table;
    vm->current_symbol_table = symbol_table;

    vm->ast = ast;

    vm->next_stack_offset = 16;
    vm->stack_size = 8;

    return 0;
}

int8_t pascal_vm_print_symbol_table(pascal_vm_t * vm) {
    symbol_table_t* symbol_table = vm->current_symbol_table;

    while(symbol_table) {
        iterator_t* iter = hashmap_iterator_create(symbol_table->symbols);

        if (iter == NULL) {
            return -1;
        }

        while (iter->end_of_iterator(iter) != 0) {
            const pascal_symbol_t* symbol = iter->get_item(iter);

            if (symbol == NULL) {
                iter->destroy(iter);
                return -1;
            }

            PRINTLOG(COMPILER_PASCAL, LOG_INFO, "symbol %s type %d size %d value %lli", symbol->name, symbol->type, symbol->size, symbol->int_value);

            iter->next(iter);
        }

        iter->destroy(iter);

        symbol_table = symbol_table->parent;
    }

    return 0;
}

int8_t pascal_vm_destroy_symbol_table(pascal_vm_t * vm) {
    symbol_table_t* symbol_table = vm->current_symbol_table;

    while(symbol_table) {
        hashmap_destroy(symbol_table->symbols);

        symbol_table_t* parent = symbol_table->parent;

        memory_free(symbol_table);

        symbol_table = parent;
    }

    return 0;
}

int8_t pascal_vm_destroy(pascal_vm_t * vm) {
    if (vm->text_buffer != NULL) {
        buffer_destroy(vm->text_buffer);
    }

    if (vm->data_buffer != NULL) {
        buffer_destroy(vm->data_buffer);
    }

    if (vm->rodata_buffer != NULL) {
        buffer_destroy(vm->rodata_buffer);
    }

    if (vm->bss_buffer != NULL) {
        buffer_destroy(vm->bss_buffer);
    }

    pascal_vm_destroy_symbol_table(vm);

    memory_free(vm->program_name_symbol);

    pascal_ast_destroy(vm->ast);
    return 0;
}

int8_t pascal_vm_build_stack(pascal_vm_t* vm) {
    if (vm->current_symbol_table == NULL) {
        return -1;
    }

    iterator_t* iter = hashmap_iterator_create(vm->current_symbol_table->symbols);

    if (iter == NULL) {
        return -1;
    }

    while (iter->end_of_iterator(iter) != 0) {
        const pascal_symbol_t* symbol = iter->get_item(iter);

        if (symbol == NULL) {
            iter->destroy(iter);
            return -1;
        }

        if(symbol->is_local &&  symbol->type == PASCAL_SYMBOL_TYPE_INTEGER) {
            buffer_printf(vm->text_buffer, "\tmovq $%lli, -%d(%%rbp)\n", symbol->int_value, symbol->stack_offset);
        }

        iter->next(iter);
    }

    iter->destroy(iter);

    return 0;
}


int8_t pascal_vm_execute_block(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result) {
    if(node->left) {
        if(node->left->type == PASCAL_AST_NODE_TYPE_DECLS) {

            for(size_t i = 0; i < linkedlist_size(node->left->children); i++) {
                pascal_ast_node_t * tmp_node = (pascal_ast_node_t*)linkedlist_get_data_at_position(node->left->children, i);

                for(size_t j = 0; j < linkedlist_size(tmp_node->children); j++) {
                    pascal_symbol_t * tmp_symbol = (pascal_symbol_t*)linkedlist_get_data_at_position(tmp_node->children, j);

                    if(strcmp(vm->program_name, tmp_symbol->name) != 0 && hashmap_get(vm->current_symbol_table->symbols, tmp_symbol->name) != NULL) {
                        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "symbol %s already defined", tmp_symbol->name);
                        return -1;
                    }

                    if(tmp_symbol->type == PASCAL_SYMBOL_TYPE_INTEGER) {

                        if(tmp_symbol->is_local){
                            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "local symbol %s type %d size %d value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);
                            // tmp_symbol->stack_offset = vm->next_stack_offset;
                            // vm->next_stack_offset += tmp_symbol->size;
                            // vm->stack_size += tmp_symbol->size;
                        } else {
                            tmp_symbol->stack_offset = 0;

                            if(tmp_symbol->is_const){
                                PRINTLOG(COMPILER_PASCAL, LOG_INFO, "rodata symbol %s type %d size %d value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);
                                buffer_printf(vm->rodata_buffer, ".section .rodata.%s\n", tmp_symbol->name);
                                buffer_printf(vm->rodata_buffer, ".align 8\n");
                                buffer_printf(vm->rodata_buffer, ".local %s\n", tmp_symbol->name);
                                buffer_printf(vm->rodata_buffer, ".type %s, @object\n", tmp_symbol->name);
                                buffer_printf(vm->rodata_buffer, ".size %s, %d\n", tmp_symbol->name, tmp_symbol->size);
                                buffer_printf(vm->rodata_buffer, "%s:\n", tmp_symbol->name);
                                buffer_printf(vm->rodata_buffer, "\t.long %lli\n", tmp_symbol->int_value);
                                // buffer_printf(vm->rodata_buffer, ".size %s, .-%s\n", tmp_symbol->name, tmp_symbol->name);
                                buffer_printf(vm->rodata_buffer, "\n\n\n");
                            } else {
                                if(tmp_symbol->int_value == 0) {
                                    PRINTLOG(COMPILER_PASCAL, LOG_INFO, "bss symbol %s type %d size %d value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);
                                    buffer_printf(vm->bss_buffer, ".section .bss.%s\n", tmp_symbol->name);
                                    buffer_printf(vm->bss_buffer, ".align 8\n");
                                    buffer_printf(vm->bss_buffer, ".local %s\n", tmp_symbol->name);
                                    buffer_printf(vm->bss_buffer, ".type %s, @object\n", tmp_symbol->name);
                                    buffer_printf(vm->bss_buffer, ".size %s, %d\n", tmp_symbol->name, tmp_symbol->size);
                                    buffer_printf(vm->bss_buffer, "%s:\n", tmp_symbol->name);
                                    buffer_printf(vm->bss_buffer, "\t.zero %d\n", tmp_symbol->size);
                                    // buffer_printf(vm->bss_buffer, ".size %s, .-%s\n", tmp_symbol->name, tmp_symbol->name);
                                    buffer_printf(vm->bss_buffer, "\n\n\n");
                                } else {
                                    PRINTLOG(COMPILER_PASCAL, LOG_INFO, "data symbol %s type %d size %d value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);
                                    buffer_printf(vm->data_buffer, ".section .data.%s\n", tmp_symbol->name);
                                    buffer_printf(vm->data_buffer, ".align 8\n");
                                    buffer_printf(vm->data_buffer, ".local %s\n", tmp_symbol->name);
                                    buffer_printf(vm->data_buffer, ".type %s, @object\n", tmp_symbol->name);
                                    buffer_printf(vm->data_buffer, ".size %s, %d\n", tmp_symbol->name, tmp_symbol->size);
                                    buffer_printf(vm->data_buffer, "%s:\n", tmp_symbol->name);
                                    buffer_printf(vm->data_buffer, "\t.long %lli\n", tmp_symbol->int_value);
                                    // buffer_printf(vm->data_buffer, ".size %s, .-%s\n", tmp_symbol->name, tmp_symbol->name);
                                    buffer_printf(vm->data_buffer, "\n\n\n");
                                }
                            }
                        }
                    }

                    hashmap_put(vm->current_symbol_table->symbols, tmp_symbol->name, tmp_symbol);
                }
            }
        } else {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected decls");
            return -1;
        }
    } else {
        PRINTLOG(COMPILER_PASCAL, LOG_INFO, "no decls");
    }

    return pascal_vm_execute_ast_node(vm, node->right, result);
}

int8_t pascal_vm_execure_unary_op(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result) {
    int32_t right = 0;

    if(pascal_vm_execute_ast_node(vm, node->right, &right) != 0) {
        return -1;
    }

    if (node->token->type == PASCAL_TOKEN_TYPE_PLUS) {
        *result = +right;
    } else if (node->token->type == PASCAL_TOKEN_TYPE_MINUS) {
        buffer_printf(vm->text_buffer, "# unary minus\n");

        *result = -right;

        if(vm->is_at_mem) {
            buffer_printf(vm->text_buffer, "\tneg %s\n", node->right->token->text);
        } else if(vm->is_at_reg) {
            buffer_printf(vm->text_buffer, "\tneg %%%s\n", pascal_vm_regs[node->right->used_register]);
            node->used_register = node->right->used_register;
        } else if(vm->is_const) {
            *result = -*result;
        } else if(vm->is_at_stack) {
            int16_t reg = pascal_vm_find_free_reg(vm);
            buffer_printf(vm->text_buffer, "\tmov -%d(%%rbp), %%%s\n", vm->at_stack_offset, pascal_vm_regs[reg]);
            buffer_printf(vm->text_buffer, "\tneg %%%s\n", pascal_vm_regs[reg]);
            vm->is_at_stack = false;
            vm->is_at_reg = true;
            node->used_register = reg;
        } else {
            int16_t reg = pascal_vm_find_free_reg(vm);
            buffer_printf(vm->text_buffer, "\tpop %%%s\n", pascal_vm_regs[reg]);
            buffer_printf(vm->text_buffer, "\tneg %%%s\n", pascal_vm_regs[reg]);
            vm->is_at_reg = true;
            node->used_register = reg;
        }

    } else {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unknown unary op");
        return -1;
    }

    return 0;
}

int8_t pascal_vm_execure_binary_op(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result) {
    int32_t left = 0;
    int32_t right = 0;

    if(pascal_vm_execute_ast_node(vm, node->left, &left) != 0) {
        return -1;
    }

    boolean_t left_is_const = vm->is_const;
    vm->is_const = false;
    boolean_t left_is_at_reg = vm->is_at_reg;
    int16_t left_at_reg = node->left->used_register;
    vm->is_at_reg = false;
    boolean_t left_is_at_stack = vm->is_at_stack;
    vm->is_at_stack = false;
    uint16_t left_at_stack_offset = vm->at_stack_offset;


    if(!left_is_const) {
        if(!left_is_at_reg) {
            left_at_reg = pascal_vm_find_free_reg(vm);

            if(left_at_reg == -1) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "no free registers");
                return -1;
            }

            if(left_is_at_stack) {
                buffer_printf(vm->text_buffer, "\tmov -%d(%%rbp), %%%s\n", left_at_stack_offset, pascal_vm_regs[left_at_reg]);
            } else {
                buffer_printf(vm->text_buffer, "\tpop %%%s\n", pascal_vm_regs[left_at_reg]);
            }
        }
    }

    if(pascal_vm_execute_ast_node(vm, node->right, &right) != 0) {
        return -1;
    }

    boolean_t right_is_const = vm->is_const;
    vm->is_const = false;
    boolean_t right_is_at_reg = vm->is_at_reg;
    vm->is_at_reg = false;
    int16_t right_at_reg = node->right->used_register;
    boolean_t right_is_at_stack = vm->is_at_stack;
    vm->is_at_stack = false;
    uint16_t right_at_stack_offset = vm->at_stack_offset;

    if(!right_is_const) {
        if(!right_is_at_reg) {
            right_at_reg = pascal_vm_find_free_reg(vm);

            if(right_at_reg == -1) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "no free registers");
                return -1;
            }

            if(right_is_at_stack) {
                buffer_printf(vm->text_buffer, "\tmov -%d(%%rbp), %%%s\n", right_at_stack_offset, pascal_vm_regs[right_at_reg]);
            } else {
                buffer_printf(vm->text_buffer, "\tpop %%%s\n", pascal_vm_regs[right_at_reg]);
            }
        }
    }

    if(left_is_const && right_is_const) {
        vm->is_const = true;
    }

    if (node->token->type == PASCAL_TOKEN_TYPE_PLUS) {
        *result = left + right;
        if(!vm->is_const) {
            if(left_is_const) {
                buffer_printf(vm->text_buffer, "\tadd $%d, %%%s\n", left, pascal_vm_regs[right_at_reg]);
                node->used_register = right_at_reg;
            } else if(right_is_const) {
                buffer_printf(vm->text_buffer, "\tadd $%d, %%%s\n", right, pascal_vm_regs[left_at_reg]);
                node->used_register = left_at_reg;
            } else {
                buffer_printf(vm->text_buffer, "\tadd %%%s, %%%s\n", pascal_vm_regs[left_at_reg], pascal_vm_regs[right_at_reg]);
                node->used_register = right_at_reg;
                vm->busy_regs[left_at_reg] = false;
            }
            vm->is_at_reg = true;
        }
    } else if (node->token->type == PASCAL_TOKEN_TYPE_MINUS) {
        *result = left - right;

        if(!vm->is_const) {
            if(left_is_const) {
                buffer_printf(vm->text_buffer, "\tsub $%d, %%%s\n", left, pascal_vm_regs[right_at_reg]);
                node->used_register = right_at_reg;
            } else if(right_is_const) {
                buffer_printf(vm->text_buffer, "\tsub $%d, %%%s\n", right, pascal_vm_regs[left_at_reg]);
                node->used_register = left_at_reg;
            } else {
                buffer_printf(vm->text_buffer, "\tsub %%%s, %%%s\n", pascal_vm_regs[right_at_reg], pascal_vm_regs[left_at_reg]);
                node->used_register = left_at_reg;
                vm->busy_regs[right_at_reg] = false;
            }
            vm->is_at_reg = true;
        }
    } else if (node->token->type == PASCAL_TOKEN_TYPE_MULTIPLY) {
        *result = left * right;

        if(!vm->is_const) {
            if(left_is_const) {
                buffer_printf(vm->text_buffer, "\timul $%d, %%%s\n", left, pascal_vm_regs[right_at_reg]);
                node->used_register = right_at_reg;
            } else if(right_is_const) {
                buffer_printf(vm->text_buffer, "\timul $%d, %%%s\n", right, pascal_vm_regs[left_at_reg]);
                node->used_register = left_at_reg;
            } else {
                buffer_printf(vm->text_buffer, "\timul %%%s, %%%s\n", pascal_vm_regs[left_at_reg], pascal_vm_regs[right_at_reg]);
                node->used_register = right_at_reg;
                vm->busy_regs[left_at_reg] = false;
            }
            vm->is_at_reg = true;
        }
    } else if (node->token->type == PASCAL_TOKEN_TYPE_INTEGER_DIVIDE) {
        *result = left / right;

        if(!vm->is_const) {
            boolean_t need_swap = false;
            int16_t swap_reg = -1;

            if(left_at_reg != 0) {
                if(vm->busy_regs[0]) {
                    need_swap = true;
                    swap_reg = pascal_vm_find_free_reg(vm);

                    if(swap_reg == -1) {
                        buffer_printf(vm->text_buffer, "\tpush %%rax\n");
                    } else {
                        buffer_printf(vm->text_buffer, "\tmov %%rax, %%%s\n", pascal_vm_regs[swap_reg]);
                    }
                }
            }

            if(left_is_const) {
                buffer_printf(vm->text_buffer, "\tmov $%d, %%rax\n", left);
                buffer_printf(vm->text_buffer, "\tcqo\n");
                buffer_printf(vm->text_buffer, "\tidiv %%%s\n", pascal_vm_regs[right_at_reg]);
                node->used_register = 0;
            } else if(right_is_const) {
                boolean_t old_rdx_busy = vm->busy_regs[3];
                vm->busy_regs[3] = true;
                int16_t const_swap_reg = pascal_vm_find_free_reg(vm);
                vm->busy_regs[3] = old_rdx_busy;

                if(const_swap_reg == -1) {
                    const_swap_reg = 8;
                    buffer_printf(vm->text_buffer, "\tpush %%%s\n", pascal_vm_regs[right_at_reg]);
                }

                buffer_printf(vm->text_buffer, "\tmov $%d, %%%s\n", right, pascal_vm_regs[const_swap_reg]);


                buffer_printf(vm->text_buffer, "\tmov %%%s, %%rax\n", pascal_vm_regs[left_at_reg]);
                buffer_printf(vm->text_buffer, "\tcqo\n");
                buffer_printf(vm->text_buffer, "\tidiv %%%s\n", pascal_vm_regs[const_swap_reg]);
                vm->busy_regs[left_at_reg] = false;
                node->used_register = 0;

                if(const_swap_reg == -1) {
                    buffer_printf(vm->text_buffer, "\tpop %%%s\n", pascal_vm_regs[const_swap_reg]);
                } else {
                    vm->busy_regs[const_swap_reg] = false;
                }
            } else {
                buffer_printf(vm->text_buffer, "\tmov %%%s, %%rax\n", pascal_vm_regs[left_at_reg]);
                buffer_printf(vm->text_buffer, "\tcqo\n");
                buffer_printf(vm->text_buffer, "\tidiv %%%s\n", pascal_vm_regs[right_at_reg]);
                node->used_register = 0;
                vm->busy_regs[left_at_reg] = false;
                vm->busy_regs[right_at_reg] = false;
            }

            vm->busy_regs[0] = true;
            vm->is_at_reg = true;

            if(need_swap) {
                if(swap_reg == -1) {
                    buffer_printf(vm->text_buffer, "\txchg %%rax, (%%rsp)\n");
                    buffer_printf(vm->text_buffer, "\tpop %%rax\n");
                    vm->is_at_reg = false;
                } else {
                    buffer_printf(vm->text_buffer, "\txchg %%%s, %%rax\n", pascal_vm_regs[swap_reg]);
                    node->used_register = swap_reg;
                    vm->is_at_reg = true;
                }
            }


        }
    } else {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unknown binary op: %d", node->token->type);
        return -1;
    }

    return 0;
}

int8_t pascal_vm_execute_compound(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result) {
    if(node->children == NULL) {
        return 0;
    }

    symbol_table_t* symbol_table = memory_malloc(sizeof(symbol_table_t));

    if (symbol_table == NULL) {
        return -1;
    }

    symbol_table->symbols = hashmap_string(128);

    if (symbol_table->symbols == NULL) {
        memory_free(symbol_table);
        return -1;
    }

    symbol_table->parent = vm->current_symbol_table;
    vm->current_symbol_table = symbol_table;

    for(size_t i = 0; i < linkedlist_size(node->children); i++) {
        pascal_ast_node_t * tmp_node = (pascal_ast_node_t*)linkedlist_get_data_at_position(node->children, i);

        if(tmp_node->type == PASCAL_AST_NODE_TYPE_DECLS) {
            for(size_t j = 0; j < linkedlist_size(tmp_node->children); j++) {
                pascal_ast_node_t * tmp_var_node = (pascal_ast_node_t*)linkedlist_get_data_at_position(tmp_node->children, j);

                for(size_t k = 0; k < linkedlist_size(tmp_var_node->children); k++) {
                    pascal_symbol_t * tmp_symbol = (pascal_symbol_t*)linkedlist_get_data_at_position(tmp_var_node->children, k);

                    if(strcmp(vm->program_name, tmp_symbol->name) != 0 && hashmap_get(vm->current_symbol_table->symbols, tmp_symbol->name) != NULL) {
                        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "symbol %s already defined", tmp_symbol->name);
                        return -1;
                    }

                    if(tmp_symbol->type == PASCAL_SYMBOL_TYPE_INTEGER) {
                        PRINTLOG(COMPILER_PASCAL, LOG_INFO, "local symbol %s type %d size %d value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);

                        tmp_symbol->stack_offset = vm->next_stack_offset;
                        vm->next_stack_offset += 8; // tmp_symbol->size;
                        vm->stack_size += 8; // tmp_symbol->size;
                    }

                    hashmap_put(vm->current_symbol_table->symbols, tmp_symbol->name, tmp_symbol);
                }

            }
        }
    }

    if(pascal_vm_build_stack(vm) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot build stack");
        return -1;
    }

    for(size_t i = 0; i < linkedlist_size(node->children); i++) {
        pascal_ast_node_t * tmp_node = (pascal_ast_node_t*)linkedlist_get_data_at_position(node->children, i);

        if(tmp_node->type != PASCAL_AST_NODE_TYPE_DECLS && pascal_vm_execute_ast_node(vm, tmp_node, result) != 0) {
            return -1;
        }
    }

    vm->current_symbol_table = vm->current_symbol_table->parent;

    hashmap_destroy(symbol_table->symbols);
    memory_free(symbol_table);

    return 0;
}

int8_t pascal_vm_save_to_mem(pascal_vm_t* vm, const char_t* reg, const char_t* mem) {
    int16_t swap_reg = pascal_vm_find_free_reg(vm);
    boolean_t need_swap = false;

    if(swap_reg == -1) {
        need_swap = true;
        swap_reg = 8;
        buffer_printf(vm->text_buffer, "\tpush %%%s\n", pascal_vm_regs[swap_reg]);
    }

    buffer_printf(vm->text_buffer, "\tmov $%s@GOT, %%%s\n", mem, pascal_vm_regs[swap_reg]);
    buffer_printf(vm->text_buffer, "\tmov (%%r15, %%%s), %%%s\n", pascal_vm_regs[swap_reg], pascal_vm_regs[swap_reg]);
    buffer_printf(vm->text_buffer, "\tmov %%%s, (%%%s)\n", reg, pascal_vm_regs[swap_reg]);

    if(need_swap) {
        buffer_printf(vm->text_buffer, "\tpop %%%s\n", pascal_vm_regs[swap_reg]);
    } else {
        vm->busy_regs[swap_reg] = false;
    }

    return 0;
}

int8_t pascal_vm_save_const_int_to_mem(pascal_vm_t* vm, int32_t value, const char_t* mem) {
    int16_t swap_reg = pascal_vm_find_free_reg(vm);
    boolean_t need_swap = false;

    if(swap_reg == -1) {
        need_swap = true;
        swap_reg = 8;
        buffer_printf(vm->text_buffer, "\tpush %%%s\n", pascal_vm_regs[swap_reg]);
    }

    buffer_printf(vm->text_buffer, "\tmov $%s@GOT, %%%s\n", mem, pascal_vm_regs[swap_reg]);
    buffer_printf(vm->text_buffer, "\tmov (%%r15, %%%s), %%%s\n", pascal_vm_regs[swap_reg], pascal_vm_regs[swap_reg]);
    buffer_printf(vm->text_buffer, "\tmov $%d, (%%%s)\n", value, pascal_vm_regs[swap_reg]);

    if(need_swap) {
        buffer_printf(vm->text_buffer, "\tpop %%%s\n", pascal_vm_regs[swap_reg]);
    } else {
        vm->busy_regs[swap_reg] = false;
    }

    return 0;
}

int8_t pascal_vm_execute_assign(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result) {
    const pascal_symbol_t * symbol = pascal_vm_find_symbol(vm, node->left->token->text);

    if(symbol == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "symbol %s not found", node->left->token->text);
        return -1;
    }

    buffer_printf(vm->text_buffer, "# assign %s\n", symbol->name);

    if(pascal_vm_execute_ast_node(vm, node->right, result) != 0) {
        return -1;
    }

    ((pascal_symbol_t*)symbol)->int_value = *result;

    if(vm->is_at_mem) {
        if(symbol->is_local) {
            buffer_printf(vm->text_buffer, "\tmov %s, -%d(%%rbp)\n", node->right->token->text, symbol->stack_offset);
        } else {
            int16_t swap_reg = pascal_vm_find_free_reg(vm);
            boolean_t need_swap = false;

            if(swap_reg == -1) {
                need_swap = true;
                swap_reg = 8;
                buffer_printf(vm->text_buffer, "\tpush %%%s\n", pascal_vm_regs[swap_reg]);
            }

            buffer_printf(vm->text_buffer, "\tmov %s, %%%s\n", node->right->token->text, pascal_vm_regs[swap_reg]);

            pascal_vm_save_to_mem(vm, pascal_vm_regs[swap_reg], symbol->name);

            if(need_swap) {
                buffer_printf(vm->text_buffer, "\tpop %%%s\n", pascal_vm_regs[swap_reg]);
            } else {
                vm->busy_regs[swap_reg] = false;
            }
        }

    } else if(vm->is_at_reg) {
        if(symbol->is_local) {
            buffer_printf(vm->text_buffer, "\tmov %%%s, -%d(%%rbp)\n", pascal_vm_regs[node->right->used_register], symbol->stack_offset);
        } else {
            pascal_vm_save_to_mem(vm, pascal_vm_regs[node->right->used_register], symbol->name);
        }

        vm->is_at_reg = false;
        vm->busy_regs[node->right->used_register] = false;
    } else if(vm->is_const) {
        if(symbol->is_local) {
            buffer_printf(vm->text_buffer, "\tmov $%d, -%d(%%rbp)\n", *result, symbol->stack_offset);
        } else {
            pascal_vm_save_const_int_to_mem(vm, *result, symbol->name);
        }

        vm->is_const = false;
    } else if(vm->is_at_stack) {
        int16_t swap_reg = pascal_vm_find_free_reg(vm);
        boolean_t need_swap = false;

        if(swap_reg == -1) {
            need_swap = true;
            swap_reg = 8;
            buffer_printf(vm->text_buffer, "\tpush %%%s\n", pascal_vm_regs[swap_reg]);
        }

        buffer_printf(vm->text_buffer, "\tmov -%d(%%rbp), %%%s\n", vm->at_stack_offset, pascal_vm_regs[swap_reg]);

        if(symbol->is_local) {
            buffer_printf(vm->text_buffer, "\tmov %%%s, -%d(%%rbp)\n", pascal_vm_regs[swap_reg], symbol->stack_offset);
        } else {
            pascal_vm_save_to_mem(vm, pascal_vm_regs[swap_reg], symbol->name);
        }

        if(need_swap) {
            buffer_printf(vm->text_buffer, "\tpop %%%s\n", pascal_vm_regs[swap_reg]);
        } else {
            vm->busy_regs[swap_reg] = false;
        }


        vm->is_at_stack = false;
    } else {
        int16_t swap_reg = pascal_vm_find_free_reg(vm);
        boolean_t need_swap = false;

        if(swap_reg == -1) {
            need_swap = true;
            swap_reg = 8;
            buffer_printf(vm->text_buffer, "\tpush %%%s\n", pascal_vm_regs[swap_reg]);
        }

        buffer_printf(vm->text_buffer, "\tpop %%%s\n", pascal_vm_regs[swap_reg]);

        if(symbol->is_local) {
            buffer_printf(vm->text_buffer, "\tmov %%%s, -%d(%%rbp)\n", pascal_vm_regs[swap_reg], symbol->stack_offset);
        } else {
            pascal_vm_save_to_mem(vm, pascal_vm_regs[swap_reg], symbol->name);
        }

        if(need_swap) {
            buffer_printf(vm->text_buffer, "\tpop %%%s\n", pascal_vm_regs[swap_reg]);
        } else {
            vm->busy_regs[swap_reg] = false;
        }

    }

    return 0;
}

#define SYS_exit 60UL

int8_t pascal_vm_execute_ast_node(pascal_vm_t* vm, pascal_ast_node_t* node, int32_t* result) {
    if (node == NULL) {
        return -1;
    }

    if(node->type == PASCAL_AST_NODE_TYPE_NO_OP) {
        return 0;
    } else if(node->type == PASCAL_AST_NODE_TYPE_PROGRAM) {
        pascal_symbol_t * symbol = memory_malloc(sizeof(pascal_symbol_t));

        if (symbol == NULL) {
            return -1;
        }

        symbol->name = node->token->text;
        symbol->type = PASCAL_SYMBOL_TYPE_INTEGER;
        symbol->size = 8;
        symbol->int_value = 0;
        symbol->stack_offset = 8;
        symbol->is_local = true;

        hashmap_put(vm->current_symbol_table->symbols, symbol->name, symbol);

        vm->program_name = symbol->name;
        vm->program_name_symbol = symbol;

        buffer_printf(vm->text_buffer, "\tmov $%s@GOT, %%rax\n", symbol->name);
        buffer_printf(vm->text_buffer, "\tcall *(%%r15,%%rax)\n");
        buffer_printf(vm->text_buffer, "\tmov %%rax, %%rdi\n");
        buffer_printf(vm->text_buffer, "\tmov $0x%lx, %%rax\n", SYS_exit);
        buffer_printf(vm->text_buffer, "\tsyscall\n");
        buffer_printf(vm->text_buffer, ".size _start, .-_start\n");

        buffer_printf(vm->text_buffer, "\n\n\n");
        buffer_printf(vm->text_buffer, ".section .text.%s\n", symbol->name);
        buffer_printf(vm->text_buffer, ".local %s\n", symbol->name);
        buffer_printf(vm->text_buffer, ".type %s, @function\n", symbol->name);
        buffer_printf(vm->text_buffer, "%s:\n", symbol->name);

        buffer_t* tmp_buffer = vm->text_buffer;

        vm->text_buffer = buffer_new();

        if(pascal_vm_execute_ast_node(vm, node->left, result) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot execute program %s", symbol->name);
            return -1;
        }

        buffer_printf(tmp_buffer, "\tenter $%d, $0\n", vm->stack_size);

        buffer_printf(tmp_buffer, "%s", buffer_get_view_at_position(vm->text_buffer, 0, buffer_get_length(vm->text_buffer)));

        buffer_destroy(vm->text_buffer);

        vm->text_buffer = tmp_buffer;

        buffer_printf(vm->text_buffer, "\tmov -8(%%rbp), %%rax\n");
        buffer_printf(vm->text_buffer, "\tleave\n");
        buffer_printf(vm->text_buffer, "\tret\n");
        buffer_printf(vm->text_buffer, ".size %s, .-%s\n", symbol->name, symbol->name);
        buffer_printf(vm->text_buffer, "\n\n\n");


        return 0;
    } else if(node->type == PASCAL_AST_NODE_TYPE_BLOCK) {
        return pascal_vm_execute_block(vm, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_COMPOUND) {
        return pascal_vm_execute_compound(vm, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_ASSIGN) {
        return pascal_vm_execute_assign(vm, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_VAR) {
        const pascal_symbol_t * symbol = pascal_vm_find_symbol(vm, node->token->text);

        if(symbol == NULL) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "symbol %s not found", node->token->text);
            return -1;
        }

        *result = symbol->int_value;

        if(symbol->type == PASCAL_SYMBOL_TYPE_INTEGER) {
            if(symbol->is_local) {
                vm->is_at_stack = true;
                vm->at_stack_offset = symbol->stack_offset;
            } else {
                int16_t reg = pascal_vm_find_free_reg(vm);

                if(reg == -1) {
                    vm->is_at_mem = true;
                } else {
                    vm->is_at_reg = true;
                    node->used_register = reg;
                    buffer_printf(vm->text_buffer, "\tmov $%s@GOT, %%%s\n", symbol->name, pascal_vm_regs[reg]);
                    buffer_printf(vm->text_buffer, "\tmov (%%r15, %%%s), %%%s\n", pascal_vm_regs[reg], pascal_vm_regs[reg]);
                    buffer_printf(vm->text_buffer, "\tmov (%%%s), %%%s\n", pascal_vm_regs[reg], pascal_vm_regs[reg]);
                }
            }
        } else { // do we need this?
            vm->is_at_reg = true;
        }
    } else if (node->type == PASCAL_AST_NODE_TYPE_INTEGER_CONST) {
        *result = node->token->value;
        vm->is_const = true;
    } else if(node->type == PASCAL_AST_NODE_TYPE_UNARY_OP) {
        return pascal_vm_execure_unary_op(vm, node, result);
    } else if (node->type == PASCAL_AST_NODE_TYPE_BINARY_OP) {
        return pascal_vm_execure_binary_op(vm, node, result);
    } else {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unknown node type %d", node->type);
        return -1;
    }

    return 0;
}

int8_t pascal_vm_execute(pascal_vm_t * vm, int32_t * result) {
    pascal_ast_node_t * node = vm->ast->root;

    if (node == NULL) {
        return -1;
    }

    buffer_printf(vm->text_buffer, ".section .text\n");
    buffer_printf(vm->data_buffer, ".section .data\n");
    buffer_printf(vm->rodata_buffer, ".section .rodata\n");
    buffer_printf(vm->bss_buffer, ".section .bss\n");

    buffer_printf(vm->text_buffer, ".section .text._start\n");
    buffer_printf(vm->text_buffer, ".globl _start\n");
    buffer_printf(vm->text_buffer, ".type _start, @function\n");
    buffer_printf(vm->text_buffer, "_start:\n");
    buffer_printf(vm->text_buffer, "\tpush %%rbp\n");
    buffer_printf(vm->text_buffer, "\tmov %%rsp, %%rbp\n");
    buffer_printf(vm->text_buffer, "\tlea 0x0(%%rip), %%r14\n");
    buffer_printf(vm->text_buffer, ".LGOT_BASE:\n");
    buffer_printf(vm->text_buffer, "\tmov $_GLOBAL_OFFSET_TABLE_-.LGOT_BASE, %%r15\n");
    buffer_printf(vm->text_buffer, "\tadd %%r14, %%r15\n");
    buffer_printf(vm->text_buffer, "\txor %%rax, %%rax\n");


    if(pascal_vm_execute_ast_node(vm, node, result) != 0) {
        return -1;
    }

    buffer_printf(vm->text_buffer, "%s", buffer_get_view_at_position(vm->data_buffer, 0, buffer_get_length(vm->data_buffer)));
    buffer_printf(vm->text_buffer, "%s", buffer_get_view_at_position(vm->rodata_buffer, 0, buffer_get_length(vm->rodata_buffer)));
    buffer_printf(vm->text_buffer, "%s", buffer_get_view_at_position(vm->bss_buffer, 0, buffer_get_length(vm->bss_buffer)));

    return 0;
}



int32_t main(int32_t argc, char * argv[]) {
    if (argc != 3) {
        print_error("Usage: %s <in> <out>\n", argv[0]);
        return -1;
    }

    FILE* in = fopen(argv[1], "r");
    fseek(in, 0, SEEK_END);
    size_t size = ftell(in);
    fseek(in, 0, SEEK_SET);

    char_t* source = memory_malloc(size + 1);

    if (source == NULL) {
        print_error("Error: Failed to allocate memory for source\n");
        return -1;
    }

    fread(source, 1, size, in);

    source[size] = '\0';

    fclose(in);

    buffer_t * buffer = buffer_encapsulate((uint8_t*)source, size + 1);

    pascal_lexer_t lexer = {0};
    pascal_lexer_init(&lexer, buffer);

    pascal_ast_t ast = {0};
    pascal_ast_init(&ast);

    pascal_parser_t parser = {0};
    pascal_parser_init(&parser, &lexer);

    if(pascal_parser_parse(&parser, &ast) != 0) {
        print_error("Error: Parsing failed\n");
    }

    memory_free(source);
    buffer_destroy(buffer);

    pascal_vm_t vm = {0};

    if(pascal_vm_init(&vm, &ast) != 0) {
        print_error("Error: Failed to initialize vm\n");

        return -1;
    }

    int32_t result = 0;

    if(pascal_vm_execute(&vm, &result) != 0) {
        print_error("Error: Failed to execute vm\n");

        pascal_vm_print_symbol_table(&vm);

        pascal_vm_destroy(&vm);

        return -1;
    }

    pascal_vm_print_symbol_table(&vm);
    PRINTLOG(COMPILER_PASCAL, LOG_INFO, "Result: %d", result);

    FILE* out = fopen(argv[2], "w");

    if (out == NULL) {
        print_error("Error: Failed to open output file\n");

        pascal_vm_destroy(&vm);

        return -1;
    }

    size_t out_size = 0;

    uint8_t* out_bytes = buffer_get_all_bytes(vm.text_buffer, &out_size);

    fwrite(out_bytes, 1, out_size, out);

    fclose(out);

    memory_free(out_bytes);

    pascal_vm_destroy(&vm);

    PRINTLOG(COMPILER_PASCAL, LOG_INFO, "Success");

    return 0;
}
