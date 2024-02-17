/**
 * @file asm_parser.c
 * @brief intel x86_64 assembly parser
 */

#include <compiler/asm_parser.h>
#include <strings.h>
#include <logging.h>

MODULE("turnstone.compiler.assembler");

boolean_t            asm_parser_emit_whitespace(buffer_t* buf);
boolean_t            asm_parser_emit_comment(buffer_t* buf);
boolean_t            asm_parser_emit_line(buffer_t* buf, buffer_t* line);
boolean_t            asm_parser_is_whitespace(uint8_t c);
asm_directive_type_t asm_parser_get_directive_type(const char_t* str);
boolean_t            asm_parser_parse_line(buffer_t* line, list_t* tokens);

boolean_t asm_parser_is_whitespace(uint8_t c) {
    return c == ' ' || c == '\t' || c == '\r';
}

boolean_t asm_parser_emit_comment(buffer_t* buf) {
    if(buffer_remaining(buf) < 2) {
        return false;
    }

    uint8_t c = buffer_peek_byte(buf);

    if(c != ';' && c != '#') {
        return false;
    }

    buffer_get_byte(buf);

    while(buffer_remaining(buf) > 0) {
        c = buffer_peek_byte(buf);

        if(c == '\n') {
            break;
        }

        buffer_get_byte(buf);
    }

    return true;
}

boolean_t asm_parser_emit_whitespace(buffer_t* buf) {

    while(buffer_remaining(buf) > 0) {
        uint8_t c = buffer_peek_byte(buf);

        if(!asm_parser_is_whitespace(c)) {
            break;
        }

        buffer_get_byte(buf);
    }

    return true;
}

boolean_t asm_parser_emit_line(buffer_t* buf, buffer_t* line) {
    buffer_reset(line);

    while(buffer_remaining(buf) > 0) {
        uint8_t c = buffer_peek_byte(buf);

        if(c == '\n') {
            break;
        }

        buffer_append_byte(line, c);
        buffer_get_byte(buf);
    }

    return true;
}

asm_directive_type_t asm_parser_get_directive_type(const char_t* str) {
    if(str == NULL) {
        return ASM_DIRECTIVE_TYPE_NULL;
    } else if(strcmp(str, ".align") == 0) {
        return ASM_DIRECTIVE_TYPE_ALIGN;
    } else if(strcmp(str, ".byte") == 0) {
        return ASM_DIRECTIVE_TYPE_BYTE;
    } else if(strcmp(str, ".file") == 0) {
        return ASM_DIRECTIVE_TYPE_FILE;
    } else if(strcmp(str, ".globl") == 0) {
        return ASM_DIRECTIVE_TYPE_GLOBAL;
    } else if(strcmp(str, ".internal") == 0) {
        return ASM_DIRECTIVE_TYPE_INTERNAL;
    } else if(strcmp(str, ".long") == 0) {
        return ASM_DIRECTIVE_TYPE_LONG;
    } else if(strcmp(str, ".p2align") == 0) {
        return ASM_DIRECTIVE_TYPE_P2ALIGN;
    } else if(strcmp(str, ".quad") == 0) {
        return ASM_DIRECTIVE_TYPE_QUAD;
    } else if(strcmp(str, ".section") == 0) {
        return ASM_DIRECTIVE_TYPE_SECTION;
    } else if(strcmp(str, ".set") == 0) {
        return ASM_DIRECTIVE_TYPE_SET;
    } else if(strcmp(str, ".size") == 0) {
        return ASM_DIRECTIVE_TYPE_SIZE;
    } else if(strcmp(str, ".string") == 0 || strcmp(str, ".ascii") == 0) {
        return ASM_DIRECTIVE_TYPE_STRING;
    } else if(strcmp(str, ".text") == 0) {
        return ASM_DIRECTIVE_TYPE_TEXT;
    } else if(strcmp(str, ".type") == 0) {
        return ASM_DIRECTIVE_TYPE_TYPE;
    } else if(strcmp(str, ".value") == 0) {
        return ASM_DIRECTIVE_TYPE_VALUE;
    } else if(strcmp(str, ".zero") == 0) {
        return ASM_DIRECTIVE_TYPE_ZERO;
    } else {
        return ASM_DIRECTIVE_TYPE_NULL;
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
boolean_t asm_parser_parse_line(buffer_t* line, list_t* tokens) {
    if(buffer_remaining(line) == 0) {
        return true;
    }

    buffer_t* part = buffer_new_with_capacity(NULL, 1024);

    // the first part is delimetered one of : or whitespace
    // if it is : then it is a label
    // if it is whitespace then it is a directive or instruction
    // directive start with .
    // directives and instructions can have parameters delimetered by , whitespaces between parameters are ignored
    // label lines can have instructions or directives after them

    boolean_t is_label = false;
    boolean_t is_directive = false;

    while(buffer_remaining(line)) {
        uint8_t c = buffer_peek_byte(line);

        if(c == ':') {
            is_label = true;

            break;
        }

        if(asm_parser_is_whitespace(c)) {
            break;
        }

        buffer_append_byte(part, c);
        buffer_get_byte(line);
    }

    buffer_seek(part, 0, BUFFER_SEEK_DIRECTION_START);

    if(!is_label) {
        is_directive = buffer_peek_byte(part) == '.';
    }

    asm_token_t* token = memory_malloc(sizeof(asm_token_t));

    if(token == NULL) {
        buffer_destroy(part);

        return false;
    }

    token->token_type = is_label ? ASM_TOKEN_TYPE_LABEL : is_directive ? ASM_TOKEN_TYPE_DIRECTIVE : ASM_TOKEN_TYPE_INSTRUCTION;
    token->token_value = (char_t*)buffer_get_all_bytes(part, NULL);
    token->directive_type = is_directive ? asm_parser_get_directive_type(token->token_value) : ASM_DIRECTIVE_TYPE_NULL;

    list_queue_push(tokens, token);

    asm_parser_emit_whitespace(line);

    if(is_label && buffer_remaining(line) > 0) {
        buffer_get_byte(line);
        buffer_destroy(part);

        return asm_parser_parse_line(line, tokens);
    }

    if(strcmp("lock", token->token_value) == 0 ||
       strcmp("rep", token->token_value) == 0 ||
       strcmp("repe", token->token_value) == 0 ||
       strcmp("repz", token->token_value) == 0 ||
       strcmp("repne", token->token_value) == 0 ||
       strcmp("repnz", token->token_value) == 0) {
        buffer_destroy(part);

        return asm_parser_parse_line(line, tokens);
    }

    if(buffer_remaining(line) == 0) {
        buffer_destroy(part);

        return true;
    }

    while(buffer_remaining(line)) {
        buffer_reset(part);

        boolean_t is_mem_paran = false;
        boolean_t is_string = false;
        uint8_t c = NULL;
        uint8_t c_prev = 0;

        while(buffer_remaining(line)) {
            c_prev = c;
            c = buffer_peek_byte(line);

            if(c == '(') {
                is_mem_paran = true;
            } else if(c == ')') {
                is_mem_paran = false;
            } else if(c_prev != '\\' && c == '"') {
                is_string = !is_string;
            }

            if((!is_mem_paran && !is_string) && c == ',') {
                buffer_get_byte(line);

                break;
            }

            buffer_append_byte(part, c);
            buffer_get_byte(line);
        }

        token = memory_malloc(sizeof(asm_token_t));

        if(token == NULL) {
            buffer_destroy(part);

            return false;
        }

        token->token_type = ASM_TOKEN_TYPE_PARAMETER;
        token->token_value = (char_t*)buffer_get_all_bytes(part, NULL);

        list_queue_push(tokens, token);

        asm_parser_emit_whitespace(line);
    }

    buffer_destroy(part);

    return true;
}
#pragma GCC diagnostic pop

void asm_parser_print_tokens(list_t* tokens) {
    iterator_t* it = list_iterator_create(tokens);

    while(it->end_of_iterator(it) != 0) {
        const asm_token_t* tok = it->get_item(it);

        printf("%i %i %s\n", tok->token_type, tok->directive_type, tok->token_value);

        it = it->next(it);
    }

    it->destroy(it);
}

boolean_t asm_parser_destroy_tokens(list_t* tokens) {
    iterator_t* it = list_iterator_create(tokens);

    while(it->end_of_iterator(it) != 0) {
        const asm_token_t* tok = it->delete_item(it);

        memory_free((void*)tok->token_value);
        memory_free((void*)tok);

        it = it->next(it);
    }

    it->destroy(it);

    list_destroy(tokens);

    return true;
}

list_t* asm_parser_parse(buffer_t* buf) {
    list_t* tokens = list_create_list();

    buffer_t* line = buffer_new_with_capacity(NULL, 1024);

    while(buffer_remaining(buf) > 0 ) {
        buffer_reset(line);

        asm_parser_emit_whitespace(buf);
        asm_parser_emit_comment(buf);

        if(!asm_parser_emit_line(buf, line)) {
            printf("error\n");
            break;
        }

        buffer_seek(line, 0, BUFFER_SEEK_DIRECTION_START);

        if(!asm_parser_parse_line(line, tokens)) {
            printf("error\n");
            break;
        }

        asm_parser_emit_comment(buf);

        uint8_t c = buffer_peek_byte(buf);

        if(c == '\n') {
            buffer_get_byte(buf);
            continue;
        } else {
            printf("error\n");
            break;
        }
    }

    buffer_destroy(line);

    return tokens;
}


