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

const char_t* pascal_compiler_regs[] = {
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

typedef enum pascal_compiler_reg_ids_t {
    PASCAL_VM_REG_RAX = 0,
    PASCAL_VM_REG_RBX,
    PASCAL_VM_REG_RCX,
    PASCAL_VM_REG_RDX,
    PASCAL_VM_REG_RSI,
    PASCAL_VM_REG_RDI,
    PASCAL_VM_REG_R8,
    PASCAL_VM_REG_R9,
    PASCAL_VM_REG_R10,
    PASCAL_VM_REG_R11,
    PASCAL_VM_REG_R12,
    PASCAL_VM_REG_R13,
    PASCAL_VM_REG_R14,
} pascal_compiler_reg_ids_t;

const int16_t pascal_compiler_fcall_reg_order[] = {
    PASCAL_VM_REG_RDI,
    PASCAL_VM_REG_RSI,
    PASCAL_VM_REG_RDX,
    PASCAL_VM_REG_R10,
    PASCAL_VM_REG_R8,
    PASCAL_VM_REG_R9,
};

_Static_assert(sizeof(pascal_compiler_regs) / sizeof(pascal_compiler_regs[0]) == PASCAL_VM_REG_COUNT, "invalid register count");

int8_t                 pascal_compiler_init(pascal_compiler_t * compiler, pascal_ast_t * ast);
int8_t                 pascal_compiler_destroy(pascal_compiler_t * compiler);
int8_t                 pascal_compiler_execute_ast_node(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result);
int8_t                 pascal_compiler_execure_unary_op(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result);
int8_t                 pascal_compiler_execure_binary_op(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result);
int8_t                 pascal_compiler_execute_relational_op(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result);
int8_t                 pascal_compiler_execute_block(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result);
int8_t                 pascal_compiler_execute_compound(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result);
int8_t                 pascal_compiler_execute_assign(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result);
int8_t                 pascal_compiler_execute(pascal_compiler_t * compiler, int64_t* result);
int8_t                 pascal_compiler_print_symbol_table(pascal_compiler_t * compiler);
int8_t                 pascal_compiler_destroy_symbol_table(pascal_compiler_t * compiler);
int8_t                 pascal_compiler_build_stack(pascal_compiler_t* compiler);
int8_t                 pascal_compiler_find_free_reg(pascal_compiler_t* compiler);
const pascal_symbol_t* pascal_compiler_find_symbol(pascal_compiler_t* compiler, const char_t* name);
int8_t                 pascal_compiler_save_to_mem(pascal_compiler_t* compiler, const char_t* reg, const pascal_symbol_t* symbol);
int8_t                 pascal_compiler_save_const_int_to_mem(pascal_compiler_t* compiler, int64_t value, const pascal_symbol_t* symbol);
int8_t                 pascal_compiler_execute_function_call(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result);
int8_t                 pascal_compiler_execute_string_const(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result);
int8_t                 pascal_compiler_execute_if(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result);
int8_t                 pascal_compiler_execute_while(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result);
int8_t                 pascal_compiler_execute_repeat(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result);
const char_t*          pascal_compiler_cast_reg_to_size(const char_t* reg, uint8_t size);
char_t                 pascal_compiler_get_reg_suffix(uint8_t size);

const pascal_symbol_t* pascal_compiler_find_symbol(pascal_compiler_t* compiler, const char_t* name) {
    symbol_table_t* symbol_table = compiler->current_symbol_table;

    while(symbol_table) {
        const pascal_symbol_t* symbol = hashmap_get(symbol_table->symbols, name);

        if(symbol != NULL) {
            return symbol;
        }

        symbol_table = symbol_table->parent;
    }

    return NULL;
}

int8_t pascal_compiler_find_free_reg(pascal_compiler_t* compiler) {
    for(int32_t i = PASCAL_VM_REG_COUNT - 1; i >= 0; i--) {
        if(compiler->busy_regs[i] == false) {
            compiler->busy_regs[i] = true;
            return i;
        }
    }

    return -1;
}

char_t pascal_compiler_get_reg_suffix(uint8_t size) {
    if(size == 1 || size == 8) {
        return 'b';
    } else if(size == 16) {
        return 'w';
    } else if(size == 32) {
        return 'l';
    } else if(size == 64) {
        return 'q';
    }

    return '\0';
}

const char_t* pascal_compiler_cast_reg_to_size(const char_t* reg, uint8_t size) {
    if(strcmp(reg, "rax") == 0) {
        if(size == 1 || size == 64) {
            return "al";
        } else if(size == 16) {
            return "ax";
        } else if(size == 32) {
            return "eax";
        } else if(size == 64) {
            return "rax";
        }
    } else if(strcmp(reg, "rbx") == 0) {
        if(size == 1 || size == 8) {
            return "bl";
        } else if(size == 16) {
            return "bx";
        } else if(size == 32) {
            return "ebx";
        } else if(size == 64) {
            return "rbx";
        }
    } else if(strcmp(reg, "rcx") == 0) {
        if(size == 1 || size == 8) {
            return "cl";
        } else if(size == 16) {
            return "cx";
        } else if(size == 32) {
            return "ecx";
        } else if(size == 64) {
            return "rcx";
        }
    } else if(strcmp(reg, "rdx") == 0) {
        if(size == 1 || size == 8) {
            return "dl";
        } else if(size == 16) {
            return "dx";
        } else if(size == 32) {
            return "edx";
        } else if(size == 64) {
            return "rdx";
        }
    } else if(strcmp(reg, "rsi") == 0) {
        if(size == 1 || size == 8) {
            return "sil";
        } else if(size == 16) {
            return "si";
        } else if(size == 32) {
            return "esi";
        } else if(size == 64) {
            return "rsi";
        }
    } else if(strcmp(reg, "rdi") == 0) {
        if(size == 1 || size == 8) {
            return "dil";
        } else if(size == 16) {
            return "di";
        } else if(size == 32) {
            return "edi";
        } else if(size == 64) {
            return "rdi";
        }
    } else if(strcmp(reg, "r8") == 0) {
        if(size == 1 || size == 8) {
            return "r8b";
        } else if(size == 16) {
            return "r8w";
        } else if(size == 32) {
            return "r8d";
        } else if(size == 64) {
            return "r8";
        }
    } else if(strcmp(reg, "r9") == 0) {
        if(size == 1 || size == 8) {
            return "r9b";
        } else if(size == 16) {
            return "r9w";
        } else if(size == 32) {
            return "r9d";
        } else if(size == 64) {
            return "r9";
        }
    } else if(strcmp(reg, "r10") == 0) {
        if(size == 1 || size == 8) {
            return "r10b";
        } else if(size == 16) {
            return "r10w";
        } else if(size == 32) {
            return "r10d";
        } else if(size == 64) {
            return "r10";
        }
    } else if(strcmp(reg, "r11") == 0) {
        if(size == 1 || size == 8) {
            return "r11b";
        } else if(size == 16) {
            return "r11w";
        } else if(size == 32) {
            return "r11d";
        } else if(size == 64) {
            return "r11";
        }
    } else if(strcmp(reg, "r12") == 0) {
        if(size == 1 || size == 8) {
            return "r12b";
        } else if(size == 16) {
            return "r12w";
        } else if(size == 32) {
            return "r12d";
        } else if(size == 64) {
            return "r12";
        }
    } else if(strcmp(reg, "r13") == 0) {
        if(size == 1 || size == 8) {
            return "r13b";
        } else if(size == 16) {
            return "r13w";
        } else if(size == 32) {
            return "r13d";
        } else if(size == 64) {
            return "r13";
        }
    } else if(strcmp(reg, "r14") == 0) {
        if(size == 1 || size == 8) {
            return "r14b";
        } else if(size == 16) {
            return "r14w";
        } else if(size == 32) {
            return "r14d";
        } else if(size == 64) {
            return "r14";
        }
    } else if(strcmp(reg, "r15") == 0) {
        if(size == 1 || size == 8) {
            return "r15b";
        } else if(size == 16) {
            return "r15w";
        } else if(size == 32) {
            return "r15d";
        } else if(size == 64) {
            return "r15";
        }
    }

    return NULL;
}

int8_t pascal_compiler_init(pascal_compiler_t * compiler, pascal_ast_t * ast) {
    if (ast == NULL) {
        return -1;
    }

    compiler->text_buffer = buffer_new();

    if (compiler->text_buffer == NULL) {
        return -1;
    }

    compiler->data_buffer = buffer_new();

    if (compiler->data_buffer == NULL) {
        buffer_destroy(compiler->text_buffer);
        return -1;
    }

    compiler->rodata_buffer = buffer_new();

    if (compiler->rodata_buffer == NULL) {
        buffer_destroy(compiler->text_buffer);
        buffer_destroy(compiler->data_buffer);
        return -1;
    }

    compiler->bss_buffer = buffer_new();

    if (compiler->bss_buffer == NULL) {
        buffer_destroy(compiler->text_buffer);
        buffer_destroy(compiler->data_buffer);
        buffer_destroy(compiler->rodata_buffer);
        return -1;
    }

    symbol_table_t* symbol_table = memory_malloc(sizeof(symbol_table_t));

    if (symbol_table == NULL) {
        buffer_destroy(compiler->text_buffer);
        buffer_destroy(compiler->data_buffer);
        buffer_destroy(compiler->rodata_buffer);
        buffer_destroy(compiler->bss_buffer);
        return -1;
    }

    symbol_table->symbols = hashmap_string(128);

    if (symbol_table->symbols == NULL) {
        buffer_destroy(compiler->text_buffer);
        buffer_destroy(compiler->data_buffer);
        buffer_destroy(compiler->rodata_buffer);
        buffer_destroy(compiler->bss_buffer);
        memory_free(symbol_table);
        return -1;
    }

    compiler->main_symbol_table = symbol_table;
    compiler->current_symbol_table = symbol_table;

    compiler->ast = ast;

    compiler->next_stack_offset = 16;
    compiler->stack_size = 8;

    compiler->cond_label_stack = linkedlist_create_stack();
    compiler->loop_label_stack = linkedlist_create_stack();

    if (compiler->cond_label_stack == NULL) {
        buffer_destroy(compiler->text_buffer);
        buffer_destroy(compiler->data_buffer);
        buffer_destroy(compiler->rodata_buffer);
        buffer_destroy(compiler->bss_buffer);
        hashmap_destroy(symbol_table->symbols);
        memory_free(symbol_table);
        return -1;
    }

    return 0;
}

int8_t pascal_compiler_print_symbol_table(pascal_compiler_t * compiler) {
    symbol_table_t* symbol_table = compiler->current_symbol_table;

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

            PRINTLOG(COMPILER_PASCAL, LOG_INFO, "symbol %s type %d size %lli value %lli", symbol->name, symbol->type, symbol->size, symbol->int_value);

            iter->next(iter);
        }

        iter->destroy(iter);

        symbol_table = symbol_table->parent;
    }

    return 0;
}

int8_t pascal_compiler_destroy_symbol_table(pascal_compiler_t * compiler) {
    symbol_table_t* symbol_table = compiler->current_symbol_table;

    while(symbol_table) {
        hashmap_destroy(symbol_table->symbols);

        symbol_table_t* parent = symbol_table->parent;

        memory_free(symbol_table);

        symbol_table = parent;
    }

    return 0;
}

int8_t pascal_compiler_destroy(pascal_compiler_t * compiler) {
    if (compiler->text_buffer != NULL) {
        buffer_destroy(compiler->text_buffer);
    }

    if (compiler->data_buffer != NULL) {
        buffer_destroy(compiler->data_buffer);
    }

    if (compiler->rodata_buffer != NULL) {
        buffer_destroy(compiler->rodata_buffer);
    }

    if (compiler->bss_buffer != NULL) {
        buffer_destroy(compiler->bss_buffer);
    }

    linkedlist_destroy(compiler->cond_label_stack);
    linkedlist_destroy(compiler->loop_label_stack);

    pascal_compiler_destroy_symbol_table(compiler);

    memory_free(compiler->program_name_symbol);

    pascal_ast_destroy(compiler->ast);
    return 0;
}

int8_t pascal_compiler_build_stack(pascal_compiler_t* compiler) {
    if (compiler->current_symbol_table == NULL) {
        return -1;
    }

    iterator_t* iter = hashmap_iterator_create(compiler->current_symbol_table->symbols);

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
            buffer_printf(compiler->text_buffer, "\tmovq $%lli, -%d(%%rbp)\n", symbol->int_value, symbol->stack_offset);
        }

        iter->next(iter);
    }

    iter->destroy(iter);

    return 0;
}


int8_t pascal_compiler_execute_block(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result) {
    if(node->left) {
        if(node->left->type == PASCAL_AST_NODE_TYPE_DECLS) {

            for(size_t i = 0; i < linkedlist_size(node->left->children); i++) {
                pascal_ast_node_t * tmp_node = (pascal_ast_node_t*)linkedlist_get_data_at_position(node->left->children, i);

                for(size_t j = 0; j < linkedlist_size(tmp_node->children); j++) {
                    pascal_symbol_t * tmp_symbol = (pascal_symbol_t*)linkedlist_get_data_at_position(tmp_node->children, j);

                    if(strcmp(compiler->program_name, tmp_symbol->name) != 0 && hashmap_get(compiler->current_symbol_table->symbols, tmp_symbol->name) != NULL) {
                        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "symbol %s already defined", tmp_symbol->name);
                        return -1;
                    }

                    if(tmp_symbol->type == PASCAL_SYMBOL_TYPE_INTEGER) {

                        if(tmp_symbol->is_local){
                            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "local symbol %s type %d size %lli value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);
                            // tmp_symbol->stack_offset = compiler->next_stack_offset;
                            // compiler->next_stack_offset += tmp_symbol->size;
                            // compiler->stack_size += tmp_symbol->size;
                        } else {
                            tmp_symbol->stack_offset = 0;

                            if(tmp_symbol->is_const){
                                PRINTLOG(COMPILER_PASCAL, LOG_INFO, "rodata symbol %s type %d size %lli value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);
                                buffer_printf(compiler->rodata_buffer, ".section .rodata.%s\n", tmp_symbol->name);
                                buffer_printf(compiler->rodata_buffer, ".align 8\n");
                                buffer_printf(compiler->rodata_buffer, ".local %s\n", tmp_symbol->name);
                                buffer_printf(compiler->rodata_buffer, ".type %s, @object\n", tmp_symbol->name);
                                buffer_printf(compiler->rodata_buffer, ".size %s, %lli\n", tmp_symbol->name, tmp_symbol->size);
                                buffer_printf(compiler->rodata_buffer, "%s:\n", tmp_symbol->name);
                                buffer_printf(compiler->rodata_buffer, "\t.long %lli\n", tmp_symbol->int_value);
                                // buffer_printf(compiler->rodata_buffer, ".size %s, .-%s\n", tmp_symbol->name, tmp_symbol->name);
                                buffer_printf(compiler->rodata_buffer, "\n\n\n");
                            } else {
                                if(tmp_symbol->int_value == 0) {
                                    PRINTLOG(COMPILER_PASCAL, LOG_INFO, "bss symbol %s type %d size %lli value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);
                                    buffer_printf(compiler->bss_buffer, ".section .bss.%s\n", tmp_symbol->name);
                                    buffer_printf(compiler->bss_buffer, ".align 8\n");
                                    buffer_printf(compiler->bss_buffer, ".local %s\n", tmp_symbol->name);
                                    buffer_printf(compiler->bss_buffer, ".type %s, @object\n", tmp_symbol->name);
                                    buffer_printf(compiler->bss_buffer, ".size %s, %lli\n", tmp_symbol->name, tmp_symbol->size);
                                    buffer_printf(compiler->bss_buffer, "%s:\n", tmp_symbol->name);
                                    buffer_printf(compiler->bss_buffer, "\t.zero %lli\n", tmp_symbol->size / 8);
                                    // buffer_printf(compiler->bss_buffer, ".size %s, .-%s\n", tmp_symbol->name, tmp_symbol->name);
                                    buffer_printf(compiler->bss_buffer, "\n\n\n");
                                } else {
                                    PRINTLOG(COMPILER_PASCAL, LOG_INFO, "data symbol %s type %d size %lli value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);
                                    buffer_printf(compiler->data_buffer, ".section .data.%s\n", tmp_symbol->name);
                                    buffer_printf(compiler->data_buffer, ".align 8\n");
                                    buffer_printf(compiler->data_buffer, ".local %s\n", tmp_symbol->name);
                                    buffer_printf(compiler->data_buffer, ".type %s, @object\n", tmp_symbol->name);
                                    buffer_printf(compiler->data_buffer, ".size %s, %lli\n", tmp_symbol->name, tmp_symbol->size);
                                    buffer_printf(compiler->data_buffer, "%s:\n", tmp_symbol->name);
                                    buffer_printf(compiler->data_buffer, "\t.long %lli\n", tmp_symbol->int_value);
                                    // buffer_printf(compiler->data_buffer, ".size %s, .-%s\n", tmp_symbol->name, tmp_symbol->name);
                                    buffer_printf(compiler->data_buffer, "\n\n\n");
                                }
                            }
                        }
                    }

                    hashmap_put(compiler->current_symbol_table->symbols, tmp_symbol->name, tmp_symbol);
                }
            }
        } else {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "expected decls");
            return -1;
        }
    } else {
        PRINTLOG(COMPILER_PASCAL, LOG_INFO, "no decls");
    }

    return pascal_compiler_execute_ast_node(compiler, node->right, result);
}

int8_t pascal_compiler_execure_unary_op(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result) {
    int64_t right = 0;

    if(pascal_compiler_execute_ast_node(compiler, node->right, &right) != 0) {
        return -1;
    }

    if (node->token->type == PASCAL_TOKEN_TYPE_PLUS) {
        *result = +right;
    } else if (node->token->type == PASCAL_TOKEN_TYPE_MINUS) {
        buffer_printf(compiler->text_buffer, "# unary minus\n");

        *result = -right;

        if(compiler->is_at_mem) {
            buffer_printf(compiler->text_buffer, "\tneg %s\n", node->right->token->text);
        } else if(compiler->is_at_reg) {
            buffer_printf(compiler->text_buffer, "\tneg%c %%%s\n",
                          pascal_compiler_get_reg_suffix(compiler->computed_size),
                          pascal_compiler_cast_reg_to_size(pascal_compiler_regs[node->right->used_register], compiler->computed_size));
            node->used_register = node->right->used_register;
        } else if(compiler->is_const) {
            *result = -*result;
        } else if(compiler->is_at_stack) {
            buffer_printf(compiler->text_buffer, "\tneg%c -%d(%%rbp)\n",
                          pascal_compiler_get_reg_suffix(compiler->computed_size),
                          compiler->at_stack_offset);
        } else {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "need inspect");
            int16_t reg = pascal_compiler_find_free_reg(compiler);
            buffer_printf(compiler->text_buffer, "\tpop %%%s\n", pascal_compiler_regs[reg]);
            buffer_printf(compiler->text_buffer, "\tneg %%%s\n", pascal_compiler_regs[reg]);
            compiler->is_at_reg = true;
            node->used_register = reg;
            return -1;
        }

    } else if(node->token->type == PASCAL_TOKEN_TYPE_NOT) {
        *result = !right;

        if(compiler->is_at_reg) {
            buffer_printf(compiler->text_buffer, "\tnot%c %%%s\n",
                          pascal_compiler_get_reg_suffix(compiler->computed_size),
                          pascal_compiler_cast_reg_to_size(pascal_compiler_regs[node->right->used_register], compiler->computed_size));
            node->used_register = node->right->used_register;
        } else if(compiler->is_const) {
            *result = !*result;
        } else if(compiler->is_at_stack) {
            buffer_printf(compiler->text_buffer, "\tnot%c -%d(%%rbp)\n",
                          pascal_compiler_get_reg_suffix(compiler->computed_size),
                          compiler->at_stack_offset);
        } else {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "need inspect");
            int16_t reg = pascal_compiler_find_free_reg(compiler);
            buffer_printf(compiler->text_buffer, "\tpop %%%s\n", pascal_compiler_regs[reg]);
            buffer_printf(compiler->text_buffer, "\tnot %%%s\n", pascal_compiler_regs[reg]);
            compiler->is_at_reg = true;
            node->used_register = reg;

            return -1;
        }
    } else {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unknown unary op");
        return -1;
    }

    return 0;
}

int8_t pascal_compiler_execute_if(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result) {
    int64_t condition = 0;

    compiler->cond_depth++;

    char_t* label = sprintf(".L%d", compiler->next_label_id++);

    linkedlist_stack_push(compiler->cond_label_stack, label);

    buffer_printf(compiler->text_buffer, "# begin if cond %lli\n", compiler->cond_depth);

    compiler->is_cond_eval = true;

    if(pascal_compiler_execute_ast_node(compiler, node->condition, &condition) != 0) {
        memory_free(label);

        return -1;
    }

    compiler->is_cond_eval = false;

    if(node->condition->type != PASCAL_AST_NODE_TYPE_RELATIONAL_OP) {
        if(compiler->is_const) {
            if(!condition) {
                buffer_printf(compiler->text_buffer, "\tjmp %s\n", label);
            }
        } else if(compiler->is_at_reg) {
            if(compiler->computed_type == PASCAL_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tjz %s\n", label);
            } else {
                buffer_printf(compiler->text_buffer, "\tcmp $0, %%%s\n", pascal_compiler_regs[node->condition->used_register]);
                buffer_printf(compiler->text_buffer, "\tje %s\n", label);
            }

            compiler->busy_regs[node->condition->used_register] = false;
            compiler->is_at_reg = false;
        } else if(compiler->is_at_stack) {
            if(compiler->computed_type == PASCAL_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tjz %s\n", label);
            } else {
                buffer_printf(compiler->text_buffer, "\tcmp%c $0, -%d(%%rbp)\n",
                              pascal_compiler_get_reg_suffix(compiler->computed_size),
                              compiler->at_stack_offset);
                buffer_printf(compiler->text_buffer, "\tje %s\n", label);
            }
        } else {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "need inspect");
            return -1;
        }
    }

    buffer_printf(compiler->text_buffer, "# end if cond %lli\n", compiler->cond_depth);
    buffer_printf(compiler->text_buffer, "# begin if body %lli\n", compiler->cond_depth);

    if(pascal_compiler_execute_ast_node(compiler, node->left, result) != 0) {
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

        if(pascal_compiler_execute_ast_node(compiler, node->right, result) != 0) {
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

int8_t pascal_compiler_execute_while(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result) {
    int64_t condition = 0;

    compiler->loop_depth++;

    char_t* while_label = sprintf(".L%d", compiler->next_label_id++);
    char_t* end_label = sprintf(".L%d", compiler->next_label_id++);

    buffer_printf(compiler->text_buffer, "# begin while %lli\n", compiler->loop_depth);
    buffer_printf(compiler->text_buffer, "%s:\n", while_label);
    buffer_printf(compiler->text_buffer, "# begin while cond %lli\n", compiler->loop_depth);


    linkedlist_stack_push(compiler->cond_label_stack, end_label);

    compiler->is_cond_eval = true;

    if(pascal_compiler_execute_ast_node(compiler, node->condition, &condition) != 0) {
        memory_free(end_label);
        memory_free(while_label);

        return -1;
    }

    compiler->is_cond_eval = false;

    linkedlist_stack_push(compiler->cond_label_stack, end_label);

    if(node->condition->type != PASCAL_AST_NODE_TYPE_RELATIONAL_OP) {
        if(compiler->is_const) {
            if(!condition) {
                buffer_printf(compiler->text_buffer, "\tjmp %s\n", end_label);
            }
        } else if(compiler->is_at_reg) {
            if(compiler->computed_type == PASCAL_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tjz %s\n", end_label);
            } else {
                buffer_printf(compiler->text_buffer, "\tcmp $0, %%%s\n", pascal_compiler_regs[node->condition->used_register]);
                buffer_printf(compiler->text_buffer, "\tje %s\n", end_label);
            }

            compiler->busy_regs[node->condition->used_register] = false;
            compiler->is_at_reg = false;
        } else if(compiler->is_at_stack) {
            if(compiler->computed_type == PASCAL_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tjz %s\n", end_label);
            } else {
                buffer_printf(compiler->text_buffer, "\tcmp%c $0, -%d(%%rbp)\n",
                              pascal_compiler_get_reg_suffix(compiler->computed_size),
                              compiler->at_stack_offset);
                buffer_printf(compiler->text_buffer, "\tje %s\n", end_label);
            }
        } else {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "need inspect");
            return -1;
        }
    }

    buffer_printf(compiler->text_buffer, "# end while cond %lli\n", compiler->cond_depth);
    buffer_printf(compiler->text_buffer, "# begin while body %lli\n", compiler->cond_depth);

    linkedlist_stack_push(compiler->loop_label_stack, end_label);

    if(pascal_compiler_execute_ast_node(compiler, node->left, result) != 0) {
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

int8_t pascal_compiler_execute_repeat(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result) {
    int64_t condition = 0;

    compiler->loop_depth++;

    char_t* repeat_label = sprintf(".L%d", compiler->next_label_id++);
    char_t* end_label = sprintf(".L%d", compiler->next_label_id++);

    buffer_printf(compiler->text_buffer, "# begin repeat %lli\n", compiler->loop_depth);
    buffer_printf(compiler->text_buffer, "%s:\n", repeat_label);


    buffer_printf(compiler->text_buffer, "# begin repeat body %lli\n", compiler->cond_depth);

    linkedlist_stack_push(compiler->loop_label_stack, end_label);

    if(pascal_compiler_execute_ast_node(compiler, node->left, result) != 0) {
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

    if(pascal_compiler_execute_ast_node(compiler, node->condition, &condition) != 0) {
        memory_free(end_label);
        memory_free(repeat_label);

        return -1;
    }

    compiler->is_cond_reverse = false;
    compiler->is_cond_eval = false;

    linkedlist_stack_pop(compiler->cond_label_stack);

    if(node->condition->type != PASCAL_AST_NODE_TYPE_RELATIONAL_OP) {
        if(compiler->is_const) {
            if(!condition) {
                buffer_printf(compiler->text_buffer, "\tjmp %s\n", end_label);
            }
        } else if(compiler->is_at_reg) {
            if(compiler->computed_type == PASCAL_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tjz %s\n", end_label);
            } else {
                buffer_printf(compiler->text_buffer, "\tcmp $0, %%%s\n", pascal_compiler_regs[node->condition->used_register]);
                buffer_printf(compiler->text_buffer, "\tje %s\n", end_label);
            }

            compiler->busy_regs[node->condition->used_register] = false;
            compiler->is_at_reg = false;
        } else if(compiler->is_at_stack) {
            if(compiler->computed_type == PASCAL_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tjz %s\n", end_label);
            } else {
                buffer_printf(compiler->text_buffer, "\tcmp%c $0, -%d(%%rbp)\n",
                              pascal_compiler_get_reg_suffix(compiler->computed_size),
                              compiler->at_stack_offset);
                buffer_printf(compiler->text_buffer, "\tje %s\n", end_label);
            }
        } else {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "need inspect");
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

int8_t pascal_compiler_execute_relational_op(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result) {
    int64_t left = 0;
    int64_t right = 0;

    buffer_printf(compiler->text_buffer, "# begin relational op\n");

    if(pascal_compiler_execute_ast_node(compiler, node->left, &left) != 0) {
        return -1;
    }

    boolean_t left_is_const = compiler->is_const;
    compiler->is_const = false;
    boolean_t left_is_at_reg = compiler->is_at_reg;
    int16_t left_at_reg = node->left->used_register;
    compiler->is_at_reg = false;
    boolean_t left_is_at_stack = compiler->is_at_stack;
    compiler->is_at_stack = false;
    uint16_t left_at_stack_offset = compiler->at_stack_offset;
    int64_t left_size = compiler->computed_size;


    if(!left_is_const) {
        if(!left_is_at_reg) {
            left_at_reg = pascal_compiler_find_free_reg(compiler);

            if(left_at_reg == -1) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "no free registers");
                return -1;
            }

            if(left_is_at_stack) {
                buffer_printf(compiler->text_buffer, "\tmov -%d(%%rbp), %%%s\n", left_at_stack_offset, pascal_compiler_regs[left_at_reg]);
            } else {
                buffer_printf(compiler->text_buffer, "\tpop %%%s\n", pascal_compiler_regs[left_at_reg]);
            }
        }
    }

    if(pascal_compiler_execute_ast_node(compiler, node->right, &right) != 0) {
        return -1;
    }

    boolean_t right_is_const = compiler->is_const;
    compiler->is_const = false;
    boolean_t right_is_at_reg = compiler->is_at_reg;
    compiler->is_at_reg = false;
    int16_t right_at_reg = node->right->used_register;
    boolean_t right_is_at_stack = compiler->is_at_stack;
    compiler->is_at_stack = false;
    uint16_t right_at_stack_offset = compiler->at_stack_offset;
    int64_t right_size = compiler->computed_size;

    if(!right_is_const) {
        if(!right_is_at_reg) {
            right_at_reg = pascal_compiler_find_free_reg(compiler);

            if(right_at_reg == -1) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "no free registers");
                return -1;
            }

            if(right_is_at_stack) {
                buffer_printf(compiler->text_buffer, "\tmov -%d(%%rbp), %%%s\n", right_at_stack_offset, pascal_compiler_regs[right_at_reg]);
            } else {
                buffer_printf(compiler->text_buffer, "\tpop %%%s\n", pascal_compiler_regs[right_at_reg]);
            }
        }
    }

    if(left_is_const && right_is_const) {
        compiler->is_const = true;
    }

    int64_t max_size = MAX(left_size, right_size);

    char_t reg_suffix = pascal_compiler_get_reg_suffix(max_size);

    const char_t* left_reg = pascal_compiler_cast_reg_to_size(pascal_compiler_regs[left_at_reg], max_size);
    const char_t* right_reg = pascal_compiler_cast_reg_to_size(pascal_compiler_regs[right_at_reg], max_size);

    if(!left_is_const && left_size < max_size) {
        buffer_printf(compiler->text_buffer, "# cast left %lli to %lli\n", left_size, max_size);
        buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                      pascal_compiler_cast_reg_to_size(pascal_compiler_regs[left_at_reg], left_size),
                      left_reg);
    }

    if(!right_is_const && right_size < max_size) {
        buffer_printf(compiler->text_buffer, "# cast right %lli to %lli\n", right_size, max_size);
        buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                      pascal_compiler_cast_reg_to_size(pascal_compiler_regs[right_at_reg], right_size),
                      right_reg);
    }

    if(!compiler->is_const) {
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

        compiler->is_at_reg = true;
    }

    compiler->computed_size = 8;
    compiler->computed_type = PASCAL_SYMBOL_TYPE_BOOLEAN;

    const char_t* used_reg = pascal_compiler_cast_reg_to_size(pascal_compiler_regs[node->used_register], compiler->computed_size);

    if (node->token->type == PASCAL_TOKEN_TYPE_EQUAL) {
        *result = left == right;

        if(!compiler->is_const) {

            if(compiler->is_cond_eval) {
                const char_t* label = linkedlist_stack_peek(compiler->cond_label_stack);

                if(compiler->is_cond_reverse) {
                    buffer_printf(compiler->text_buffer, "\tje %s\n", label);
                } else {
                    buffer_printf(compiler->text_buffer, "\tjne %s\n", label);
                }
            } else {
                buffer_printf(compiler->text_buffer, "\tseteb %%%s\n", used_reg);
            }
        }

    } else if (node->token->type == PASCAL_TOKEN_TYPE_NOT_EQUAL) {
        *result = left != right;

        if(!compiler->is_const) {
            if(compiler->is_cond_eval) {
                const char_t* label = linkedlist_stack_peek(compiler->cond_label_stack);

                if(compiler->is_cond_reverse) {
                    buffer_printf(compiler->text_buffer, "\tjne %s\n", label);
                } else {
                    buffer_printf(compiler->text_buffer, "\tje %s\n", label);
                }
            } else {
                buffer_printf(compiler->text_buffer, "\tsetne %%%s\n", used_reg);
            }
        }

    } else if (node->token->type == PASCAL_TOKEN_TYPE_LESS_THAN) {
        *result = left < right;

        if(!compiler->is_const) {
            if(compiler->is_cond_eval) {
                const char_t* label = linkedlist_stack_peek(compiler->cond_label_stack);

                if(compiler->is_cond_reverse) {
                    buffer_printf(compiler->text_buffer, "\tjl %s\n", label);
                } else {
                    buffer_printf(compiler->text_buffer, "\tjge %s\n", label);
                }
            } else {
                buffer_printf(compiler->text_buffer, "\tsetl %%%s\n", used_reg);
            }
        }

    } else if (node->token->type == PASCAL_TOKEN_TYPE_LESS_THAN_OR_EQUAL) {
        *result = left <= right;

        if(!compiler->is_const) {
            if(compiler->is_cond_eval) {
                const char_t* label = linkedlist_stack_peek(compiler->cond_label_stack);

                if(compiler->is_cond_reverse) {
                    buffer_printf(compiler->text_buffer, "\tjle %s\n", label);
                } else {
                    buffer_printf(compiler->text_buffer, "\tjg %s\n", label);
                }
            } else {
                buffer_printf(compiler->text_buffer, "\tsetle %%%s\n", used_reg);
            }
        }

    } else if (node->token->type == PASCAL_TOKEN_TYPE_GREATER_THAN) {
        *result = left > right;

        if(!compiler->is_const) {
            if(compiler->is_cond_eval) {
                const char_t* label = linkedlist_stack_peek(compiler->cond_label_stack);

                if(compiler->is_cond_reverse) {
                    buffer_printf(compiler->text_buffer, "\tjg %s\n", label);
                } else {
                    buffer_printf(compiler->text_buffer, "\tjle %s\n", label);
                }
            } else {
                buffer_printf(compiler->text_buffer, "\tsetg %%%s\n", used_reg);
            }
        }

    } else if (node->token->type == PASCAL_TOKEN_TYPE_GREATER_THAN_OR_EQUAL) {
        *result = left >= right;

        if(!compiler->is_const) {
            if(compiler->is_cond_eval) {
                const char_t* label = linkedlist_stack_peek(compiler->cond_label_stack);

                if(compiler->is_cond_reverse) {
                    buffer_printf(compiler->text_buffer, "\tjge %s\n", label);
                } else {
                    buffer_printf(compiler->text_buffer, "\tjl %s\n", label);
                }
            } else {
                buffer_printf(compiler->text_buffer, "\tsetge %%%s\n", used_reg);
            }
        }

    } else {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unknown relational op");
        return -1;
    }

    if(compiler->is_const) {
        if(compiler->is_cond_eval) {
            const char_t* label = linkedlist_stack_peek(compiler->cond_label_stack);

            if(!*result) {
                buffer_printf(compiler->text_buffer, "\tjmp %s\n", label);
            }
        }
    }

    buffer_printf(compiler->text_buffer, "# end relational op\n");

    return 0;
}

int8_t pascal_compiler_execure_binary_op(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result) {
    int64_t left = 0;
    int64_t right = 0;

    buffer_printf(compiler->text_buffer, "# begin binary op\n");

    if(pascal_compiler_execute_ast_node(compiler, node->left, &left) != 0) {
        return -1;
    }

    boolean_t left_is_const = compiler->is_const;
    compiler->is_const = false;
    boolean_t left_is_at_reg = compiler->is_at_reg;
    int16_t left_at_reg = node->left->used_register;
    compiler->is_at_reg = false;
    boolean_t left_is_at_stack = compiler->is_at_stack;
    compiler->is_at_stack = false;
    uint16_t left_at_stack_offset = compiler->at_stack_offset;
    int64_t left_size = compiler->computed_size;
    pascal_symbol_type_t left_type = compiler->computed_type;


    if(!left_is_const) {
        if(!left_is_at_reg) {
            left_at_reg = pascal_compiler_find_free_reg(compiler);

            if(left_at_reg == -1) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "no free registers");
                return -1;
            }

            if(left_is_at_stack) {
                buffer_printf(compiler->text_buffer, "\tmov -%d(%%rbp), %%%s\n", left_at_stack_offset, pascal_compiler_regs[left_at_reg]);
            } else {
                buffer_printf(compiler->text_buffer, "\tpop %%%s\n", pascal_compiler_regs[left_at_reg]);
            }
        }
    }

    if(pascal_compiler_execute_ast_node(compiler, node->right, &right) != 0) {
        return -1;
    }

    boolean_t right_is_const = compiler->is_const;
    compiler->is_const = false;
    boolean_t right_is_at_reg = compiler->is_at_reg;
    compiler->is_at_reg = false;
    int16_t right_at_reg = node->right->used_register;
    boolean_t right_is_at_stack = compiler->is_at_stack;
    compiler->is_at_stack = false;
    uint16_t right_at_stack_offset = compiler->at_stack_offset;
    int64_t right_size = compiler->computed_size;
    pascal_symbol_type_t right_type = compiler->computed_type;

    if(!right_is_const) {
        if(!right_is_at_reg) {
            right_at_reg = pascal_compiler_find_free_reg(compiler);

            if(right_at_reg == -1) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "no free registers");
                return -1;
            }

            if(right_is_at_stack) {
                buffer_printf(compiler->text_buffer, "\tmov -%d(%%rbp), %%%s\n", right_at_stack_offset, pascal_compiler_regs[right_at_reg]);
            } else {
                buffer_printf(compiler->text_buffer, "\tpop %%%s\n", pascal_compiler_regs[right_at_reg]);
            }
        }
    }

    if(left_is_const && right_is_const) {
        compiler->is_const = true;
    }

    int64_t max_size = MAX(left_size, right_size);

    compiler->computed_size = max_size;

    char_t reg_suffix = pascal_compiler_get_reg_suffix(max_size);

    const char_t* left_reg = pascal_compiler_cast_reg_to_size(pascal_compiler_regs[left_at_reg], max_size);
    const char_t* right_reg = pascal_compiler_cast_reg_to_size(pascal_compiler_regs[right_at_reg], max_size);

    if(!left_is_const && left_size < max_size) {
        buffer_printf(compiler->text_buffer, "# cast left %lli to %lli\n", left_size, max_size);
        buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                      pascal_compiler_cast_reg_to_size(pascal_compiler_regs[left_at_reg], left_size),
                      left_reg);
    }

    if(!right_is_const && right_size < max_size) {
        buffer_printf(compiler->text_buffer, "# cast right %lli to %lli\n", right_size, max_size);
        buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                      pascal_compiler_cast_reg_to_size(pascal_compiler_regs[right_at_reg], right_size),
                      right_reg);
    }

    if (node->token->type == PASCAL_TOKEN_TYPE_PLUS) {
        *result = left + right;
        if(!compiler->is_const) {
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
            compiler->is_at_reg = true;
        }
    } else if (node->token->type == PASCAL_TOKEN_TYPE_MINUS) {
        *result = left - right;

        if(!compiler->is_const) {
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
            compiler->is_at_reg = true;
        }
    } else if (node->token->type == PASCAL_TOKEN_TYPE_MULTIPLY) {
        *result = left * right;

        if(!compiler->is_const) {
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
            compiler->is_at_reg = true;
        }
    } else if (node->token->type == PASCAL_TOKEN_TYPE_INTEGER_DIVIDE) {
        *result = left / right;

        if(!compiler->is_const) {
            boolean_t need_swap = false;
            int16_t swap_reg = -1;

            if(left_at_reg != 0) {
                if(compiler->busy_regs[0]) {
                    need_swap = true;
                    swap_reg = pascal_compiler_find_free_reg(compiler);

                    if(swap_reg == -1) {
                        buffer_printf(compiler->text_buffer, "\tpush %%rax\n");
                    } else {
                        buffer_printf(compiler->text_buffer, "\tmov %%rax, %%%s\n", pascal_compiler_regs[swap_reg]);
                    }
                }
            }

            if(left_is_const) {
                buffer_printf(compiler->text_buffer, "\tmov%c $0x%llx, %%%s\n", reg_suffix, left, pascal_compiler_cast_reg_to_size("rax", max_size));
                buffer_printf(compiler->text_buffer, "\tcqo\n");
                buffer_printf(compiler->text_buffer, "\tidiv%c %%%s\n", reg_suffix, right_reg);
                node->used_register = 0;
            } else if(right_is_const) {
                boolean_t old_rdx_busy = compiler->busy_regs[3];
                compiler->busy_regs[3] = true;
                int16_t const_swap_reg = pascal_compiler_find_free_reg(compiler);
                compiler->busy_regs[3] = old_rdx_busy;

                if(const_swap_reg == -1) {
                    const_swap_reg = 8;
                    buffer_printf(compiler->text_buffer, "\tpush %%%s\n", pascal_compiler_regs[right_at_reg]);
                }

                buffer_printf(compiler->text_buffer, "\tmov%c $0x%llx, %%%s\n", reg_suffix, right, pascal_compiler_cast_reg_to_size(pascal_compiler_regs[const_swap_reg], max_size));


                buffer_printf(compiler->text_buffer, "\tmov%c %%%s, %%%s\n",
                              reg_suffix,
                              pascal_compiler_cast_reg_to_size(pascal_compiler_regs[left_at_reg], max_size),
                              pascal_compiler_cast_reg_to_size("rax", max_size));
                buffer_printf(compiler->text_buffer, "\tcqo\n");
                buffer_printf(compiler->text_buffer, "\tidiv%c %%%s\n", reg_suffix, pascal_compiler_cast_reg_to_size(pascal_compiler_regs[const_swap_reg], max_size));
                compiler->busy_regs[left_at_reg] = false;
                node->used_register = 0;

                if(const_swap_reg == -1) {
                    buffer_printf(compiler->text_buffer, "\tpop %%%s\n", pascal_compiler_regs[const_swap_reg]);
                } else {
                    compiler->busy_regs[const_swap_reg] = false;
                }
            } else {
                buffer_printf(compiler->text_buffer, "\tmov%c %%%s, %%rax\n", reg_suffix, pascal_compiler_cast_reg_to_size(pascal_compiler_regs[left_at_reg], max_size));
                buffer_printf(compiler->text_buffer, "\tcqo\n");
                buffer_printf(compiler->text_buffer, "\tidiv%c %%%s\n", reg_suffix, pascal_compiler_cast_reg_to_size(pascal_compiler_regs[right_at_reg], max_size));
                node->used_register = 0;
                compiler->busy_regs[left_at_reg] = false;
                compiler->busy_regs[right_at_reg] = false;
            }

            compiler->busy_regs[0] = true;
            compiler->is_at_reg = true;

            if(need_swap) {
                if(swap_reg == -1) {
                    buffer_printf(compiler->text_buffer, "\txchg %%rax, (%%rsp)\n");
                    buffer_printf(compiler->text_buffer, "\tpop %%rax\n");
                    compiler->is_at_reg = false;
                } else {
                    buffer_printf(compiler->text_buffer, "\txchg %%%s, %%rax\n", pascal_compiler_regs[swap_reg]);
                    node->used_register = swap_reg;
                    compiler->is_at_reg = true;
                }
            }


        }
    } else if(node->token->type == PASCAL_TOKEN_TYPE_AND) {
        if(left_type != PASCAL_SYMBOL_TYPE_BOOLEAN || right_type != PASCAL_SYMBOL_TYPE_BOOLEAN) {
            compiler->computed_type = PASCAL_SYMBOL_TYPE_INTEGER;
            *result = left & right;
        } else {
            compiler->computed_type = PASCAL_SYMBOL_TYPE_BOOLEAN;
            *result = left && right;
        }

        buffer_printf(compiler->text_buffer, "# and left is 0x%llx right is 0x%llx\n", left, right);

        if(!compiler->is_const) {
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

            if(!compiler->is_cond_eval && compiler->computed_type == PASCAL_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tsetne %%%s\n", pascal_compiler_cast_reg_to_size(pascal_compiler_regs[node->used_register], compiler->computed_size));
            }

            compiler->is_at_reg = true;
        }
        buffer_printf(compiler->text_buffer, "# and result is 0x%llx\n", *result);

    } else if(node->token->type == PASCAL_TOKEN_TYPE_OR) {
        if(left_type != PASCAL_SYMBOL_TYPE_BOOLEAN || right_type != PASCAL_SYMBOL_TYPE_BOOLEAN) {
            compiler->computed_type = PASCAL_SYMBOL_TYPE_INTEGER;
            *result = left | right;
        } else {
            compiler->computed_type = PASCAL_SYMBOL_TYPE_BOOLEAN;
            *result = left || right;
        }

        if(!compiler->is_const) {
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

            if(!compiler->is_cond_eval && compiler->computed_type == PASCAL_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tsetne %%%s\n", pascal_compiler_cast_reg_to_size(pascal_compiler_regs[node->used_register], compiler->computed_size));
            }

            compiler->is_at_reg = true;
        }
    } else if(node->token->type == PASCAL_TOKEN_TYPE_XOR) {
        if(left_type != PASCAL_SYMBOL_TYPE_BOOLEAN || right_type != PASCAL_SYMBOL_TYPE_BOOLEAN) {
            compiler->computed_type = PASCAL_SYMBOL_TYPE_INTEGER;
            *result = left ^ right;
        } else {
            compiler->computed_type = PASCAL_SYMBOL_TYPE_BOOLEAN;
            *result = !left != !right;
        }

        if(!compiler->is_const) {
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

            if(!compiler->is_cond_eval && compiler->computed_type == PASCAL_SYMBOL_TYPE_BOOLEAN) {
                buffer_printf(compiler->text_buffer, "\tsetne %%%s\n", pascal_compiler_cast_reg_to_size(pascal_compiler_regs[node->used_register], compiler->computed_size));
            }

            compiler->is_at_reg = true;
        }
    } else if(node->token->type == PASCAL_TOKEN_TYPE_SHL) {
        *result = left << right;

        if(!compiler->is_const) {
            if(left_is_const) {
                left_at_reg = pascal_compiler_find_free_reg(compiler);

                if(left_at_reg == -1) {
                    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "no free registers");
                    return -1;
                }

                left_reg = pascal_compiler_cast_reg_to_size(pascal_compiler_regs[left_at_reg], max_size);

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

            compiler->is_at_reg = true;
        }
    }  else if(node->token->type == PASCAL_TOKEN_TYPE_SHR) {
        *result = left >> right;

        if(!compiler->is_const) {
            if(left_is_const) {
                left_at_reg = pascal_compiler_find_free_reg(compiler);

                if(left_at_reg == -1) {
                    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "no free registers");
                    return -1;
                }

                left_reg = pascal_compiler_cast_reg_to_size(pascal_compiler_regs[left_at_reg], max_size);

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

            compiler->is_at_reg = true;
        }
    }else {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unknown binary op: %d", node->token->type);
        return -1;
    }

    buffer_printf(compiler->text_buffer, "# end binary op\n");

    return 0;
}

int8_t pascal_compiler_execute_compound(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result) {
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

    symbol_table->parent = compiler->current_symbol_table;
    compiler->current_symbol_table = symbol_table;

    for(size_t i = 0; i < linkedlist_size(node->children); i++) {
        pascal_ast_node_t * tmp_node = (pascal_ast_node_t*)linkedlist_get_data_at_position(node->children, i);

        if(tmp_node->type == PASCAL_AST_NODE_TYPE_DECLS) {
            for(size_t j = 0; j < linkedlist_size(tmp_node->children); j++) {
                pascal_ast_node_t * tmp_var_node = (pascal_ast_node_t*)linkedlist_get_data_at_position(tmp_node->children, j);

                for(size_t k = 0; k < linkedlist_size(tmp_var_node->children); k++) {
                    pascal_symbol_t * tmp_symbol = (pascal_symbol_t*)linkedlist_get_data_at_position(tmp_var_node->children, k);

                    if(strcmp(compiler->program_name, tmp_symbol->name) != 0 && hashmap_get(compiler->current_symbol_table->symbols, tmp_symbol->name) != NULL) {
                        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "symbol %s already defined", tmp_symbol->name);
                        return -1;
                    }

                    if(tmp_symbol->type == PASCAL_SYMBOL_TYPE_INTEGER) {
                        PRINTLOG(COMPILER_PASCAL, LOG_INFO, "local symbol %s type %d size %lli value %lli", tmp_symbol->name, tmp_symbol->type, tmp_symbol->size, tmp_symbol->int_value);

                        tmp_symbol->stack_offset = compiler->next_stack_offset;
                        compiler->next_stack_offset += 8; // tmp_symbol->size;
                        compiler->stack_size += 8; // tmp_symbol->size;
                    }

                    hashmap_put(compiler->current_symbol_table->symbols, tmp_symbol->name, tmp_symbol);
                }

            }
        }
    }

    if(pascal_compiler_build_stack(compiler) != 0) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot build stack");
        return -1;
    }

    for(size_t i = 0; i < linkedlist_size(node->children); i++) {
        pascal_ast_node_t * tmp_node = (pascal_ast_node_t*)linkedlist_get_data_at_position(node->children, i);

        if(tmp_node->type != PASCAL_AST_NODE_TYPE_DECLS && pascal_compiler_execute_ast_node(compiler, tmp_node, result) != 0) {
            return -1;
        }
    }

    compiler->current_symbol_table = compiler->current_symbol_table->parent;

    hashmap_destroy(symbol_table->symbols);
    memory_free(symbol_table);

    return 0;
}

int8_t pascal_compiler_save_to_mem(pascal_compiler_t* compiler, const char_t* reg, const pascal_symbol_t* symbol) {
    int16_t swap_reg = pascal_compiler_find_free_reg(compiler);
    boolean_t need_swap = false;

    if(swap_reg == -1) {
        need_swap = true;
        swap_reg = 8;
        buffer_printf(compiler->text_buffer, "\tpush %%%s\n", pascal_compiler_regs[swap_reg]);
    }

    buffer_printf(compiler->text_buffer, "\tmov $%s@GOT, %%%s\n", symbol->name, pascal_compiler_regs[swap_reg]);
    buffer_printf(compiler->text_buffer, "\tmov (%%r15, %%%s), %%%s\n", pascal_compiler_regs[swap_reg], pascal_compiler_regs[swap_reg]);

    buffer_printf(compiler->text_buffer, "# save %s to %s cs %lli ss %lli\n", reg, symbol->name, compiler->computed_size, symbol->size);

    if(compiler->computed_size != symbol->size) {
        buffer_printf(compiler->text_buffer, "# cast %s to %lli\n", reg, symbol->size);
        buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                      pascal_compiler_cast_reg_to_size(reg, compiler->computed_size),
                      pascal_compiler_cast_reg_to_size(reg, symbol->size));
    }

    buffer_printf(compiler->text_buffer, "\tmov%c %%%s, (%%%s)\n",
                  pascal_compiler_get_reg_suffix(symbol->size),
                  pascal_compiler_cast_reg_to_size(reg, symbol->size),
                  pascal_compiler_regs[swap_reg]);

    if(need_swap) {
        buffer_printf(compiler->text_buffer, "\tpop %%%s\n", pascal_compiler_regs[swap_reg]);
    } else {
        compiler->busy_regs[swap_reg] = false;
    }

    return 0;
}

int8_t pascal_compiler_save_const_int_to_mem(pascal_compiler_t* compiler, int64_t value, const pascal_symbol_t* symbol) {
    int16_t swap_reg = pascal_compiler_find_free_reg(compiler);
    boolean_t need_swap = false;

    if(swap_reg == -1) {
        need_swap = true;
        swap_reg = 8;
        buffer_printf(compiler->text_buffer, "\tpush %%%s\n", pascal_compiler_regs[swap_reg]);
    }

    buffer_printf(compiler->text_buffer, "\tmov $%s@GOT, %%%s\n", symbol->name, pascal_compiler_regs[swap_reg]);
    buffer_printf(compiler->text_buffer, "\tmov (%%r15, %%%s), %%%s\n", pascal_compiler_regs[swap_reg], pascal_compiler_regs[swap_reg]);
    buffer_printf(compiler->text_buffer, "\tmov%c $0x%llx, (%%%s)\n",
                  pascal_compiler_get_reg_suffix(symbol->size),
                  value,
                  pascal_compiler_regs[swap_reg]);

    if(need_swap) {
        buffer_printf(compiler->text_buffer, "\tpop %%%s\n", pascal_compiler_regs[swap_reg]);
    } else {
        compiler->busy_regs[swap_reg] = false;
    }

    return 0;
}

int8_t pascal_compiler_execute_assign(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result) {
    const pascal_symbol_t * symbol = pascal_compiler_find_symbol(compiler, node->left->token->text);

    if(symbol == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "symbol %s not found", node->left->token->text);
        return -1;
    }

    buffer_printf(compiler->text_buffer, "# begin assign %s\n", symbol->name);

    buffer_printf(compiler->text_buffer, "# begin get right\n");

    if(pascal_compiler_execute_ast_node(compiler, node->right, result) != 0) {
        return -1;
    }

    buffer_printf(compiler->text_buffer, "# end get right\n");

    ((pascal_symbol_t*)symbol)->int_value = *result;

    if(compiler->is_at_mem) {
        if(symbol->is_local) {
            buffer_printf(compiler->text_buffer, "\tmov%c %s, -%d(%%rbp)\n",
                          pascal_compiler_get_reg_suffix(symbol->size),
                          node->right->token->text,
                          symbol->stack_offset);
        } else {
            int16_t swap_reg = pascal_compiler_find_free_reg(compiler);
            boolean_t need_swap = false;

            if(swap_reg == -1) {
                need_swap = true;
                swap_reg = 8;
                buffer_printf(compiler->text_buffer, "\tpush %%%s\n", pascal_compiler_regs[swap_reg]);
            }

            buffer_printf(compiler->text_buffer, "\tmov%c %s, %%%s\n",
                          pascal_compiler_get_reg_suffix(symbol->size),
                          node->right->token->text,
                          pascal_compiler_cast_reg_to_size(pascal_compiler_regs[swap_reg], symbol->size));

            pascal_compiler_save_to_mem(compiler, pascal_compiler_regs[swap_reg], symbol);

            if(need_swap) {
                buffer_printf(compiler->text_buffer, "\tpop %%%s\n", pascal_compiler_regs[swap_reg]);
            } else {
                compiler->busy_regs[swap_reg] = false;
            }
        }

    } else if(compiler->is_at_reg) {
        if(symbol->is_local) {
            buffer_printf(compiler->text_buffer, "\tmov%c %%%s, -%d(%%rbp)\n",
                          pascal_compiler_get_reg_suffix(symbol->size),
                          pascal_compiler_cast_reg_to_size(pascal_compiler_regs[node->right->used_register], symbol->size),
                          symbol->stack_offset);
        } else {
            pascal_compiler_save_to_mem(compiler, pascal_compiler_regs[node->right->used_register], symbol);
        }

        compiler->is_at_reg = false;
        compiler->busy_regs[node->right->used_register] = false;
    } else if(compiler->is_const) {
        if(symbol->is_local) {
            buffer_printf(compiler->text_buffer, "\tmov%c $0x%llx, -%d(%%rbp)\n",
                          pascal_compiler_get_reg_suffix(symbol->size),
                          *result,
                          symbol->stack_offset);
        } else {
            pascal_compiler_save_const_int_to_mem(compiler, *result, symbol);
        }

        compiler->is_const = false;
    } else if(compiler->is_at_stack) {
        int16_t swap_reg = pascal_compiler_find_free_reg(compiler);
        boolean_t need_swap = false;

        if(swap_reg == -1) {
            need_swap = true;
            swap_reg = 8;
            buffer_printf(compiler->text_buffer, "\tpush %%%s\n", pascal_compiler_regs[swap_reg]);
        }

        buffer_printf(compiler->text_buffer, "\tmov%c -%d(%%rbp), %%%s\n",
                      pascal_compiler_get_reg_suffix(symbol->size),
                      compiler->at_stack_offset,
                      pascal_compiler_cast_reg_to_size(pascal_compiler_regs[swap_reg], symbol->size));

        if(symbol->is_local) {
            buffer_printf(compiler->text_buffer, "\tmov%c %%%s, -%d(%%rbp)\n",
                          pascal_compiler_get_reg_suffix(symbol->size),
                          pascal_compiler_cast_reg_to_size(pascal_compiler_regs[swap_reg], symbol->size),
                          symbol->stack_offset);
        } else {
            pascal_compiler_save_to_mem(compiler, pascal_compiler_regs[swap_reg], symbol);
        }

        if(need_swap) {
            buffer_printf(compiler->text_buffer, "\tpop %%%s\n", pascal_compiler_regs[swap_reg]);
        } else {
            compiler->busy_regs[swap_reg] = false;
        }


        compiler->is_at_stack = false;
    } else {
        int16_t swap_reg = pascal_compiler_find_free_reg(compiler);
        boolean_t need_swap = false;

        if(swap_reg == -1) {
            need_swap = true;
            swap_reg = 8;
            buffer_printf(compiler->text_buffer, "\tpush %%%s\n", pascal_compiler_regs[swap_reg]);
        }

        buffer_printf(compiler->text_buffer, "\tpop %%%s\n", pascal_compiler_regs[swap_reg]);

        if(symbol->is_local) {
            buffer_printf(compiler->text_buffer, "\tmov%c %%%s, -%d(%%rbp)\n",
                          pascal_compiler_get_reg_suffix(symbol->size),
                          pascal_compiler_cast_reg_to_size(pascal_compiler_regs[swap_reg], symbol->size),
                          symbol->stack_offset);
        } else {
            pascal_compiler_save_to_mem(compiler, pascal_compiler_regs[swap_reg], symbol);
        }

        if(need_swap) {
            buffer_printf(compiler->text_buffer, "\tpop %%%s\n", pascal_compiler_regs[swap_reg]);
        } else {
            compiler->busy_regs[swap_reg] = false;
        }

    }

    buffer_printf(compiler->text_buffer, "# end assign %s\n", symbol->name);

    return 0;
}

int8_t pascal_compiler_execute_string_const(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result) {
    UNUSED(result);

    pascal_symbol_t * symbol = memory_malloc(sizeof(pascal_symbol_t));

    if (symbol == NULL) {
        return -1;
    }

    symbol->name = sprintf(".L%i", compiler->next_label_id++);
    symbol->type = PASCAL_SYMBOL_TYPE_STRING;
    symbol->size = strlen(node->token->text) + 1;
    symbol->is_const = true;
    symbol->string_value = node->token->text;

    node->symbol = symbol;

    buffer_printf(compiler->rodata_buffer, ".section .rodata.%s\n", symbol->name);
    buffer_printf(compiler->rodata_buffer, ".align 8\n");
    buffer_printf(compiler->rodata_buffer, ".local %s\n", symbol->name);
    buffer_printf(compiler->rodata_buffer, ".type %s, @object\n", symbol->name);
    buffer_printf(compiler->rodata_buffer, ".size %s, %lli\n", symbol->name, symbol->size);
    buffer_printf(compiler->rodata_buffer, "%s:\n", symbol->name);
    buffer_printf(compiler->rodata_buffer, "\t.string \"%s\"\n", symbol->string_value);
    buffer_printf(compiler->rodata_buffer, "\n\n\n");

    hashmap_put(compiler->main_symbol_table->symbols, symbol->name, symbol);

    return 0;
}

#define SYS_exit 60ULL
#define SYS_write 1ULL

int8_t pascal_compiler_execute_function_call(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result) {
    buffer_printf(compiler->text_buffer, "# function call %s\n", node->token->text);

    if(strcmp(node->token->text, "write") == 0) {
        if(linkedlist_size(node->children) != 1) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "write expects 1 argument");
            return -1;
        }

        uint64_t stack_push_size = 0;

        boolean_t rdi_pushed = false;
        boolean_t rsi_pushed = false;
        boolean_t rdx_pushed = false;
        boolean_t rcx_pushed = false;

        boolean_t stack_need_align = false;



        if(compiler->busy_regs[PASCAL_VM_REG_RCX]) {
            buffer_printf(compiler->text_buffer, "\tpush %%rcx\n");
            rcx_pushed = true;
            stack_push_size += 8;
        }

        if(compiler->busy_regs[PASCAL_VM_REG_RDI]) {
            buffer_printf(compiler->text_buffer, "\tpush %%rdi\n");
            rdi_pushed = true;
            stack_push_size += 8;
        }

        if(compiler->busy_regs[PASCAL_VM_REG_RSI]) {
            buffer_printf(compiler->text_buffer, "\tpush %%rsi\n");
            rsi_pushed = true;
            stack_push_size += 8;
        }

        if(compiler->busy_regs[PASCAL_VM_REG_RDX]) {
            buffer_printf(compiler->text_buffer, "\tpush %%rdx\n");
            rdx_pushed = true;
            stack_push_size += 8;
        }

        if(stack_push_size % 16) {
            stack_need_align = true;
        }

        buffer_printf(compiler->text_buffer, "\tmov $1, %%rdi\n");

        pascal_ast_node_t* tmp_node = (pascal_ast_node_t*)linkedlist_get_data_at_position(node->children, 0);

        if(pascal_compiler_execute_ast_node(compiler, tmp_node, result) != 0) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot execute write");
            return -1;
        }

        if(tmp_node->type == PASCAL_AST_NODE_TYPE_STRING_CONST) {
            buffer_printf(compiler->text_buffer, "\tmov $%s@GOT, %%rsi\n", tmp_node->symbol->name);
            buffer_printf(compiler->text_buffer, "\tmov (%%r15, %%rsi), %%rsi\n");

            buffer_printf(compiler->text_buffer, "\tmov $%lli, %%rdx\n", tmp_node->symbol->size);
        } else if(tmp_node->type == PASCAL_AST_NODE_TYPE_VAR) {
            const pascal_symbol_t * symbol = pascal_compiler_find_symbol(compiler, tmp_node->token->text);

            if(symbol == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "symbol %s not found", tmp_node->token->text);
                return -1;
            }


            // TODO: check type -> integer probably pointer to string
            if(symbol->type == PASCAL_SYMBOL_TYPE_INTEGER) {
                if(symbol->is_local) {
                    buffer_printf(compiler->text_buffer, "\tmov -%d(%%rbp), %%rsi\n", symbol->stack_offset);
                } else {
                    buffer_printf(compiler->text_buffer, "\tmov $%s@GOT, %%rsi\n", symbol->name);
                    buffer_printf(compiler->text_buffer, "\tmov (%%r15, %%rsi), %%rsi\n");
                    buffer_printf(compiler->text_buffer, "\tmov (%%rsi), %%rsi\n");
                }
            } else {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot write symbol %s type %d", symbol->name, symbol->type);
                return -1;
            }
        } else {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot write node type %d", tmp_node->type);
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
        node->used_register = PASCAL_VM_REG_RAX;
        compiler->busy_regs[PASCAL_VM_REG_RAX] = true;


        return 0;
    }

    return -1;
}


int8_t pascal_compiler_execute_ast_node(pascal_compiler_t* compiler, pascal_ast_node_t* node, int64_t* result) {
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

        hashmap_put(compiler->current_symbol_table->symbols, symbol->name, symbol);

        compiler->program_name = symbol->name;
        compiler->program_name_symbol = symbol;

        buffer_printf(compiler->text_buffer, "\tmov $%s@GOT, %%rax\n", symbol->name);
        buffer_printf(compiler->text_buffer, "\tcall *(%%r15,%%rax)\n");
        buffer_printf(compiler->text_buffer, "\tmov %%rax, %%rdi\n");
        buffer_printf(compiler->text_buffer, "\tmov $0x%llx, %%rax\n", SYS_exit);
        buffer_printf(compiler->text_buffer, "\tsyscall\n");
        buffer_printf(compiler->text_buffer, ".size _start, .-_start\n");

        buffer_printf(compiler->text_buffer, "\n\n\n");
        buffer_printf(compiler->text_buffer, ".section .text.%s\n", symbol->name);
        buffer_printf(compiler->text_buffer, ".local %s\n", symbol->name);
        buffer_printf(compiler->text_buffer, ".type %s, @function\n", symbol->name);
        buffer_printf(compiler->text_buffer, "%s:\n", symbol->name);

        buffer_t* tmp_buffer = compiler->text_buffer;

        compiler->text_buffer = buffer_new();

        if(pascal_compiler_execute_ast_node(compiler, node->left, result) != 0) {
            buffer_destroy(compiler->text_buffer);
            compiler->text_buffer = tmp_buffer;
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot execute program %s", symbol->name);

            return -1;
        }

        buffer_printf(tmp_buffer, "\tenter $%d, $0\n", compiler->stack_size);

        buffer_printf(tmp_buffer, "%s", buffer_get_view_at_position(compiler->text_buffer, 0, buffer_get_length(compiler->text_buffer)));

        buffer_destroy(compiler->text_buffer);

        compiler->text_buffer = tmp_buffer;

        buffer_printf(compiler->text_buffer, "\tmov -8(%%rbp), %%rax\n");
        buffer_printf(compiler->text_buffer, "\tleave\n");
        buffer_printf(compiler->text_buffer, "\tret\n");
        buffer_printf(compiler->text_buffer, ".size %s, .-%s\n", symbol->name, symbol->name);
        buffer_printf(compiler->text_buffer, "\n\n\n");


        return 0;
    } else if(node->type == PASCAL_AST_NODE_TYPE_BLOCK) {
        return pascal_compiler_execute_block(compiler, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_COMPOUND) {
        return pascal_compiler_execute_compound(compiler, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_ASSIGN) {
        return pascal_compiler_execute_assign(compiler, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_VAR) {
        const pascal_symbol_t * symbol = pascal_compiler_find_symbol(compiler, node->token->text);

        if(symbol == NULL) {
            PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "symbol %s not found", node->token->text);
            return -1;
        }

        buffer_printf(compiler->text_buffer, "# begin load var %s\n", symbol->name);

        *result = symbol->int_value;

        compiler->is_at_reg = false;
        compiler->is_at_mem = false;
        compiler->is_at_stack = false;
        compiler->is_const = false;

        if(symbol->type == PASCAL_SYMBOL_TYPE_INTEGER) {
            compiler->computed_size = symbol->size;
            compiler->computed_type = symbol->type;

            if(symbol->is_local) {
                compiler->is_at_stack = true;
                compiler->at_stack_offset = symbol->stack_offset;
            } else {
                int16_t reg = pascal_compiler_find_free_reg(compiler);

                if(reg == -1) {
                    PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "no free register");
                    compiler->is_at_mem = true;
                } else {
                    compiler->is_at_reg = true;
                    node->used_register = reg;
                    buffer_printf(compiler->text_buffer, "\tmov $%s@GOT, %%%s\n", symbol->name, pascal_compiler_regs[reg]);
                    buffer_printf(compiler->text_buffer, "\tmov (%%r15, %%%s), %%%s\n", pascal_compiler_regs[reg], pascal_compiler_regs[reg]);
                    buffer_printf(compiler->text_buffer, "\tmov%c (%%%s), %%%s\n",
                                  pascal_compiler_get_reg_suffix(symbol->size),
                                  pascal_compiler_regs[reg],
                                  pascal_compiler_cast_reg_to_size(pascal_compiler_regs[reg], symbol->size));
                }
            }
        } else { // do we need this?
            compiler->is_at_reg = true;
        }

        buffer_printf(compiler->text_buffer, "# end load var %s\n", symbol->name);
    } else if (node->type == PASCAL_AST_NODE_TYPE_INTEGER_CONST) {
        *result = node->token->value;

        buffer_printf(compiler->text_buffer, "# begin load const %lli\n", *result);

        compiler->is_at_reg = false;
        compiler->is_at_mem = false;
        compiler->is_at_stack = false;
        compiler->is_const = true;

        compiler->computed_size = node->token->size;

        buffer_printf(compiler->text_buffer, "# const size %lli\n", compiler->computed_size);

        compiler->computed_type = PASCAL_SYMBOL_TYPE_INTEGER;

        buffer_printf(compiler->text_buffer, "# end load const %lli\n", *result);

    } else if(node->type == PASCAL_AST_NODE_TYPE_UNARY_OP) {
        return pascal_compiler_execure_unary_op(compiler, node, result);
    } else if (node->type == PASCAL_AST_NODE_TYPE_BINARY_OP) {
        return pascal_compiler_execure_binary_op(compiler, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_FUNCTION_CALL) {
        return pascal_compiler_execute_function_call(compiler, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_STRING_CONST) {
        return pascal_compiler_execute_string_const(compiler, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_IF) {
        return pascal_compiler_execute_if(compiler, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_RELATIONAL_OP) {
        return pascal_compiler_execute_relational_op(compiler, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_WHILE) {
        return pascal_compiler_execute_while(compiler, node, result);
    } else if(node->type == PASCAL_AST_NODE_TYPE_REPEAT) {
        return pascal_compiler_execute_repeat(compiler, node, result);
    } else {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unknown node type %d", node->type);
        return -1;
    }

    return 0;
}

int8_t pascal_compiler_execute(pascal_compiler_t * compiler, int64_t* result) {
    pascal_ast_node_t * node = compiler->ast->root;

    if (node == NULL) {
        return -1;
    }

    buffer_printf(compiler->text_buffer, ".section .text\n");
    buffer_printf(compiler->data_buffer, ".section .data\n");
    buffer_printf(compiler->rodata_buffer, ".section .rodata\n");
    buffer_printf(compiler->bss_buffer, ".section .bss\n");

    buffer_printf(compiler->text_buffer, ".section .text._start\n");
    buffer_printf(compiler->text_buffer, ".globl _start\n");
    buffer_printf(compiler->text_buffer, ".type _start, @function\n");
    buffer_printf(compiler->text_buffer, "_start:\n");
    buffer_printf(compiler->text_buffer, "\tpush %%rbp\n");
    buffer_printf(compiler->text_buffer, "\tmov %%rsp, %%rbp\n");
    buffer_printf(compiler->text_buffer, "\tlea 0x0(%%rip), %%r14\n");
    buffer_printf(compiler->text_buffer, ".LGOT_BASE:\n");
    buffer_printf(compiler->text_buffer, "\tmov $_GLOBAL_OFFSET_TABLE_-.LGOT_BASE, %%r15\n");
    buffer_printf(compiler->text_buffer, "\tadd %%r14, %%r15\n");
    buffer_printf(compiler->text_buffer, "\txor %%rax, %%rax\n");


    int8_t res = pascal_compiler_execute_ast_node(compiler, node, result);

    buffer_printf(compiler->text_buffer, "%s", buffer_get_view_at_position(compiler->data_buffer, 0, buffer_get_length(compiler->data_buffer)));
    buffer_printf(compiler->text_buffer, "%s", buffer_get_view_at_position(compiler->rodata_buffer, 0, buffer_get_length(compiler->rodata_buffer)));
    buffer_printf(compiler->text_buffer, "%s", buffer_get_view_at_position(compiler->bss_buffer, 0, buffer_get_length(compiler->bss_buffer)));

    return res;
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

    pascal_compiler_t compiler = {0};

    if(pascal_compiler_init(&compiler, &ast) != 0) {
        print_error("Error: Failed to initialize compiler\n");

        return -1;
    }

    int64_t result = 0;

    int8_t res = 0;

    if(pascal_compiler_execute(&compiler, &result) != 0) {
        print_error("Error: Failed to execute compiler\n");

        res = -1;
    }

    FILE* out = fopen(argv[2], "w");

    if (out == NULL) {
        print_error("Error: Failed to open output file\n");

        pascal_compiler_destroy(&compiler);

        return -1;
    }

    size_t out_size = 0;

    uint8_t* out_bytes = buffer_get_all_bytes(compiler.text_buffer, &out_size);

    fwrite(out_bytes, 1, out_size, out);

    fclose(out);

    memory_free(out_bytes);

    pascal_compiler_destroy(&compiler);

    PRINTLOG(COMPILER_PASCAL, LOG_INFO, "Success");

    return res;
}
