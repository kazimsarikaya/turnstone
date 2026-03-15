/**
 * @file argumentparser.h
 * @brief argument parser header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */


#ifndef ___ARGUMENTPARSER_H
#define ___ARGUMENTPARSER_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! @struct argument_parser_t
 * @brief Represents an argument parser instance.
 *
 * This structure holds the state for parsing a string of arguments.
 */
typedef struct argument_parser_t {
    char16_t* arguments; /**< @brief Pointer to the string containing the arguments. */
    uint32_t  idx; /**< @brief Current index within the arguments string. */
} argument_parser_t;

/**
 * @brief Advances the argument parser to the next argument.
 *
 * This function parses the next argument from the `arguments` string,
 * updating the internal index. It handles spaces as delimiters.
 *
 * @param parser Pointer to the argument_parser_t instance.
 * @return A pointer to the beginning of the next argument string,
 *         or NULL if no more arguments are available. The returned string
 *         is part of the original `arguments` buffer and should not be freed.
 */
char16_t* argument_parser_advance(argument_parser_t* parser);

#ifdef __cplusplus
}
#endif

#endif
