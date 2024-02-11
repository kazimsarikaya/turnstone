/**
 * @file compiler_load.64.c
 * @brief
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>
#include <utils.h>

MODULE("turnstone.compiler.codegen");

int8_t compiler_execute_load_int(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t compiler_execute_load_var(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);


int8_t compiler_execute_load_int(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    *result = node->token->value;

    buffer_printf(compiler->text_buffer, "# begin load const %lli\n", *result);

    node->is_at_reg = false;
    node->is_const = true;

    node->computed_size = node->token->size;

    buffer_printf(compiler->text_buffer, "# const size %lli\n", node->computed_size);

    node->computed_type = COMPILER_SYMBOL_TYPE_INTEGER;

    buffer_printf(compiler->text_buffer, "# end load const %lli\n", *result);

    return 0;
}

int8_t compiler_execute_load_var(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    if(compiler_var_resolver(compiler, node, result) != 0) {
        PRINTLOG(COMPILER, LOG_ERROR, "cannot resolve var");
        return -1;
    }

    if(!node->computed_is_scalar && !node->computed_is_array) {
        buffer_printf(compiler->text_buffer, "\tmov%c (%%%s), %%%s\n",
                      compiler_get_reg_suffix(node->computed_size),
                      compiler_regs[node->used_register],
                      compiler_cast_reg_to_size(compiler_regs[node->used_register], node->computed_size));
    }

    return 0;
}

int8_t compiler_execute_load(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    if(node->type == COMPILER_AST_NODE_TYPE_INTEGER_CONST) {
        return compiler_execute_load_int(compiler, node, result);
    }

    if(node->type == COMPILER_AST_NODE_TYPE_STRING_CONST) {
        return compiler_execute_string_const(compiler, node, result);
    }

    if(node->type == COMPILER_AST_NODE_TYPE_VAR) {
        return compiler_execute_load_var(compiler, node, result);
    }

    PRINTLOG(COMPILER, LOG_ERROR, "Invalid node type for load");
    return -1;
}
