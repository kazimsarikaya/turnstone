/**
 * @file asm_parser.h
 * @brief intel assembly parser
 */

#ifndef __COMPILER_ASM_PARSER_H
#define __COMPILER_ASM_PARSER_H 0

#include <types.h>
#include <buffer.h>
#include <list.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum asm_token_type_t {
    ASM_TOKEN_TYPE_NULL,
    ASM_TOKEN_TYPE_DIRECTIVE,
    ASM_TOKEN_TYPE_INSTRUCTION,
    ASM_TOKEN_TYPE_PARAMETER,
    ASM_TOKEN_TYPE_LABEL,
} asm_token_type_t;

typedef enum asm_directive_type_t {
    ASM_DIRECTIVE_TYPE_NULL,
    ASM_DIRECTIVE_TYPE_FILE,
    ASM_DIRECTIVE_TYPE_TEXT,
    ASM_DIRECTIVE_TYPE_SECTION,
    ASM_DIRECTIVE_TYPE_STRING,
    ASM_DIRECTIVE_TYPE_ALIGN,
    ASM_DIRECTIVE_TYPE_P2ALIGN,
    ASM_DIRECTIVE_TYPE_GLOBAL,
    ASM_DIRECTIVE_TYPE_EXTERN,
    ASM_DIRECTIVE_TYPE_LOCAL,
    ASM_DIRECTIVE_TYPE_TYPE,
    ASM_DIRECTIVE_TYPE_SIZE,
    ASM_DIRECTIVE_TYPE_INTERNAL,
    ASM_DIRECTIVE_TYPE_ZERO,
    ASM_DIRECTIVE_TYPE_BYTE,
    ASM_DIRECTIVE_TYPE_LONG,
    ASM_DIRECTIVE_TYPE_QUAD,
    ASM_DIRECTIVE_TYPE_VALUE,
    ASM_DIRECTIVE_TYPE_SET,
} asm_directive_type_t;

typedef struct asm_token_t {
    asm_token_type_t     token_type;
    asm_directive_type_t directive_type;
    char_t*              token_value;
} asm_token_t;


list_t*   asm_parser_parse(buffer_t* buf);
void      asm_parser_print_tokens(list_t* tokens);
boolean_t asm_parser_destroy_tokens(list_t* tokens);

#ifdef __cplusplus
}
#endif

#endif /* __COMPILER_ASM_PARSER_H */
