/**
 * @file tokenizer.64.c
 * @brief tokenizer interface implementation
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tokenizer.h>

const uint8_t token_delimeter_chars[] = {
    NULL, '\n', '\r', ' ', '\t', ',', ';', ':',
    '.', '\'', '"', '+', '-', '*', '/', '\\',
    '%', '|', '(', ')', '[', ']', '{', '}'
};



typedef struct tokenizer_iterator_ctx_t {
    buffer_t                      buf;
    const token_delimiter_type_t* delimeters;
    const token_delimiter_type_t* whitespaces;
    token_position_t              position;
    token_t*                      item;
    void*                         extra_data;
} tokenizer_iterator_ctx_t;

int8_t      tokenizer_destroy(iterator_t* iter);
int8_t      tokenizer_end_of_iterator(iterator_t* iter);
iterator_t* tokenizer_next(iterator_t* iter);
const void* tokenizer_get_item(iterator_t* iter);
const void* tokenizer_get_extra_data(iterator_t* iter);
boolean_t   tokenizer_is_in_list(iterator_t* iter, uint8_t c, const token_delimiter_type_t* list, token_delimiter_type_t* type);

boolean_t   tokenizer_is_in_list(iterator_t* iter, uint8_t c, const token_delimiter_type_t* list, token_delimiter_type_t* type) {
    if(!iter) {
        return true;
    }

    tokenizer_iterator_ctx_t* ctx = iter->metadata;

    if(!ctx) {
        return true;
    }

    const token_delimiter_type_t* tmp = list;

    while(*tmp != TOKEN_DELIMETER_TYPE_NULL) {
        if(token_delimeter_chars[*tmp] == c) {
            if(type) {
                *type = *tmp;
            }
            return true;
        }

        tmp++;
    }

    return false;
}

int8_t tokenizer_destroy(iterator_t* iter) {
    if(!iter) {
        return 0;
    }

    tokenizer_iterator_ctx_t* ctx = iter->metadata;

    memory_free(ctx);
    memory_free(iter);

    return 0;
}

int8_t tokenizer_end_of_iterator(iterator_t* iter) {
    if(!iter) {
        return -1;
    }

    tokenizer_iterator_ctx_t* ctx = iter->metadata;

    if(!ctx) {
        return -2;
    }

    buffer_t buf = ctx->buf;

    uint64_t pos = buffer_get_position(buf);
    uint64_t len = buffer_get_length(buf);

    if(!ctx->item && pos == len) {
        return 0;
    }

    return 1;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
iterator_t* tokenizer_next(iterator_t* iter) {
    if(!iter) {
        return iter;
    }

    tokenizer_iterator_ctx_t* ctx = iter->metadata;

    if(!ctx) {
        return iter;
    }

    buffer_t buf = ctx->buf;

    uint64_t pos = buffer_get_position(buf);
    uint64_t len = buffer_get_length(buf);

    if(pos == len) {
        return iter;
    }

    //TODO: tokenize

    uint8_t c = buffer_peek_byte_at_position(ctx->buf, pos);
    token_delimiter_type_t type = TOKEN_DELIMETER_TYPE_NULL;

    while(tokenizer_is_in_list(iter, c, ctx->whitespaces, &type)) {
        pos++;
        ctx->position.offset++;

        if(type == TOKEN_DELIMETER_TYPE_LF) {
            ctx->position.line++;
            ctx->position.offset = 0;
        }

        c = buffer_peek_byte_at_position(ctx->buf, pos);
    }

    buffer_seek(ctx->buf, pos, BUFFER_SEEK_DIRECTION_START);

    if(tokenizer_is_in_list(iter, c, ctx->delimeters, &type)) {
        token_t* token = memory_malloc(sizeof(token_t) + 2);

        if(!token) {
            return iter;
        }

        buffer_get_byte(ctx->buf);

        token->type = TOKEN_TYPE_DELIMETER;
        token->delimiter_type = type;
        token->position.offset = ctx->position.offset;
        token->position.line = ctx->position.line;

        ctx->position.offset++;

        if(type == TOKEN_DELIMETER_TYPE_LF) {
            ctx->position.line++;
            ctx->position.offset = 0;
        }

        token->value_length = 1;
        token->value[0] = token_delimeter_chars[type];

        ctx->item = token;

        return iter;
    }

    if(pos == len) {
        return iter;
    }


    buffer_t res_buf = buffer_new();

    if(!res_buf) {
        return iter;
    }

    token_position_t res_pos = {.offset = ctx->position.offset, .line = ctx->position.line};

    while(pos < len) {
        if(tokenizer_is_in_list(iter, c, ctx->delimeters, &type)) {
            break;
        }

        buffer_append_byte(res_buf, c);

        buffer_get_byte(ctx->buf);

        pos++;
        ctx->position.offset++;

        if(type == TOKEN_DELIMETER_TYPE_LF) {
            ctx->position.line++;
            ctx->position.offset = 0;
        }

        c = buffer_peek_byte_at_position(ctx->buf, pos);

    }

    token_t* item = memory_malloc(sizeof(token_t) + buffer_get_length(res_buf) + 1);

    if(!item) {
        buffer_destroy(res_buf);

        return iter;
    }

    item->type = TOKEN_TYPE_IDENTIFIER;
    item->position.line = res_pos.line;
    item->position.offset = res_pos.offset;

    uint8_t* res_data = buffer_get_all_bytes(res_buf, &item->value_length);
    buffer_destroy(res_buf);

    memory_memcopy(res_data, item->value, item->value_length);

    memory_free(res_data);

    ctx->item = item;

    return iter;
}
#pragma GCC diagnostic pop

const void* tokenizer_get_item(iterator_t* iter) {
    if(!iter) {
        return NULL;
    }

    tokenizer_iterator_ctx_t* ctx = iter->metadata;

    if(!ctx) {
        return NULL;
    }

    void* res = ctx->item;
    ctx->item = NULL;

    return res;
}

const void* tokenizer_get_extra_data(iterator_t* iter) {
    if(!iter) {
        return NULL;
    }

    tokenizer_iterator_ctx_t* ctx = iter->metadata;

    if(!ctx) {
        return NULL;
    }

    return ctx->extra_data;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
iterator_t* tokenizer_new(buffer_t buf, const token_delimiter_type_t* delimeters, const token_delimiter_type_t* whitespaces) {
    tokenizer_iterator_ctx_t* ctx = memory_malloc(sizeof(tokenizer_iterator_ctx_t));

    if(!ctx) {
        return NULL;
    }

    ctx->buf = buf;
    ctx->delimeters = delimeters;
    ctx->whitespaces = whitespaces;

    iterator_t* res = memory_malloc(sizeof(iterator_t));

    if(!res) {
        return NULL;
    }

    res->metadata = ctx;
    res->destroy = tokenizer_destroy;
    res->end_of_iterator = tokenizer_end_of_iterator;
    res->next = tokenizer_next;
    res->get_item = tokenizer_get_item;
    res->get_extra_data = tokenizer_get_extra_data;

    return res->next(res);
}
#pragma GCC diagnostic pop
