/**
 * @file asm_encoder.64.c
 * @brief 64-bit assembler encoder.
 */

#include <compiler/asm_encoder.h>
#include <compiler/asm_parser.h>
#include <compiler/asm_instructions.h>
#include <strings.h>
#include <int_limits.h>
#include <logging.h>

MODULE("turnstone.compiler.assembler");

typedef struct asm_register_map_t {
    const char_t*        name;
    const asm_register_t reg;
} asm_register_map_t;

const asm_register_map_t asm_register_map[] = {
    {.name = "al", .reg = { .register_index = 0, .register_size  = 8, .force_rex = false, .is_control = false } },
    {.name = "ax", .reg = { .register_index = 0, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "eax", .reg = { .register_index = 0, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "rax", .reg = { .register_index = 0, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm0", .reg = { .register_index = 0, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "es", .reg = { .register_index = 0, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "cr0", .reg = { .register_index = 0, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "cl", .reg = { .register_index = 1, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "cx", .reg = { .register_index = 1, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "ecx", .reg = { .register_index = 1, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "rcx", .reg = { .register_index = 1, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm1", .reg = { .register_index = 1, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "cs", .reg = { .register_index = 1, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "cr1", .reg = { .register_index = 1, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "dl", .reg = { .register_index = 2, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "dx", .reg = { .register_index = 2, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "edx", .reg = { .register_index = 2, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "rdx", .reg = { .register_index = 2, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm2", .reg = { .register_index = 2, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "ss", .reg = { .register_index = 2, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "cr2", .reg = { .register_index = 2, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "bl", .reg = { .register_index = 3, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "bx", .reg = { .register_index = 3, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "ebx", .reg = { .register_index = 3, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "rbx", .reg = { .register_index = 3, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm3", .reg = { .register_index = 3, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "ds", .reg = { .register_index = 3, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "cr3", .reg = { .register_index = 3, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "ah", .reg = { .register_index = 4, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "spl", .reg = { .register_index = 4, .register_size = 8, .force_rex = true, .is_control = false } },
    {.name = "sp", .reg = { .register_index = 4, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "esp", .reg = { .register_index = 4, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "rsp", .reg = { .register_index = 4, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm4", .reg = { .register_index = 4, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "fs", .reg = { .register_index = 4, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "cr4", .reg = { .register_index = 4, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "ch", .reg = { .register_index = 5, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "bpl", .reg = { .register_index = 5, .register_size = 8, .force_rex = true, .is_control = false } },
    {.name = "bp", .reg = { .register_index = 5, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "ebp", .reg = { .register_index = 5, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "rbp", .reg = { .register_index = 5, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm5", .reg = { .register_index = 5, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "gs", .reg = { .register_index = 5, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "cr5", .reg = { .register_index = 5, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "dh", .reg = { .register_index = 6, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "sil", .reg = { .register_index = 6, .register_size = 8, .force_rex = true, .is_control = false } },
    {.name = "si", .reg = { .register_index = 6, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "esi", .reg = { .register_index = 6, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "rsi", .reg = { .register_index = 6, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm6", .reg = { .register_index = 6, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "cr6", .reg = { .register_index = 6, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "bh", .reg = { .register_index = 7, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "dil", .reg = { .register_index = 7, .register_size = 8, .force_rex = true, .is_control = false } },
    {.name = "di", .reg = { .register_index = 7, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "edi", .reg = { .register_index = 7, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "rdi", .reg = { .register_index = 7, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm7", .reg = { .register_index = 7, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "cr7", .reg = { .register_index = 7, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "r8b", .reg = { .register_index = 8, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "r8w", .reg = { .register_index = 8, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "r8d", .reg = { .register_index = 8, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "r8", .reg = { .register_index = 8, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm8", .reg = { .register_index = 8, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "cr8", .reg = { .register_index = 8, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "r9b", .reg = { .register_index = 9, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "r9w", .reg = { .register_index = 9, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "r9d", .reg = { .register_index = 9, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "r9", .reg = { .register_index = 9, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm9", .reg = { .register_index = 9, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "cr9", .reg = { .register_index = 9, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "r10b", .reg = { .register_index = 10, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "r10w", .reg = { .register_index = 10, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "r10d", .reg = { .register_index = 10, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "r10", .reg = { .register_index = 10, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm10", .reg = { .register_index = 10, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "cr10", .reg = { .register_index = 10, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "r11b", .reg = { .register_index = 11, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "r11w", .reg = { .register_index = 11, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "r11d", .reg = { .register_index = 11, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "r11", .reg = { .register_index = 11, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm11", .reg = { .register_index = 11, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "cr11", .reg = { .register_index = 11, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "r12b", .reg = { .register_index = 12, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "r12w", .reg = { .register_index = 12, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "r12d", .reg = { .register_index = 12, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "r12", .reg = { .register_index = 12, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm12", .reg = { .register_index = 12, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "cr12", .reg = { .register_index = 12, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "r13b", .reg = { .register_index = 13, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "r13w", .reg = { .register_index = 13, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "r13d", .reg = { .register_index = 13, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "r13", .reg = { .register_index = 13, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm13", .reg = { .register_index = 13, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "cr13", .reg = { .register_index = 13, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "r14b", .reg = { .register_index = 14, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "r14w", .reg = { .register_index = 14, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "r14d", .reg = { .register_index = 14, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "r14", .reg = { .register_index = 14, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm14", .reg = { .register_index = 14, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "cr14", .reg = { .register_index = 14, .register_size = 32, .force_rex = false, .is_control = true } },
    {.name = "r15b", .reg = { .register_index = 15, .register_size = 8, .force_rex = false, .is_control = false } },
    {.name = "r15w", .reg = { .register_index = 15, .register_size = 16, .force_rex = false, .is_control = false } },
    {.name = "r15d", .reg = { .register_index = 15, .register_size = 32, .force_rex = false, .is_control = false } },
    {.name = "r15", .reg = { .register_index = 15, .register_size = 64, .force_rex = false, .is_control = false } },
    {.name = "xmm15", .reg = { .register_index = 15, .register_size = 128, .force_rex = false, .is_control = false } },
    {.name = "cr15", .reg = { .register_index = 15, .register_size = 32, .force_rex = false, .is_control = true } },
};

#define ASM_REGISTER_COUNT (sizeof(asm_register_map) / sizeof(asm_register_map_t))

boolean_t asm_parse_number(char_t* data, uint64_t* result);
boolean_t asm_parse_instruction_param(const asm_token_t* tok, asm_instruction_param_t* param);
boolean_t asm_parse_register_param(char_t* reg_str, uint8_t reg_idx, asm_instruction_param_t* param);
boolean_t asm_parse_immediate_param(char_t* data, asm_instruction_param_t* param);
boolean_t asm_parse_memory_param(char_t* data, asm_instruction_param_t* param);
boolean_t asm_encode_modrm_sib(asm_instruction_param_t op, boolean_t* need_sib, uint8_t* modrm, uint8_t* sib,
                               boolean_t* need_rex, uint8_t* rex,
                               boolean_t* has_displacement, uint8_t* disp_size);
boolean_t asm_encode_instruction(iterator_t* it, buffer_t* outbuf, list_t* relocs);

boolean_t asm_parse_number(char_t* data, uint64_t* result) {
    boolean_t is_hex = false;

    if(data[0] == '0' && data[1] == 'x') {
        is_hex = true;
    }

    if(is_hex) {
        *result = atoh(data + 2);

        return true;
    } else if((data[0] >= '0' && data[0] <= '9') || (data[0] == '-' && data[1] >= '0' && data[1] <= '9')) {
        *result = atoi(data);

        return true;
    }

    return false;
}

boolean_t asm_parse_memory_param(char_t* data, asm_instruction_param_t* param) {
    boolean_t end_paran = false;

    if(data[0] != '(') {
        // we have a displacement may be label or number
        buffer_t* disp = buffer_new_with_capacity(NULL, 1024);

        while(*data && *data != '(') {
            buffer_append_byte(disp, *data);
            data++;
        }

        buffer_append_byte(disp, '\0');

        char_t* disp_str = (char_t*)buffer_get_all_bytes(disp, NULL);

        buffer_destroy(disp);

        if(!asm_parse_number(disp_str, &param->displacement)) {
            param->label = disp_str;
        } else {
            memory_free(disp_str);

            // find size of displacement
            int64_t signed_disp = (int64_t)param->displacement;

            if(signed_disp >= INT8_MIN && signed_disp <= INT8_MAX) {
                param->signed_displacement_size = 8;
            } else if(signed_disp >= INT16_MIN && signed_disp <= INT16_MAX) {
                param->signed_displacement_size = 16;
            } else if(signed_disp >= INT32_MIN && signed_disp <= INT32_MAX) {
                param->signed_displacement_size = 32;
            } else {
                param->signed_displacement_size = 64;
            }

            if(param->displacement <= UINT8_MAX) {
                param->displacement_size = 8;
            } else if(param->displacement <= UINT16_MAX) {
                param->displacement_size = 16;
            } else if(param->displacement <= UINT32_MAX) {
                param->displacement_size = 32;
            } else {
                param->displacement_size = 64;
            }
        }
    }

    // if there is a sib data lets parse it. first emit whitespace
    while(*data && *data == ' ') {
        data++;
    }

    if(*data == '(') {
        data++;
    } else { // if there is no sib data we are done but result is successed if we have a displacement
        return param->displacement_size != 0 || param->label != NULL;
    }

    while(*data && *data == ' ') {
        data++;
    }

    if(*data && *data == '%') {
        data++;
    } else { // format is not correct
        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "format is not correct not reg %s\n", data);
        return false;
    }

    // now we have a register may be followed by comma or closing parenthesis lets extract it
    // put null terminator at the end of register string comma or closing parenthesis overwritten by null terminator
    char_t* base_reg_str = data;

    while(*data && *data != ',' && *data != ')') {
        data++;
    }

    if(*data == ')') {
        end_paran = true;
    } else {
        end_paran = false;
    }

    *data = '\0';

    if(!asm_parse_register_param(base_reg_str, ASM_REGISTER_TYPE_BASE, param)) {
        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "format is not correct not reg %s\n", base_reg_str);
        return false;
    }

    if(end_paran) {
        return true;
    }

    data++; // skip comma or closing parenthesis (we have overwritten it with null terminator)

    if(data) {
        // we may have index register clean spaces
        while(*data && *data == ' ') {
            data++;
        }

        if(*data == '%') {
            data++;
            // we have index register

            char_t* index_reg_str = data;

            while(*data && *data != ',' && *data != ')') {
                data++;
            }

            if(*data == ')') {
                end_paran = true;
            } else {
                end_paran = false;
            }

            *data = '\0';

            if(!asm_parse_register_param(index_reg_str, ASM_REGISTER_TYPE_INDEX, param)) {
                PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "format is not correct index not reg %s\n", index_reg_str);
                return false;
            }

            if(end_paran) {
                return true;
            }

            data++; // skip comma or closing parenthesis (we have overwritten it with null terminator)

            if(data) {
                // we need to check scale exists or closed parenthesis. first clean spaces
                while(*data && *data == ' ') {
                    data++;
                }

                if(*data == ')') {
                    param->scale = 1;
                    return true;
                }

                data[1] = '\0';

                if(!asm_parse_number(data, &param->scale)) {
                    PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "format is not correct not scale %s\n", data);
                    return false;
                }

                return true;

            } else {
                PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "format is not correct, end of token %s\n", data);
                return false;
            }
        } else if(*data == ')') {
            // we have no index register
            return true;
        } else {
            return false;
        }
    }

    return false;
}

boolean_t asm_parse_immediate_param(char_t* data, asm_instruction_param_t* param) {
    uint64_t immediate = 0;

    if(!asm_parse_number(data, &immediate)) {
        param->label = strdup(data);

        return true;
    }

    param->immediate = immediate;

    if(immediate <= UINT8_MAX) {
        param->immediate_size = 8;
    } else if(immediate <= UINT16_MAX) {
        param->immediate_size = 16;
    } else if(immediate <= UINT32_MAX) {
        param->immediate_size = 32;
    } else {
        param->immediate_size = 64;
    }

    int64_t signed_immediate = (int64_t)immediate;

    if(signed_immediate >= INT8_MIN && signed_immediate <= INT8_MAX) {
        param->signed_immediate_size = 8;
    } else if(signed_immediate >= INT16_MIN && signed_immediate <= INT16_MAX) {
        param->signed_immediate_size = 16;
    } else if(signed_immediate >= INT32_MIN && signed_immediate <= INT32_MAX) {
        param->signed_immediate_size = 32;
    } else {
        param->signed_immediate_size = 64;
    }

    return true;
}

boolean_t asm_parse_register_param(char_t* reg_str, uint8_t reg_idx, asm_instruction_param_t* param) {

    reg_str = strtrim_right(reg_str);

    for(uint8_t i = 0; i < ASM_REGISTER_COUNT; i++) {
        if(strcmp(reg_str, asm_register_map[i].name) == 0) {
            param->registers[reg_idx] = asm_register_map[i].reg;
            return true;
        }
    }

    PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Unknown register: -%s-", reg_str);

    return false;
}

boolean_t asm_parse_instruction_param(const asm_token_t* tok, asm_instruction_param_t* param) {
    if(tok->token_type != ASM_TOKEN_TYPE_PARAMETER) {
        return false;
    }

    char_t* data = tok->token_value;

    if(data[0] == '%') {
        param->type = ASM_INSTRUCTION_PARAM_TYPE_REGISTER;
        return asm_parse_register_param(data + 1, ASM_REGISTER_TYPE_NORMAL, param);
    } else if(data[0] == '$') {
        param->type = ASM_INSTRUCTION_PARAM_TYPE_IMMEDIATE;
        return asm_parse_immediate_param(data + 1, param);
    }

    param->type = ASM_INSTRUCTION_PARAM_TYPE_MEMORY;
    return asm_parse_memory_param(data, param);
}

boolean_t asm_encode_modrm_sib(asm_instruction_param_t op, boolean_t* need_sib, uint8_t* modrm, uint8_t* sib,
                               boolean_t* need_rex, uint8_t* rex,
                               boolean_t* has_displacement, uint8_t* disp_size) {
    if(op.scale) {
        *need_sib = true;
        uint8_t scale_bits = 0;

        if(op.scale == 1) {
            scale_bits = 0;
        } else if(op.scale == 2) {
            scale_bits = 1;
        } else if(op.scale == 4) {
            scale_bits = 2;
        } else if(op.scale == 8) {
            scale_bits = 3;
        } else {
            PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid scale %llx", op.scale);

            return false;
        }

        *sib |= scale_bits << 6;
    }

    *has_displacement = op.displacement_size != 0 || op.label != NULL;
    *disp_size = 0;

    if(*has_displacement) {
        *need_sib = true;
    }

    if(*need_sib) {
        boolean_t has_base = false;
        boolean_t has_index = false;

        if(op.registers[ASM_REGISTER_TYPE_BASE].register_size) {
            has_base = true;
            *sib |= op.registers[ASM_REGISTER_TYPE_BASE].register_index & 0x07;

            if(op.registers[ASM_REGISTER_TYPE_BASE].register_index > 7) {
                *need_rex = true;
                *rex |= 0x01;
            }
        }

        if(op.registers[ASM_REGISTER_TYPE_INDEX].register_size) {
            has_index = true;
            *sib |= (op.registers[ASM_REGISTER_TYPE_INDEX].register_index & 0x7) << 3;

            if(op.registers[ASM_REGISTER_TYPE_INDEX].register_index > 7) {
                *need_rex = true;
                *rex |= 0x02;
            }
        }

        if(has_base) {
            if(has_index) {
                if(*has_displacement) {
                    if(op.signed_displacement_size == 8) {
                        *modrm |= 0x40;
                        *disp_size = 1;
                    } else if(op.signed_displacement_size == 32 || op.label) {
                        *modrm |= 0x80;
                        *disp_size = 4;
                    } else {
                        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid displacement size %d", op.signed_displacement_size);

                        return false;
                    }
                } else {

                }
            } else {
                if(*has_displacement) {
                    *need_sib = false;
                    // *sib |= 0x20;

                    *modrm |= op.registers[ASM_REGISTER_TYPE_BASE].register_index & 0x07;

                    if(op.signed_displacement_size == 8) {
                        *modrm |= 0x40;
                        *disp_size = 1;
                    } else if(op.signed_displacement_size == 16 || op.signed_displacement_size == 32 || op.label) {
                        *modrm |= 0x80;
                        *disp_size = 4;
                    } else {
                        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid displacement size %d", op.signed_displacement_size);

                        return false;
                    }
                } else {
                    *sib |= 0x20;
                }
            }
        } else {
            if(has_index) {
                if(*has_displacement) {
                    *sib |= 0x05;

                    if(op.signed_displacement_size == 8) {
                        *modrm |= 0x40;
                    } else if(op.signed_displacement_size == 32) {
                        *modrm |= 0x80;
                    } else {
                        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid displacement size %d", op.signed_displacement_size);

                        return false;
                    }
                } else {
                    PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid addressing mode");

                    return false;
                }
            } else {
                if(*has_displacement) {
                    *sib |= 0x25;
                } else {
                    PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid addressing mode");

                    return false;
                }
            }
        }
    }

    return true;
}



boolean_t asm_encode_instructions(list_t* tokens, buffer_t* out, list_t* relocs) {
    iterator_t* it = list_iterator_create(tokens);

    boolean_t result = true;

    while(it->end_of_iterator(it) != 0) {
        const asm_token_t* tok = it->get_item(it);

        if(tok->token_type == ASM_TOKEN_TYPE_INSTRUCTION) {
            if(strcmp("lock", tok->token_value) == 0) {
                buffer_append_byte(out, ASM_INSTRUCTION_PREFIX_LOCK);

                it = it->next(it);

                continue;
            }

            if(strcmp("rep", tok->token_value) == 0 || strcmp("repe", tok->token_value) == 0 || strcmp("repz", tok->token_value) == 0) {
                buffer_append_byte(out, ASM_INSTRUCTION_PREFIX_REP);

                it = it->next(it);

                continue;
            }

            if(strcmp("repne", tok->token_value) == 0 || strcmp("repnz", tok->token_value) == 0) {
                buffer_append_byte(out, ASM_INSTRUCTION_PREFIX_REPNE);

                it = it->next(it);

                continue;
            }

            if(!asm_encode_instruction(it, out, relocs)) {
                result = false;

                break;
            }

        } else {
            result = false;

            break;
        }

        // it = it->next(it);
    }

    it->destroy(it);

    return result;
}

void asm_encoder_print_relocs(list_t* relocs) {
    iterator_t* iter = list_iterator_create(relocs);

    while(iter->end_of_iterator(iter) != 0) {
        const asm_relocation_t* reloc = iter->get_item(iter);

        printf("reloc %i %s %llx %llx\n", reloc->type, reloc->label, reloc->offset, reloc->addend);

        iter = iter->next(iter);
    }

    iter->destroy(iter);
}

void asm_encoder_destroy_relocs(list_t* relocs) {
    iterator_t* iter = list_iterator_create(relocs);

    while(iter->end_of_iterator(iter) != 0) {
        const asm_relocation_t* reloc = iter->get_item(iter);

        memory_free((void*)reloc->label);

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    list_destroy_with_data(relocs);
}

boolean_t asm_encode_instruction(iterator_t* it, buffer_t* outbuf, list_t* relocs) {
    boolean_t result = true;

    const asm_token_t* tok_operand = it->get_item(it);

    char_t* mnemonic_str = strdup(tok_operand->token_value);
    uint64_t mnemonic_str_len = strlen(mnemonic_str) - 1;
    uint8_t operand_size = 0;
    uint8_t mem_operand_size = 0;

    if(strends(mnemonic_str, "b") == 0) {
        operand_size = 8;
        mem_operand_size = 8;
        mnemonic_str[mnemonic_str_len] = '\0';
    } else if(strends(mnemonic_str, "w") == 0) {
        operand_size = 16;
        mem_operand_size = 16;
        mnemonic_str[mnemonic_str_len] = '\0';
    } else if(strends(mnemonic_str, "l") == 0) {
        operand_size = 32;
        mem_operand_size = 32;
        mnemonic_str[mnemonic_str_len] = '\0';
    } else if(strends(mnemonic_str, "q") == 0) {
        operand_size = 64;
        mem_operand_size = 64;
        mnemonic_str[mnemonic_str_len] = '\0';
    }


    const asm_instruction_mnemonic_map_t* map = asm_instruction_mnemonic_get(mnemonic_str);

    if(map == NULL) {
        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid instruction mnemonic %s", mnemonic_str);

        memory_free(mnemonic_str);

        return false;
    }

    asm_instruction_param_t params[4] = {0};
    uint8_t param_count = 0;

    while(true) {
        it = it->next(it);

        if(it->end_of_iterator(it) == 0) {
            break;
        }

        const asm_token_t* tok = it->get_item(it);

        if(tok->token_type != ASM_TOKEN_TYPE_PARAMETER) {
            break;
        }

        if(param_count >= 4) {
            PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Too many parameters");

            memory_free(mnemonic_str);

            return false;
        }


        if(!asm_parse_instruction_param(tok, &params[param_count])) {
            PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to parse instruction parameter op1 %s %i", tok->token_value, tok->token_type);

            memory_free(mnemonic_str);

            return false;
        }

        param_count++;
    }

    asm_instruction_mnemonic_t mnemonics[5] = {0};
    mnemonics[0] = map->mnemonic;

    uint8_t idx = 1;

    if(operand_size == 0 && params[param_count - 1].type == ASM_INSTRUCTION_PARAM_TYPE_REGISTER) {
        operand_size = params[param_count - 1].registers[0].register_size;
    }

    if(operand_size == 0) {
        operand_size = map->default_operand_size;
    }

    if(operand_size > map->max_operand_size) {
        operand_size = map->max_operand_size;
    }

    if(mem_operand_size == 0) {
        mem_operand_size = map->default_mem_operand_size;
    }

    if(mem_operand_size == 0 && params[param_count - 1].type == ASM_INSTRUCTION_PARAM_TYPE_MEMORY) {
        mem_operand_size = 64;
    }

    if(mem_operand_size == 0 && params[param_count - 1].type == ASM_INSTRUCTION_PARAM_TYPE_REGISTER) {
        mem_operand_size = params[param_count - 1].registers[0].register_size;
    }

    for(int64_t i = param_count - 1; i >= 0; i--) {
        mnemonics[idx] = asm_instruction_mnemonic_get_by_param(&params[i], operand_size, mem_operand_size);

        idx++;
    }

    const asm_instruction_t* instr = asm_instruction_get(map, param_count + 1, mnemonics);

    if(instr == NULL) {
        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to find instruction");

        memory_free(mnemonic_str);

        for(int64_t i = 0; i < param_count + 1; i++) {
            printf("mnemonic %i\n", mnemonics[i]);
        }

        return false;
    }

    memory_free(mnemonic_str);


    if(instr->has_operand_size_override) {
        buffer_append_byte(outbuf, ASM_INSTRUCTION_PREFIX_OPERAND_SIZE_OVERRIDE);
    }

    if(instr->has_address_size_override) {
        buffer_append_byte(outbuf, ASM_INSTRUCTION_PREFIX_ADDRESS_SIZE_OVERRIDE);
    }

    boolean_t need_rex = instr->has_rex;
    uint8_t rex = 0;

    if(need_rex) {
        rex |= 0x40;

        if(instr->has_rex_w) {
            rex |= 0x08;
        }
    }

    uint8_t modrm = 0;
    boolean_t need_sib = false;
    uint8_t sib = 0;

    boolean_t has_displacement = false;
    uint8_t disp_size = 0;


    asm_instruction_param_t op_mem = {0};
    asm_instruction_param_t op_regs[2] = {0};
    asm_instruction_param_t op_imm = {0};
    uint8_t op_reg_count = 0;

    boolean_t has_mem_operand = false;
    boolean_t has_imm_operand = false;

    for(int64_t i = 0; i < param_count; i++) {
        if(params[i].type == ASM_INSTRUCTION_PARAM_TYPE_MEMORY) {
            op_mem = params[i];
            has_mem_operand = true;
        } else if(params[i].type == ASM_INSTRUCTION_PARAM_TYPE_REGISTER) {
            op_regs[op_reg_count++] = params[i];
        } else if(params[i].type == ASM_INSTRUCTION_PARAM_TYPE_IMMEDIATE) {
            op_imm = params[i];
            has_imm_operand = true;
        }
    }

    if(has_mem_operand && !asm_encode_modrm_sib(op_mem, &need_sib, &modrm, &sib, &need_rex, &rex, &has_displacement, &disp_size)) {
        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to encode modrm/sib");

        return false;
    }

    if(!has_mem_operand) {
        modrm |= 0xc0;
    }

    if(instr->has_modrm) {
        if(instr->modrm != 'r') {
            modrm |= (instr->modrm & 0x07) << 3;

            if(!has_mem_operand && op_reg_count == 1) {
                modrm |= (op_regs[0].registers[0].register_index & 0x07);

                if(op_regs[0].registers[0].register_index > 7) {
                    need_rex = true;
                    rex |= 0x01;
                }
            }

            if(has_mem_operand && need_sib) {
                modrm |= 0x04;
            }

            if(has_mem_operand && !need_sib) {
                modrm |= op_mem.registers[ASM_REGISTER_TYPE_BASE].register_index & 0x07;

                if(op_mem.registers[ASM_REGISTER_TYPE_BASE].register_index > 7) {
                    need_rex = true;
                    rex |= 0x01;
                }
            }

        } else {
            if(op_reg_count == 0) {
                PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Instruction requires register operand");

                return false;
            } else if(op_reg_count == 1) {
                modrm |= (op_regs[0].registers[0].register_index & 0x07) << 3;

                if(need_sib) {
                    modrm |= 0x04;
                }

                if(op_regs[0].registers[0].register_index > 7) {
                    need_rex = true;
                    rex |= 0x04;
                }
            } else {
                int64_t modrm_reg = 0;
                int64_t modrm_rm = 0;

                if(instr->operand_encode == ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG) {
                    modrm_reg = 0;
                    modrm_rm = 1;
                } else if(instr->operand_encode == ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM) {
                    modrm_reg = 1;
                    modrm_rm = 0;
                } else {
                    PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid operand encoding");

                    return false;
                }

                modrm |= (op_regs[modrm_reg].registers[0].register_index & 0x07) << 3 | (op_regs[modrm_rm].registers[0].register_index & 0x07);

                if(op_regs[modrm_rm].registers[0].register_index > 7) {
                    need_rex = true;
                    rex |= 0x01;
                }

                if(op_regs[modrm_reg].registers[0].register_index > 7) {
                    need_rex = true;
                    rex |= 0x04;
                }
            }
        }
    } else if(op_reg_count == 1) {
        if(op_regs[0].registers[0].register_index > 7) {
            need_rex = true;
            rex |= 0x04;
        }
    } else if(instr->instruction_length > 1) {
        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Instruction does not support register operand");

        return false;
    }

    if(need_rex) {
        rex |= 0x40;
        buffer_append_byte(outbuf, rex);
    }

    buffer_append_bytes(outbuf, (uint8_t*)instr->opcodes, instr->opcode_length);

    if(instr->has_modrm) {
        buffer_append_byte(outbuf, modrm);
    }

    if(need_sib) {
        buffer_append_byte(outbuf, sib);
    }

    if(has_displacement) {
        if(disp_size == 0) {
            disp_size = op_mem.signed_displacement_size == 8?1:4;

            if(disp_size == 0) {
                disp_size = operand_size == 8?1:4;
            }
        }

        if(op_mem.label) {
            asm_relocation_t* reloc = memory_malloc(sizeof(asm_relocation_t));

            if(reloc == NULL) {
                PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to allocate memory for relocation");

                return false;
            }

            reloc->addend = 0;
            reloc->offset = buffer_get_position(outbuf);
            reloc->type = disp_size == 1?LINKER_RELOCATION_TYPE_64_8:
                          disp_size == 4?LINKER_RELOCATION_TYPE_64_32:LINKER_RELOCATION_TYPE_64_64;
            reloc->label = op_mem.label;

            list_queue_push(relocs, reloc);
        }

        buffer_append_bytes(outbuf, (uint8_t*)&op_mem.displacement, disp_size);
    }

    if(has_imm_operand) {
        uint8_t imm_size = asm_instruction_get_imm_size(instr);

        if(op_imm.label) {
            asm_relocation_t* reloc = memory_malloc(sizeof(asm_relocation_t));

            if(reloc == NULL) {
                PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to allocate memory for relocation");

                return false;
            }

            reloc->type = imm_size == 1?LINKER_RELOCATION_TYPE_64_8:
                          imm_size == 4?LINKER_RELOCATION_TYPE_64_32:LINKER_RELOCATION_TYPE_64_32S;
            reloc->offset = buffer_get_position(outbuf);
            reloc->label = op_imm.label;

            list_queue_push(relocs, reloc);
        }
        buffer_append_bytes(outbuf, (uint8_t*)&op_imm.immediate, imm_size);

    }

    return result;
}
