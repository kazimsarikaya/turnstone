/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <tokenizer.h>
#include <buffer.h>
#include <strings.h>
#include <utils.h>

int32_t main(uint32_t argc, char_t** argv);
void    token_print(const token_t* token);

void token_print(const token_t* token) {
    printf("token type: %i delim type: %i\n", token->type, token->delimiter_type);
    printf("      position: %lli %lli\n", token->position.line, token->position.offset);
    printf("      value len: %lli value: %s\n", token->value_length, token->value);
}

int32_t main(uint32_t argc, char_t** argv) {
    UNUSED(argc);
    UNUSED(argv);

    uint8_t* test_data = (uint8_t*)",,\"blabla\",\n,0x1234,5,3.7,hello, world\nfrom,this test";

    token_delimiter_type_t delims[] = {
        TOKEN_DELIMETER_TYPE_COMMA,
        TOKEN_DELIMETER_TYPE_LF,
        TOKEN_DELIMETER_TYPE_NULL
    };

    token_delimiter_type_t wss[] = {
        TOKEN_DELIMETER_TYPE_NULL
    };

    buffer_t* buf = buffer_encapsulate(test_data, strlen((char_t*)test_data));

    iterator_t* tokenizer = tokenizer_new(buf, delims, wss);

    while(tokenizer->end_of_iterator(tokenizer) != 0) {
        token_t* token = (token_t*)tokenizer->get_item(tokenizer);

        token_print(token);

        memory_free(token);

        tokenizer = tokenizer->next(tokenizer);
    }

    tokenizer->destroy(tokenizer);

    buffer_destroy(buf);

    print_success("TESTS PASSED");

    return 0;
}
