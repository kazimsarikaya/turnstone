/**
 * @file compiler_symbols.64.c
 * @brief compiler symbols functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>

MODULE("turnstone.compiler");

const compiler_symbol_t* compiler_find_symbol(compiler_t* compiler, const char_t* name) {
    compiler_symbol_table_t* symbol_table = compiler->current_symbol_table;

    while(symbol_table) {
        const compiler_symbol_t* symbol = hashmap_get(symbol_table->symbols, name);

        if(symbol != NULL) {
            return symbol;
        }

        symbol_table = symbol_table->parent;
    }

    const compiler_symbol_t* symbol = hashmap_get(compiler->external_symbols, name);

    return symbol;
}

int8_t compiler_print_symbol_table(compiler_t * compiler) {
    compiler_symbol_table_t* symbol_table = compiler->current_symbol_table;

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
    compiler_symbol_table_t* symbol_table = compiler->current_symbol_table;

    while(symbol_table) {
        hashmap_destroy(symbol_table->symbols);

        compiler_symbol_table_t* parent = symbol_table->parent;

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

        if(!symbol->is_local) {
            iter->next(iter);
            continue;
        }

        if(symbol->type == COMPILER_SYMBOL_TYPE_INTEGER) {
            char_t reg_suffix = compiler_get_reg_suffix(symbol->size);

            if(symbol->is_array) {
                if(symbol->hidden_type == COMPILER_SYMBOL_TYPE_STRING) {
                    buffer_printf(compiler->text_buffer, "\tmov $%s_string@GOT, %%rax\n", symbol->name);
                    buffer_printf(compiler->text_buffer, "\tmov (%%r15, %%rax), %%rax\n");
                    buffer_printf(compiler->text_buffer, "\tmov %%rax, -%d(%%rbp)\n", symbol->stack_offset);
                } else {
                    buffer_printf(compiler->text_buffer, "\tpush %%rdi\n");
                    buffer_printf(compiler->text_buffer, "\tlea -%d(%%rbp), %%rdi\n", symbol->stack_offset);
                    buffer_printf(compiler->text_buffer, "\tmov $%lli, %%rcx\n", symbol->array_size);
                    buffer_printf(compiler->text_buffer, "\tmov $0, %%rax\n");
                    buffer_printf(compiler->text_buffer, "\trep stos%c\n", reg_suffix);
                    buffer_printf(compiler->text_buffer, "\tpop %%rdi\n");
                }
            } else {
                buffer_printf(compiler->text_buffer, "\tmov%c $%lli, -%d(%%rbp)\n", reg_suffix, symbol->int_value, symbol->stack_offset);
            }
        } else if(symbol->type == COMPILER_SYMBOL_TYPE_CUSTOM) {
            int64_t symbol_size = symbol->size;

            if(symbol->is_array) {
                symbol_size = symbol->array_size * symbol_size;
            }

            buffer_printf(compiler->text_buffer, "\tpush %%rdi\n");
            buffer_printf(compiler->text_buffer, "\tlea -%d(%%rbp), %%rdi\n", symbol->stack_offset);
            buffer_printf(compiler->text_buffer, "\tmov $%lli, %%rcx\n", symbol_size);
            buffer_printf(compiler->text_buffer, "\tmov $0, %%rax\n");
            buffer_printf(compiler->text_buffer, "\trep stosb\n");
            buffer_printf(compiler->text_buffer, "\tpop %%rdi\n");

        } else {
            PRINTLOG(COMPILER, LOG_ERROR, "unknown symbol type %d", symbol->type);
            return -1;
        }

        iter->next(iter);
    }

    iter->destroy(iter);

    return 0;
}

int8_t compiler_symbol_destroyer(memory_heap_t* heap, void* symbol) {
    if (symbol == NULL) {
        return -1;
    }

    compiler_symbol_t * l_symbol = (compiler_symbol_t*)symbol;

    if (l_symbol->name != NULL) {
        memory_free_ext(heap, (void*)l_symbol->name);
    }

    if(l_symbol->int_array_value != NULL) {
        memory_free_ext(heap, l_symbol->int_array_value);
    }

    if(l_symbol->string_value != NULL) {
        memory_free_ext(heap, (void*)l_symbol->string_value);
    }

    memory_free_ext(heap, symbol);

    return 0;
}

int8_t compiler_define_symbol(compiler_t* compiler, compiler_symbol_t* symbol, size_t symbol_size) {
    const char_t* sec_name_prefix = "data";
    buffer_t* buffer = compiler->data_buffer;

    if(symbol->is_const) {
        sec_name_prefix = "rodata";
        buffer = compiler->rodata_buffer;
    } else if(symbol->initilized) {
        sec_name_prefix = "data";
        buffer = compiler->data_buffer;
    } else {
        sec_name_prefix = "bss";
        buffer = compiler->bss_buffer;
    }

    const char_t* symbol_name = symbol->name;

    if(symbol->is_local && symbol->hidden_type == COMPILER_SYMBOL_TYPE_STRING) {
        symbol_name = strprintf("%s_string", symbol->name);
    }

    buffer_printf(buffer, ".section .%s.%s\n", sec_name_prefix, symbol_name);
    buffer_printf(buffer, ".align 8\n");
    buffer_printf(buffer, ".local %s\n", symbol_name);
    buffer_printf(buffer, ".type %s, @object\n", symbol_name);
    buffer_printf(buffer, ".size %s, %lli\n", symbol_name, symbol_size);
    buffer_printf(buffer, "%s:\n", symbol_name);

    if(symbol->is_local && symbol->hidden_type == COMPILER_SYMBOL_TYPE_STRING) {
        memory_free((void*)symbol_name);
    }

    return 0;
}

int8_t compiler_destroy_external_symbols(compiler_t* compiler) {
    iterator_t* iter = hashmap_iterator_create(compiler->external_symbols);

    if (iter == NULL) {
        return -1;
    }

    while (iter->end_of_iterator(iter) != 0) {
        compiler_symbol_t* symbol = (compiler_symbol_t*)iter->get_item(iter);

        if (symbol == NULL) {
            iter->destroy(iter);
            return -1;
        }

        compiler_symbol_destroyer(NULL, symbol);

        iter->next(iter);
    }

    iter->destroy(iter);

    hashmap_destroy(compiler->external_symbols);

    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t compiler_add_external_symbol(compiler_t* compiler, const char_t* name, compiler_symbol_type_t type, int64_t size, boolean_t is_const) {
    compiler_symbol_t* symbol = memory_malloc(sizeof(compiler_symbol_t));

    if (symbol == NULL) {
        return -1;
    }

    symbol->name = name;
    symbol->type = type;
    symbol->size = size;
    symbol->is_const = is_const;

    if (hashmap_put(compiler->external_symbols, (void*)name, symbol) != 0) {
        memory_free(symbol);
        return -1;
    }

    return 0;
}
#pragma GCC diagnostic pop
