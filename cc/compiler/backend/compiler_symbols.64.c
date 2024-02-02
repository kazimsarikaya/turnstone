/**
 * @file compiler_symbols.64.c
 * @brief compiler symbols functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>

MODULE("turnstone.compiler");

const compiler_symbol_t* compiler_find_symbol(compiler_t* compiler, const char_t* name) {
    symbol_table_t* symbol_table = compiler->current_symbol_table;

    while(symbol_table) {
        const compiler_symbol_t* symbol = hashmap_get(symbol_table->symbols, name);

        if(symbol != NULL) {
            return symbol;
        }

        symbol_table = symbol_table->parent;
    }

    return NULL;
}

int8_t compiler_print_symbol_table(compiler_t * compiler) {
    symbol_table_t* symbol_table = compiler->current_symbol_table;

    while(symbol_table) {
        iterator_t* iter = hashmap_iterator_create(symbol_table->symbols);

        if (iter == NULL) {
            return -1;
        }

        while (iter->end_of_iterator(iter) != 0) {
            const compiler_symbol_t* symbol = iter->get_item(iter);

            if (symbol == NULL) {
                iter->destroy(iter);
                return -1;
            }

            PRINTLOG(COMPILER, LOG_INFO, "symbol %s type %d size %lli value %lli", symbol->name, symbol->type, symbol->size, symbol->int_value);

            iter->next(iter);
        }

        iter->destroy(iter);

        symbol_table = symbol_table->parent;
    }

    return 0;
}

int8_t compiler_destroy_symbol_table(compiler_t * compiler) {
    symbol_table_t* symbol_table = compiler->current_symbol_table;

    while(symbol_table) {
        hashmap_destroy(symbol_table->symbols);

        symbol_table_t* parent = symbol_table->parent;

        memory_free(symbol_table);

        symbol_table = parent;
    }

    return 0;
}

int8_t compiler_build_stack(compiler_t* compiler) {
    if (compiler->current_symbol_table == NULL) {
        return -1;
    }

    iterator_t* iter = hashmap_iterator_create(compiler->current_symbol_table->symbols);

    if (iter == NULL) {
        return -1;
    }

    while (iter->end_of_iterator(iter) != 0) {
        const compiler_symbol_t* symbol = iter->get_item(iter);

        if (symbol == NULL) {
            iter->destroy(iter);
            return -1;
        }

        if(symbol->is_local &&  symbol->type == COMPILER_SYMBOL_TYPE_INTEGER) {
            char_t reg_suffix = compiler_get_reg_suffix(symbol->size);

            if(symbol->is_array) {
                buffer_printf(compiler->text_buffer, "\tpush %%rdi\n");
                buffer_printf(compiler->text_buffer, "\tlea -%d(%%rbp), %%rdi\n", symbol->stack_offset);
                buffer_printf(compiler->text_buffer, "\tmov $%lli, %%rcx\n", symbol->array_size);
                buffer_printf(compiler->text_buffer, "\tmov $0, %%rax\n");
                buffer_printf(compiler->text_buffer, "\trep stos%c\n", reg_suffix);
                buffer_printf(compiler->text_buffer, "\tpop %%rdi\n");
            } else {
                buffer_printf(compiler->text_buffer, "\tmov%c $%lli, -%d(%%rbp)\n", reg_suffix, symbol->int_value, symbol->stack_offset);
            }
        }

        iter->next(iter);
    }

    iter->destroy(iter);

    return 0;
}
