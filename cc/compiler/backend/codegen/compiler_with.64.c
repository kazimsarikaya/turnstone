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

int8_t compiler_execute_with(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    UNUSED(result);

    if(node->left == NULL) {
        return 0;
    }

    compiler->with_prefix = node->token;

    if(compiler_execute_ast_node(compiler, node->left, result) != 0) {
        PRINTLOG(COMPILER, LOG_ERROR, "cannot execute with prefix");
        compiler->with_prefix = NULL;
        return -1;
    }

    compiler->with_prefix = NULL;

    return 0;
}



