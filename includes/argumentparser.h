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

typedef struct argument_parser_t {
    char_t*  arguments;
    uint32_t idx;
} argument_parser_t;

char_t* argument_parser_advance(argument_parser_t* parser);

#ifdef __cplusplus
}
#endif

#endif
