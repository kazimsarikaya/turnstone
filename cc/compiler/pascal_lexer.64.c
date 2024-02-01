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
    {PASCAL_TOKEN_TYPE_BEGIN, true, 0, 0, "begin", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_END, true, 0, 0, "end", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_PROGRAM, true, 0, 0, "program", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_VAR, true, 0, 0, "var", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_CONST, true, 0, 0, "const", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_INTEGER, true, 0, 0, "bit", false, 1, false, false},
    {PASCAL_TOKEN_TYPE_INTEGER, true, 0, 0, "int8", false, 8, false, false},
    {PASCAL_TOKEN_TYPE_INTEGER, true, 0, 0, "int16", false, 16, false, false},
    {PASCAL_TOKEN_TYPE_INTEGER, true, 0, 0, "int32", false, 32, false, false},
    {PASCAL_TOKEN_TYPE_INTEGER, true, 0, 0, "int64", false, 64, false, false},
    {PASCAL_TOKEN_TYPE_REAL, true, 0, 0, "float32", false, 32, false, false},
    {PASCAL_TOKEN_TYPE_REAL, true, 0, 0, "float64", false, 64, false, false},
    {PASCAL_TOKEN_TYPE_INTEGER_DIVIDE, true, 0, 0, "div", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_PROCEDURE, true, 0, 0, "procedure", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_FUNCTION, true, 0, 0, "function", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_OR, true, 0, 0, "or", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_XOR, true, 0, 0, "xor", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_AND, true, 0, 0, "and", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_NOT, true, 0, 0, "not", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_MOD, true, 0, 0, "mod", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_SHL, true, 0, 0, "shl", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_SHR, true, 0, 0, "shr", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_IN, true, 0, 0, "in", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_IF, true, 0, 0, "if", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_THEN, true, 0, 0, "then", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_ELSE, true, 0, 0, "else", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_WHILE, true, 0, 0, "while", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_DO, true, 0, 0, "do", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_REPEAT, true, 0, 0, "repeat", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_UNTIL, true, 0, 0, "until", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_FOR, true, 0, 0, "for", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_TO, true, 0, 0, "to", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_DOWNTO, true, 0, 0, "downto", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_STEP, true, 0, 0, "step", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_CONTINUE, true, 0, 0, "continue", false, 0, false, false},
    {PASCAL_TOKEN_TYPE_BREAK, true, 0, 0, "break", false, 0, false, false},
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

int8_t pascal_lexer_get_number(pascal_lexer_t * lexer, pascal_token_t** token) {
    // first check it is hex number starts with 0x then it is hex number
    if (lexer->current_char == '0' && pascal_lexer_peek(lexer) == 'x') {
        pascal_lexer_advance(lexer);
        pascal_lexer_advance(lexer);

        uint64_t value = 0;

        while (isxdigit(lexer->current_char)) {
            value = value * 16 + (lexer->current_char - '0');
            pascal_lexer_advance(lexer);
        }

        (*token)->type = PASCAL_TOKEN_TYPE_INTEGER_CONST;
        (*token)->value = value;
        (*token)->size = 32;

        if (lexer->current_char == 'L' || lexer->current_char == 'l') {
            (*token)->size = 64;
            pascal_lexer_advance(lexer);
        }

        return 0;
    }

    // if it is not hex number then it is decimal number maybe real number
    // first get integer part

    uint64_t value = 0;

    while (isdigit(lexer->current_char)) {
        value = value * 10 + (lexer->current_char - '0');
        pascal_lexer_advance(lexer);
    }

    // if it is real number then get real part

    if (lexer->current_char == '.') {
        pascal_lexer_advance(lexer);

        float64_t real_value = value;

        float64_t factor = 10.0;

        while (isdigit(lexer->current_char)) {
            real_value = real_value + (lexer->current_char - '0') / factor;
            factor *= 10.0;
            pascal_lexer_advance(lexer);
        }

        (*token)->type = PASCAL_TOKEN_TYPE_REAL;
        (*token)->real_value = real_value;

        return 0;
    }

    (*token)->type = PASCAL_TOKEN_TYPE_INTEGER_CONST;
    (*token)->value = value;
    (*token)->size = 32;

    if (lexer->current_char == 'L' || lexer->current_char == 'l') {
        (*token)->size = 64;
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

    if (token_text == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot get token text");

        return -1;
    }

    token_text = strlower(token_text);

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

        if(lexer->current_char == '<') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            if(pascal_lexer_peek(lexer) == '=') {
                (*token)->type = PASCAL_TOKEN_TYPE_LESS_THAN_OR_EQUAL;
                pascal_lexer_advance(lexer);
                pascal_lexer_advance(lexer);
                return 0;
            } else if(pascal_lexer_peek(lexer) == '>') {
                (*token)->type = PASCAL_TOKEN_TYPE_NOT_EQUAL;
                pascal_lexer_advance(lexer);
                pascal_lexer_advance(lexer);
                return 0;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_LESS_THAN;
            pascal_lexer_advance(lexer);
            return 0;

        }

        if(lexer->current_char == '>') {
            (*token) = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            if(pascal_lexer_peek(lexer) == '=') {
                (*token)->type = PASCAL_TOKEN_TYPE_GREATER_THAN_OR_EQUAL;
                pascal_lexer_advance(lexer);
                pascal_lexer_advance(lexer);
                return 0;
            }

            (*token)->type = PASCAL_TOKEN_TYPE_GREATER_THAN;
            pascal_lexer_advance(lexer);
            return 0;
        }

        if (isdigit(lexer->current_char)) {
            *token = memory_malloc(sizeof(pascal_token_t));

            if (*token == NULL) {
                PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

                return -1;
            }

            pascal_lexer_get_number(lexer, token);
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

            (*token)->type = PASCAL_TOKEN_TYPE_REAL_DIVIDE;
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

        if(lexer->current_char == '\'') {
            return pascal_lexer_get_string(lexer, token);
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

int8_t pascal_lexer_get_string(pascal_lexer_t * lexer, pascal_token_t ** token) {
    buffer_t * buffer = buffer_new();

    if (buffer == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create string buffer");

        return -1;
    }

    pascal_lexer_advance(lexer);

    while (lexer->current_char != '\0' && lexer->current_char != '\'') {
        buffer_append_byte(buffer, lexer->current_char);
        pascal_lexer_advance(lexer);
    }

    if (lexer->current_char == '\0') {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "string not closed");

        buffer_destroy(buffer);
        return -1;
    }

    pascal_lexer_advance(lexer);

    buffer_append_byte(buffer, '\0');

    char_t* token_text = (char_t*)buffer_get_all_bytes(buffer, NULL);

    buffer_destroy(buffer);

    if (token_text == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot get token text");

        return -1;
    }

    pascal_token_t * new_token = memory_malloc(sizeof(pascal_token_t));

    if (new_token == NULL) {
        PRINTLOG(COMPILER_PASCAL, LOG_ERROR, "cannot create token");

        memory_free((void*)token_text);
        return -1;
    }

    new_token->type = PASCAL_TOKEN_TYPE_STRING_CONST;
    new_token->text = token_text;
    new_token->not_free = false;

    *token = new_token;

    return 0;
}
