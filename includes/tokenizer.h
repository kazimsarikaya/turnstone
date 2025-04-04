/**
 * @file tokenizer.h
 * @brief tokenizer interface.
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___TOKENIZER_H
/*! prevent duplicate header error */
#define ___TOKENIZER_H 0

#include <types.h>
#include <buffer.h>
#include <iterator.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum token_type_t {
    TOKEN_TYPE_NULL,
    TOKEN_TYPE_NUMBER,
    TOKEN_TYPE_CHAR,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_KEYWORD,
    TOKEN_TYPE_IDENTIFIER,
    TOKEN_TYPE_DELIMETER,
} token_type_t;

typedef enum token_delimiter_type_t {
    TOKEN_DELIMETER_TYPE_NULL,
    TOKEN_DELIMETER_TYPE_LF,
    TOKEN_DELIMETER_TYPE_CR,
    TOKEN_DELIMETER_TYPE_SPACE,
    TOKEN_DELIMETER_TYPE_TAB,
    TOKEN_DELIMETER_TYPE_COMMA,
    TOKEN_DELIMETER_TYPE_SEMI,
    TOKEN_DELIMETER_TYPE_COLON,
    TOKEN_DELIMETER_TYPE_DOT,
    TOKEN_DELIMETER_TYPE_SQUOTE,
    TOKEN_DELIMETER_TYPE_DQUOTE,
    TOKEN_DELIMETER_TYPE_PLUS,
    TOKEN_DELIMETER_TYPE_MINUS,
    TOKEN_DELIMETER_TYPE_ASTERIX,
    TOKEN_DELIMETER_TYPE_FWDSLASH,
    TOKEN_DELIMETER_TYPE_BCKSLASH,
    TOKEN_DELIMETER_TYPE_PERCENT,
    TOKEN_DELIMETER_TYPE_PIPE,
    TOKEN_DELIMETER_TYPE_LPARAN,
    TOKEN_DELIMETER_TYPE_RPARAN,
    TOKEN_DELIMETER_TYPE_LBRACKET,
    TOKEN_DELIMETER_TYPE_RBRACKET,
    TOKEN_DELIMETER_TYPE_LBRACE,
    TOKEN_DELIMETER_TYPE_RBRACE,
} token_delimiter_type_t;

extern const uint8_t token_delimeter_chars[];

typedef struct token_position_t {
    uint64_t line;
    uint64_t offset;
} token_position_t;

typedef struct token_t {
    token_type_t           type;
    token_delimiter_type_t delimiter_type;
    token_position_t       position;
    uint64_t               value_length;
    uint8_t                value[];
} token_t;

iterator_t* tokenizer_new(buffer_t* buf, const token_delimiter_type_t* delimeters, const token_delimiter_type_t* whitespaces);

#ifdef __cplusplus
}
#endif

#endif
