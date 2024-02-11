/**
 * @file compiler_var_resolver.64.c
 * @brief Resolves variables and saves the result mostly returns memory address for load and save operations.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>

MODULE("turnstone.compiler.codegen");

int8_t compiler_var_resolver(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    UNUSED(result);

    if(node->type != COMPILER_AST_NODE_TYPE_VAR) {
        PRINTLOG(COMPILER, LOG_ERROR, "node type is not var");
        return -1;
    }

    int16_t used_register;
    const compiler_symbol_t* symbol;
    int64_t computed_size;
    compiler_symbol_type_t computed_type;
    compiler_symbol_type_t computed_hidden_type;
    int64_t computed_custom_type_id;

    used_register = compiler_find_free_reg(compiler);

    if(used_register < 0) {
        PRINTLOG(COMPILER, LOG_ERROR, "no free register found");
        return -1;
    }

    compiler->busy_regs[used_register] = true;

    if(node->token->type != COMPILER_TOKEN_TYPE_ID) {
        PRINTLOG(COMPILER, LOG_ERROR, "type of token %s is not identifier: %d", node->token->text, node->token->type);
        return -1;
    }

    symbol = compiler_find_symbol(compiler, node->token->text);

    if(symbol == NULL) {
        PRINTLOG(COMPILER, LOG_ERROR, "symbol %s not found", node->token->text);
        return -1;
    }

    computed_size = symbol->size;
    computed_type = symbol->type;
    computed_hidden_type = symbol->hidden_type;
    computed_custom_type_id = symbol->custom_type_id;
    boolean_t computed_is_array = symbol->is_array;
    boolean_t computed_is_scalar = false;

    if(!symbol->is_local) {
        buffer_printf(compiler->text_buffer, "\tmov $%s@GOT, %%%s\n", symbol->name, compiler_regs[used_register]);
        buffer_printf(compiler->text_buffer, "\tmov (%%r15, %%%s), %%%s\n", compiler_regs[used_register], compiler_regs[used_register]);
    } else {
        if(computed_hidden_type == COMPILER_SYMBOL_TYPE_STRING) {
            buffer_printf(compiler->text_buffer, "\tmov -%d(%%rbp), %%%s\n", symbol->stack_offset, compiler_regs[used_register]);
        } else {
            buffer_printf(compiler->text_buffer, "\tlea -%d(%%rbp), %%%s\n", symbol->stack_offset, compiler_regs[used_register]);
        }
    }

    compiler_ast_node_t* tmp_node = node->right;

    while(tmp_node) {
        if(tmp_node->type == COMPILER_AST_NODE_TYPE_FUNCTION_CALL) {
            if(used_register != COMPILER_VM_REG_RBX) {
                buffer_printf(compiler->text_buffer, "\txchg %%%s, %%%s\n", compiler_regs[used_register], compiler_regs[COMPILER_VM_REG_RBX]);
            }

            boolean_t old_rbx_state = compiler->busy_regs[COMPILER_VM_REG_RBX];
            compiler->busy_regs[COMPILER_VM_REG_RBX] = true;

            int64_t dummy = 0;

            if(compiler_execute_function_call(compiler, tmp_node, &dummy) == -1) {
                PRINTLOG(COMPILER, LOG_ERROR, "cannot execute function call %s", tmp_node->token->text);
                return -1;
            }

            compiler->busy_regs[COMPILER_VM_REG_RBX] = old_rbx_state;

            if(used_register != COMPILER_VM_REG_RBX) {
                buffer_printf(compiler->text_buffer, "\txchg %%%s, %%%s\n", compiler_regs[used_register], compiler_regs[COMPILER_VM_REG_RBX]);
            }

            compiler->busy_regs[used_register] = false;
            used_register = tmp_node->used_register;
            // TODO: build node's computed size, type, hidden type and custom type id based on function call return type
            // now assume 64 bit integer
            computed_size = 64;
            computed_type = COMPILER_SYMBOL_TYPE_INTEGER;
            computed_hidden_type = COMPILER_SYMBOL_TYPE_UNKNOWN;
            computed_custom_type_id = -1;
            computed_is_array = false;
            computed_is_scalar = true;
            // function call now only is the last node
            break;
        } else if(tmp_node->type == COMPILER_AST_NODE_TYPE_ARRAY_SUBSCRIPT) {
            if(!computed_is_array) {
                PRINTLOG(COMPILER, LOG_ERROR, "symbol is not array cannot be subscripted");
                return -1;
            }

            if(tmp_node->left == NULL) {
                PRINTLOG(COMPILER, LOG_ERROR, "left node is null");
                return -1;
            }

            int64_t const_array_idx = 0;

            if(compiler_execute_ast_node(compiler, tmp_node->left, &const_array_idx) != 0) {
                PRINTLOG(COMPILER, LOG_ERROR, "cannot execute array index");
                return -1;
            }

            if(tmp_node->left->computed_type != COMPILER_SYMBOL_TYPE_INTEGER) {
                PRINTLOG(COMPILER, LOG_ERROR, "array index is not integer");
                return -1;
            }

            int64_t real_compued_size = computed_size;

            if(computed_type == COMPILER_SYMBOL_TYPE_INTEGER) {
                real_compued_size /= 8;
            }

            if(tmp_node->left->is_const){
                real_compued_size *= const_array_idx;
                buffer_printf(compiler->text_buffer, "\tlea %lli(%%%s), %%%s\n",
                              real_compued_size,
                              compiler_regs[used_register],
                              compiler_regs[used_register]);
            } else {
                if(tmp_node->left->computed_size != 64) {
                    buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                                  compiler_cast_reg_to_size(compiler_regs[tmp_node->left->used_register], tmp_node->left->computed_size),
                                  compiler_regs[tmp_node->left->used_register]);
                }

                if(computed_type == COMPILER_SYMBOL_TYPE_INTEGER) {
                    buffer_printf(compiler->text_buffer, "\tlea (%%%s, %%%s, %lli), %%%s\n",
                                  compiler_regs[used_register],
                                  compiler_regs[tmp_node->left->used_register],
                                  real_compued_size,
                                  compiler_regs[used_register]);
                } else if(computed_type == COMPILER_SYMBOL_TYPE_CUSTOM) {
                    buffer_printf(compiler->text_buffer, "\timul $%lli, %%%s\n",
                                  real_compued_size,
                                  compiler_regs[tmp_node->left->used_register]);
                    buffer_printf(compiler->text_buffer, "\tlea (%%%s, %%%s), %%%s\n",
                                  compiler_regs[used_register],
                                  compiler_regs[tmp_node->left->used_register],
                                  compiler_regs[used_register]);
                } else {
                    PRINTLOG(COMPILER, LOG_ERROR, "unknown type");
                    return -1;
                }
            }

            compiler->busy_regs[tmp_node->left->used_register] = false;
            computed_is_array = false;
        } else if(tmp_node->type == COMPILER_AST_NODE_TYPE_VAR) {
            if(computed_type != COMPILER_SYMBOL_TYPE_CUSTOM) {
                PRINTLOG(COMPILER, LOG_ERROR, "symbol is not custom type");
                return -1;
            }

            if(tmp_node->token->type != COMPILER_TOKEN_TYPE_ID) {
                PRINTLOG(COMPILER, LOG_ERROR, "token type is not identifier");
                return -1;
            }

            const compiler_type_t* custom_type = hashmap_get(compiler->types_by_id, (void*)computed_custom_type_id);

            if(custom_type == NULL) {
                PRINTLOG(COMPILER, LOG_ERROR, "custom type not found");
                return -1;
            }

            const compiler_type_field_t* field = hashmap_get(custom_type->field_map, tmp_node->token->text);

            if(field == NULL) {
                PRINTLOG(COMPILER, LOG_ERROR, "field not found");
                return -1;
            }

            computed_size = field->symbol_size;
            computed_type = field->symbol_type;
            computed_hidden_type = field->symbol_hidden_type;
            computed_custom_type_id = field->symbol_custom_type_id;
            computed_is_array = field->symbol_is_array;


            if(field->offset != 0) {
                buffer_printf(compiler->text_buffer, "\tlea %lli(%%%s), %%%s\n",
                              field->offset,
                              compiler_regs[used_register],
                              compiler_regs[used_register]);
            }

        } else {
            PRINTLOG(COMPILER, LOG_ERROR, "unknown node type %d", tmp_node->type);

            if(tmp_node->token != NULL) {
                PRINTLOG(COMPILER, LOG_ERROR, "token type %d", tmp_node->token->type);
                PRINTLOG(COMPILER, LOG_ERROR, "token text %s", tmp_node->token->text);
            } else {
                PRINTLOG(COMPILER, LOG_ERROR, "token is null");
            }

            return -1;
        }



        tmp_node = tmp_node->right;
    }

    node->is_at_reg = true;
    node->used_register = used_register;
    node->computed_size = computed_size;
    node->computed_type = computed_type;
    node->computed_hidden_type = computed_hidden_type;
    node->computed_custom_type_id = computed_custom_type_id;
    node->computed_is_array = computed_is_array;
    node->computed_is_scalar = computed_is_scalar;

    return 0;
}
