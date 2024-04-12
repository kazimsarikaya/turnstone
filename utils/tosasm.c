/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define RAMSIZE 0x1000000 // 16 MB
#include "setup.h"
#include <compiler/asm_parser.h>
#include <compiler/asm_encoder.h>
#include <compiler/asm_instructions.h>
#include <strings.h>
#include <int_limits.h>
#include <utils.h>
#include <linker.h>

uint32_t main(uint32_t argc, char_t** argv) {
    if(argc != 3) {
        print_error("not enough paramters");
        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "usage tosasm <in> <out>");

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


    list_t* tokens = asm_parser_parse(inbuf);

    buffer_destroy(inbuf);
    memory_free(in_data);

    // asm_parser_print_tokens(tokens);

    asm_encoder_ctx_t* ctx = memory_malloc(sizeof(asm_encoder_ctx_t));

    if(ctx == NULL) {
        print_error("cannot alloc encoder context");
        asm_parser_destroy_tokens(tokens);

        return -1;
    }

    ctx->tokens = tokens;

    int8_t result = asm_encode_instructions(ctx);

    asm_parser_destroy_tokens(tokens);

    if(result == false) {
        print_error("cannot encode instructions");
        asm_encoder_destroy_context(ctx);

        return -1;
    }

    buffer_t* outbuf = buffer_new();
    asm_encoder_dump(ctx, outbuf);

    asm_encoder_destroy_context(ctx);

    fd = fopen(argv[2], "w");
    uint64_t length = 0;
    uint8_t* out_data = buffer_get_all_bytes(outbuf, &length);
    buffer_destroy(outbuf);
    fwrite(out_data, 1, length, fd);
    memory_free(out_data);
    fclose(fd);


    return 0;
}
