/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <buffer.h>
#include <linkedlist.h>

typedef enum asm_token_type_t {
    ASM_TOKEN_TYPE_NULL,
    ASM_TOKEN_TYPE_DIRECTIVE,
    ASM_TOKEN_TYPE_INSTRUCTION,
} asm_token_type_t;

typedef struct asm_token_t {
    asm_token_type_t token_type;
    char_t*          token_text;
} asm_token_t;

linkedlist_t asm_parser_parse(buffer_t* buf);

boolean_t asm_parser_emit_whitespace(buffer_t* buf);

boolean_t asm_parser_emit_whitespace(buffer_t* buf) {

    while(buffer_remaining(buf) > 0 &&
          (buffer_peek_byte(buf) == ' ' || buffer_peek_byte(buf) == '\t' || buffer_peek_byte(buf) == '\r')) {
        buffer_get_byte(buf);
    }

    return true;
}

linkedlist_t asm_parser_parse(buffer_t* buf) {

    while(buffer_remaining(buf) > 0 ) {
        asm_parser_emit_whitespace(buf);

        printf("%c \n", buffer_get_byte(buf));

        break;
    }

    return NULL;
}

uint32_t main(uint32_t argc, char_t** argv) {
    if(argc != 3) {
        print_error("not enough paramters");
        printf("usage tosasm <in> <out>\n");

        return -1;
    }

    FILE* fd = fopen(argv[1], "r");

    if (!fd) {
        print_error("cannot open input file");

        return -2;
    }

    fseek(fd, 0, SEEK_END);

    int64_t in_size = ftell(fd);

    fseek(fd, 0, SEEK_SET);

    uint8_t* in_data = memory_malloc(in_size);

    if(in_data == NULL) {
        print_error("cannot alloc input data memory");
        fclose(fd);

        return -1;
    }

    fread(in_data, 1, in_size, fd);
    fclose(fd);

    buffer_t* inbuf  = buffer_encapsulate(in_data, in_size);


    asm_parser_parse(inbuf);

    buffer_destroy(inbuf);
    memory_free(in_data);

    return 0;
}
