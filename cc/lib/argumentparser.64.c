/**
 * @file argumentparser.64.c
 * @brief Argument parser implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <argumentparser.h>
#include <stdbufs.h>

MODULE("turnstone.lib.argumentparser");

char_t* argument_parser_advance(argument_parser_t* parser) {
    if(parser == NULL) {
        return NULL;
    }

    if(parser->arguments == NULL) {
        return NULL;
    }

    if(parser->arguments[parser->idx] == NULL) {
        return NULL;
    }

    uint32_t i = parser->idx;

    while(parser->arguments[i] == ' ') { // trim spaces
        i++;
    }

    char_t* start = &parser->arguments[i]; // save start

    if(start[0] == '\'' || start[0] == '\"'){
        char_t quote = start[0];
        start++;
        i++;

        while(parser->arguments[i] != quote && parser->arguments[i] != NULL) { // find end
            i++;
        }

        if(parser->arguments[i] == quote) {
            parser->arguments[i] = NULL;
            i++;
        } else {
            printf("Cannot parse argument: -%s-\n", start);
            return NULL;
        }
    } else {
        while(parser->arguments[i] != ' ' && parser->arguments[i] != NULL) { // find end
            i++;
        }

        if(parser->arguments[i] == ' ') { // replace space with null
            parser->arguments[i] = NULL;
            i++;
        }
    }

    parser->idx = i; // save new index

    return start;
}


