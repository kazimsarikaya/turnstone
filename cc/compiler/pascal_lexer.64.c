/**
 * @file pascal_lexer.64.c
 * @brief Pascal lexer implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define ___COMPILER_PASCAL_IMPLEMENTATION 0

#include <compiler/pascal.h>
#include <logging.h>
#include <utils.h>
#include <strings.h>

MODULE("turnstone.compiler.pascal");


const pascal_token_t reserved_tokens[] = {
    {PASCAL_TOKEN_TYPE_BEGIN, true, 0, "BEGIN"},
    {PASCAL_TOKEN_TYPE_END, true, 0, "END"},
    {PASCAL_TOKEN_TYPE_PROGRAM, true, 0, "PROGRAM"},
    {PASCAL_TOKEN_TYPE_VAR, true, 0, "VAR"},
    {PASCAL_TOKEN_TYPE_INTEGER, true, 0, "INTEGER"},
};

int8_t pascal_token_destroy(pascal_token_t * token) {
    if (token == NULL) {
        return -1;
    }

    if (token->not_free == false) {
        if (token->text != NULL) {
            memory_free((void*)token->text);
        }

        memory_free(token);
    }

    return 0;
}

int8_t pascal_lexer_init(pascal_lexer_t * lexer, buffer_t * buffer) {
    if (buffer == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "buffer is null");

        return -1;
    }

    lexer->buffer = buffer;
    lexer->current_char = buffer_get_byte(lexer->buffer);

    return 0;
}

int8_t pascal_lexer_advance(pascal_lexer_t * lexer) {
    if (lexer->current_char != '\0') {
        lexer->current_char = buffer_get_byte(lexer->buffer);
    }

    return 0;
}

char_t pascal_lexer_peek(pascal_lexer_t * lexer) {
    return buffer_peek_byte(lexer->buffer);
}

int8_t pascal_lexer_skip_whitespace(pascal_lexer_t * lexer) {
    while (lexer->current_char != '\0' && isspace(lexer->current_char)) {
        pascal_lexer_advance(lexer);
    }

    return 0;
}

int8_t pascal_lexer_get_integer(pascal_lexer_t * lexer, int32_t * value) {
    *value = 0;

    while (isdigit(lexer->current_char)) {
        *value = *value * 10 + (lexer->current_char - '0');
        pascal_lexer_advance(lexer);
    }

    return 0;
}

int8_t pascal_lexer_get_id(pascal_lexer_t * lexer, pascal_token_t ** token) {
    buffer_t * buffer = buffer_new();

    if (buffer == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create id buffer");

        return -1;
    }

    while (isalnum(lexer->current_char)) {
        buffer_append_byte(buffer, lexer->current_char);
        pascal_lexer_advance(lexer);
    }

    buffer_append_byte(buffer, '\0');

    char_t* token_text = (char_t*)buffer_get_all_bytes(buffer, NULL);

    buffer_destroy(buffer);


    for (size_t i = 0; i < sizeof(reserved_tokens) / sizeof(pascal_token_t); i++) {
        if (strcmp(reserved_tokens[i].text, token_text) == 0) {
            *token = (pascal_token_t*)&reserved_tokens[i];

            memory_free((void*)token_text);

            return 0;
        }
    }


    pascal_token_t * new_token = memory_malloc(sizeof(pascal_token_t));

    if (new_token == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

        buffer_destroy(buffer);
        return -1;
    }

    new_token->type = PASCAL_TOKEN_TYPE_ID;
    new_token->text = token_text;
    new_token->not_free = false;


    *token = new_token;

    return 0;
}

int8_t pascal_lexer_get_next_token(pascal_lexer_t * lexer, pascal_token_t ** token) {
    while (lexer->current_char != '\0') {
        if (isspace(lexer->current_char)) {
            pascal_lexer_skip_whitespace(lexer);
            continue;
        }

        if(isalpha(lexer->current_char)) {
            return pascal_lexer_get_id(lexer, token);
        }

        if(lexer->current_char == ':') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            if(pascal_lexer_peek(lexer) == '=') {
                (*token)->type = PASCAL_TOKEN_TYPE_ASSIGN;
                pascal_lexer_advance(lexer);
                pascal_lexer_advance(lexer);
                return 0;
            } else {
                (*token)->type = PASCAL_TOKEN_TYPE_COLON;
                pascal_lexer_advance(lexer);
                return 0;
            }
        }

        if(lexer->current_char == ';') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_SEMI;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if(lexer->current_char == '.') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_DOT;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if(lexer->current_char == ',') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_COMMA;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if(lexer->current_char == '=') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_EQUAL;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if (isdigit(lexer->current_char)) {
            *token = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_INTEGER_CONST;
            pascal_lexer_get_integer(lexer, &(*token)->value);
            return 0;
        }

        if (lexer->current_char == '+') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_PLUS;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if (lexer->current_char == '-') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_MINUS;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if (lexer->current_char == '*') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_MULTIPLY;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if (lexer->current_char == '/') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_DIVIDE;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if (lexer->current_char == '(') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_LPAREN;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if (lexer->current_char == ')') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_RPAREN;
            pascal_lexer_advance(lexer);
            return 0;
        }

        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "unknown token");

        return -1;
    }

    (*token) = memory_malloc(sizeof(pascal_token_t));

    if (*token == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

        return -1;
    }

    (*token)->type = PASCAL_TOKEN_TYPE_EOF;

    return 0;
}
