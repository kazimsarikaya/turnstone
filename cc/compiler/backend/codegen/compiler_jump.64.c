/**
 * @file compiler_jump.64.c
 * @brief goto and label code generation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>
#include <utils.h>

MODULE("turnstone.compiler.codegen");

int8_t compiler_execute_goto(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    UNUSED(result);

    buffer_printf(compiler->text_buffer, "# begin goto %s\n", node->token->text);

    buffer_printf(compiler->text_buffer, "\tjmp %s\n", node->token->text);

    buffer_printf(compiler->text_buffer, "# end goto %s\n", node->token->text);

    return 0;
}

int8_t compiler_execute_label(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    UNUSED(result);

    buffer_printf(compiler->text_buffer, "# begin label %s\n", node->token->text);

    buffer_printf(compiler->text_buffer, "%s:\n", node->token->text);

    buffer_printf(compiler->text_buffer, "# end label %s\n", node->token->text);

    return 0;
}

