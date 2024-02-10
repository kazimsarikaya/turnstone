/**
 * @file compiler.64.c
 * @brief main compiler functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>

MODULE("turnstone.compiler");

int8_t compiler_execute_ast_node(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    if (node == NULL) {
        return -1;
    }

    if(node->type == COMPILER_AST_NODE_TYPE_NO_OP) {
        return 0;
    } else if(node->type == COMPILER_AST_NODE_TYPE_PROGRAM) {
        compiler_symbol_t * symbol = memory_malloc(sizeof(compiler_symbol_t));

        if (symbol == NULL) {
            return -1;
        }

        symbol->name = node->token->text;
        symbol->type = COMPILER_SYMBOL_TYPE_INTEGER;
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

        if(compiler_execute_ast_node(compiler, node->left, result) != 0) {
            buffer_destroy(compiler->text_buffer);
            compiler->text_buffer = tmp_buffer;
            PRINTLOG(COMPILER, LOG_ERROR, "cannot execute program %s", symbol->name);

            return -1;
        }

        if(compiler->stack_size % 16) {
            compiler->stack_size += 16 - (compiler->stack_size % 16);
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
    } else if(node->type == COMPILER_AST_NODE_TYPE_BLOCK) {
        return compiler_execute_block(compiler, node, result);
    } else if(node->type == COMPILER_AST_NODE_TYPE_COMPOUND) {
        return compiler_execute_compound(compiler, node, result);
    } else if(node->type == COMPILER_AST_NODE_TYPE_ASSIGN) {
        return compiler_execute_save(compiler, node, result);
    } else if(node->type == COMPILER_AST_NODE_TYPE_VAR ||
              node->type == COMPILER_AST_NODE_TYPE_INTEGER_CONST ||
              node->type == COMPILER_AST_NODE_TYPE_STRING_CONST) {
        return compiler_execute_load(compiler, node, result);
    } else if(node->type == COMPILER_AST_NODE_TYPE_UNARY_OP) {
        return compiler_execute_unary_op(compiler, node, result);
    } else if (node->type == COMPILER_AST_NODE_TYPE_BINARY_OP) {
        return compiler_execute_binary_op(compiler, node, result);
    } else if(node->type == COMPILER_AST_NODE_TYPE_FUNCTION_CALL) {
        return compiler_execute_function_call(compiler, node, result);
    } else if(node->type == COMPILER_AST_NODE_TYPE_IF) {
        return compiler_execute_if(compiler, node, result);
    } else if(node->type == COMPILER_AST_NODE_TYPE_RELATIONAL_OP) {
        return compiler_execute_relational_op(compiler, node, result);
    } else if(node->type == COMPILER_AST_NODE_TYPE_WHILE) {
        return compiler_execute_while(compiler, node, result);
    } else if(node->type == COMPILER_AST_NODE_TYPE_REPEAT) {
        return compiler_execute_repeat(compiler, node, result);
    } else if(node->type == COMPILER_AST_NODE_TYPE_WITH) {
        return compiler_execute_with(compiler, node, result);
    } else {
        PRINTLOG(COMPILER, LOG_ERROR, "unknown node type %d", node->type);
        return -1;
    }

    return 0;
}



int8_t compiler_execute(compiler_t * compiler, int64_t* result) {
    compiler_ast_node_t * node = compiler->ast->root;

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
    buffer_printf(compiler->text_buffer, "\tsub $8, %%rsp\n");
    buffer_printf(compiler->text_buffer, "\tlea 0x0(%%rip), %%r14\n");
    buffer_printf(compiler->text_buffer, ".LGOT_BASE:\n");
    buffer_printf(compiler->text_buffer, "\tmov $_GLOBAL_OFFSET_TABLE_-.LGOT_BASE, %%r15\n");
    buffer_printf(compiler->text_buffer, "\tadd %%r14, %%r15\n");
    buffer_printf(compiler->text_buffer, "\txor %%rax, %%rax\n");


    int8_t res = compiler_execute_ast_node(compiler, node, result);

    buffer_printf(compiler->text_buffer, "%s", buffer_get_view_at_position(compiler->data_buffer, 0, buffer_get_length(compiler->data_buffer)));
    buffer_printf(compiler->text_buffer, "%s", buffer_get_view_at_position(compiler->rodata_buffer, 0, buffer_get_length(compiler->rodata_buffer)));
    buffer_printf(compiler->text_buffer, "%s", buffer_get_view_at_position(compiler->bss_buffer, 0, buffer_get_length(compiler->bss_buffer)));

    return res;
}

int8_t compiler_init(compiler_t * compiler, compiler_ast_t * ast) {
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

    compiler_symbol_table_t* symbol_table = memory_malloc(sizeof(compiler_symbol_table_t));

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

int8_t compiler_destroy(compiler_t * compiler) {
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

    if(compiler->types_by_id != NULL) {
        hashmap_destroy(compiler->types_by_id);
    }

    if(compiler->types_by_name != NULL) {
        hashmap_destroy(compiler->types_by_name);
    }

    linkedlist_destroy(compiler->cond_label_stack);
    linkedlist_destroy(compiler->loop_label_stack);

    compiler_destroy_symbol_table(compiler);

    memory_free(compiler->program_name_symbol);

    compiler_ast_destroy(compiler->ast);
    return 0;
}

