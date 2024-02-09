/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAM_SIZE 0x1000000 // 16 MB
#include "setup.h"
#include <compiler/pascal.h>
#include <strings.h>
#include <logging.h>
#include <utils.h>
#include <hashmap.h>
#include <linkedlist.h>


int32_t main(int32_t argc, char * argv[]);



int32_t main(int32_t argc, char * argv[]) {
    if (argc != 3) {
        print_error("Usage: %s <in> <out>\n", argv[0]);
        return -1;
    }

    FILE* in = fopen(argv[1], "r");
    fseek(in, 0, SEEK_END);
    size_t size = ftell(in);
    fseek(in, 0, SEEK_SET);

    char_t* source = memory_malloc(size + 1);

    if (source == NULL) {
        print_error("Error: Failed to allocate memory for source\n");
        return -1;
    }

    fread(source, 1, size, in);

    source[size] = '\0';

    fclose(in);

    buffer_t * buffer = buffer_encapsulate((uint8_t*)source, size + 1);

    pascal_lexer_t lexer = {0};
    pascal_lexer_init(&lexer, buffer);

    compiler_ast_t ast = {0};
    compiler_ast_init(&ast);

    pascal_parser_t parser = {0};
    if(pascal_parser_init(&parser, &lexer) != 0) {
        print_error("Error: Failed to initialize parser\n");
    }

    if(pascal_parser_parse(&parser, &ast) != 0) {
        print_error("Error: Parsing failed\n");

        pascal_parser_destroy(&parser);
        compiler_ast_destroy(&ast);
        buffer_destroy(buffer);
        memory_free(source);

        return -1;
    }

    memory_free(source);
    buffer_destroy(buffer);
    pascal_parser_destroy(&parser);

    compiler_t compiler = {0};

    if(compiler_init(&compiler, &ast) != 0) {
        print_error("Error: Failed to initialize compiler\n");
        compiler_ast_destroy(&ast);

        return -1;
    }

    int64_t result = 0;

    int8_t res = 0;

    if(compiler_execute(&compiler, &result) != 0) {
        print_error("Error: Failed to execute compiler\n");

        res = -1;
    }

    FILE* out = fopen(argv[2], "w");

    if (out == NULL) {
        print_error("Error: Failed to open output file\n");

        compiler_destroy(&compiler);

        return -1;
    }

    size_t out_size = 0;

    uint8_t* out_bytes = buffer_get_all_bytes(compiler.text_buffer, &out_size);

    fwrite(out_bytes, 1, out_size, out);

    fclose(out);

    memory_free(out_bytes);

    compiler_destroy(&compiler);

    PRINTLOG(COMPILER, LOG_INFO, "Success");

    return res;
}
