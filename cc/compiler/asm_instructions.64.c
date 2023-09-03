/**
 * @file asm_instructions.64.c
 * @brief Test for asm instructions
 */

#include <compiler/asm_instructions.h>
#include <strings.h>

const asm_instruction_mnemonic_map_t asm_instruction_mnemonic_map[] = {
    {"adc", ASM_INSTRUCTION_MNEMONIC_ADC, 3, 3, 32, 32},
    {NULL, ASM_INSTRUCTION_MNEMONIC_NULL, 0, 0, 0, 0},
};

const asm_instruction_t asm_instructions[] = {
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_AL, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x14, }, false, false, 0, false, false, false, false, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_AX, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x15, }, false, false, 0, true, false, false, false, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_EAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x15, }, false, false, 0, false, false, false, false, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_RAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x15, }, false, false, 0, false, false, true, true, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 2, false, false, false, false, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80}, true, false, 2, false, false, true, false, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x81, }, true, false, 2, true, false, false, false, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 2, false, false, false, false, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 2, false, false, true, true, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83}, true, false, 2, true, false, false, false, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 2, false, false, false, false, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 2, false, false, true, true, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x10, }, true, false, 'r', false, false, false, false, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x10, }, true, false, 'r', false, false, true, false, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_R_16},
     1, {0x11}, true, false, 'r', true, false, false, false, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_R_32},
     1, {0x11, }, true, false, 'r', false, false, false, false, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_R_64},
     1, {0x11, }, true, false, 'r', false, false, true, true, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x12, }, true, false, 'r', false, false, false, false, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x12, }, true, false, 'r', false, false, true, false, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_16, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16},
     1, {0x13, }, true, false, 'r', true, false, false, false, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_32, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32},
     1, {0x13, }, true, false, 'r', false, false, false, false, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_64, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64},
     1, {0x13, }, true, false, 'r', false, false, true, true, },


    // end of table
    {NULL, false, false, 0, {0, }, 0, {0, }, false, false, 0, false, false, false, false, },
};


asm_instruction_mnemonic_t asm_instruction_mnemonic_get_by_param(asm_instruction_param_t* param, uint8_t operand_size, uint8_t mem_operand_size) {
    if(param->type == ASM_INSTRUCTION_PARAM_TYPE_IMMEDIATE) {
        uint8_t imm_size = param->immediate_size;

        if(param->label) {
            imm_size = 64;
        }

        if(imm_size > operand_size) {
            imm_size = operand_size;
        }

        switch(imm_size) {
        case 8:
            return ASM_INSTRUCTION_MNEMONIC_IMM_8;
        case 16:
            return ASM_INSTRUCTION_MNEMONIC_IMM_16;
        case 32:
            return ASM_INSTRUCTION_MNEMONIC_IMM_32;
        case 64:
            return ASM_INSTRUCTION_MNEMONIC_IMM_64;
        default:
            return ASM_INSTRUCTION_MNEMONIC_NULL;
        }
    }

    if(param->type == ASM_INSTRUCTION_PARAM_TYPE_REGISTER) {
        if(param->registers[0].register_index == 0) {
            // rax, eax, ax, al
            switch(param->registers[0].register_size) {
            case 8:
                return ASM_INSTRUCTION_MNEMONIC_AL;
            case 16:
                return ASM_INSTRUCTION_MNEMONIC_AX;
            case 32:
                return ASM_INSTRUCTION_MNEMONIC_EAX;
            case 64:
                return ASM_INSTRUCTION_MNEMONIC_RAX;
            default:
                return ASM_INSTRUCTION_MNEMONIC_NULL;
            }
        }

        switch(param->registers[0].register_size) {
        case 8:
            return ASM_INSTRUCTION_MNEMONIC_R_8;
        case 16:
            return ASM_INSTRUCTION_MNEMONIC_R_16;
        case 32:
            return ASM_INSTRUCTION_MNEMONIC_R_32;
        case 64:
            return ASM_INSTRUCTION_MNEMONIC_R_64;
        default:
            return ASM_INSTRUCTION_MNEMONIC_NULL;
        }
    }

    switch(mem_operand_size) {
    case 8:
        return ASM_INSTRUCTION_MNEMONIC_M_8;
    case 16:
        return ASM_INSTRUCTION_MNEMONIC_M_16;
    case 32:
        return ASM_INSTRUCTION_MNEMONIC_M_32;
    case 64:
        return ASM_INSTRUCTION_MNEMONIC_M_64;
    default:
        return ASM_INSTRUCTION_MNEMONIC_NULL;
    }

    return ASM_INSTRUCTION_MNEMONIC_NULL;
}

const asm_instruction_mnemonic_map_t* asm_instruction_mnemonic_get(const char_t* mnemonic_string) {
    const asm_instruction_mnemonic_map_t* map = asm_instruction_mnemonic_map;

    while (map->mnemonic_string != NULL) {
        if (strcmp(map->mnemonic_string, mnemonic_string) == 0) {
            return map;
        }

        map++;
    }

    return NULL;
}

const asm_instruction_t* asm_instruction_get(const asm_instruction_mnemonic_map_t* map, uint8_t instruction_length, asm_instruction_mnemonic_t mnemonics[5]) {
    if (map == NULL) {
        return NULL;
    }

    if (map->min_opcode_count > instruction_length || map->max_opcode_count < instruction_length) {
        return NULL;
    }

    for (uint8_t i = 0; asm_instructions[i].general_mnemonic != NULL; i++) {
        if(asm_instructions[i].instruction_length == instruction_length) {
            boolean_t found = true;

            for (uint8_t j = 0; j < instruction_length; j++) {
                if (asm_instructions[i].mnemonics[j] != mnemonics[j]) {

                    if(asm_instructions[i].mnemonics[j] <= ASM_INSTRUCTION_MNEMONIC_IMM_64 &&
                       mnemonics[j] <= ASM_INSTRUCTION_MNEMONIC_IMM_64 &&
                       asm_instructions[i].mnemonics[j] >= mnemonics[j]) {
                        continue;
                    }

                    if(mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_AL &&
                       (asm_instructions[i].mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_R_8 || asm_instructions[i].mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_R_OR_M_8)) {
                        continue;
                    } else if(mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_AX &&
                              (asm_instructions[i].mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_R_16 || asm_instructions[i].mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_R_OR_M_16)) {
                        continue;
                    } else if(mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_EAX &&
                              (asm_instructions[i].mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_R_32 || asm_instructions[i].mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_R_OR_M_32)) {
                        continue;
                    } else if(mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_RAX &&
                              (asm_instructions[i].mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_R_64 || asm_instructions[i].mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_R_OR_M_64)) {
                        continue;
                    }

                    int16_t real_mnemonic_idx = asm_instructions[i].mnemonics[j] - ASM_INSTRUCTION_MNEMONIC_R_OR_M_8;

                    if (real_mnemonic_idx < 0 || real_mnemonic_idx > 3) {
                        found = false;
                        break;
                    }


                    int16_t req_mnemonic_idx_for_mem = mnemonics[j] - ASM_INSTRUCTION_MNEMONIC_M_8;
                    int16_t req_mnemonic_idx_for_reg = mnemonics[j] - ASM_INSTRUCTION_MNEMONIC_R_8;

                    if ((req_mnemonic_idx_for_mem < 0 || req_mnemonic_idx_for_mem > 3) && (req_mnemonic_idx_for_reg < 0 || req_mnemonic_idx_for_reg > 3)) {
                        found = false;
                        break;
                    }

                    if((real_mnemonic_idx == req_mnemonic_idx_for_mem) || (real_mnemonic_idx == req_mnemonic_idx_for_reg)) {
                        continue;
                    }

                    found = false;
                    break;
                }
            }

            if (found) {
                return &asm_instructions[i];
            }
        }
    }

    return NULL;
}

uint8_t asm_instruction_get_imm_size(const asm_instruction_t* instr) {
    if(instr == NULL) {
        return 0;
    }

    uint8_t imm_size = 0;

    for(uint8_t i = 0; i < instr->instruction_length; i++) {
        if(instr->mnemonics[i] >= ASM_INSTRUCTION_MNEMONIC_IMM_8 && instr->mnemonics[i] <= ASM_INSTRUCTION_MNEMONIC_IMM_64) {
            imm_size = instr->mnemonics[i] - ASM_INSTRUCTION_MNEMONIC_IMM_8;
        }
    }

    return (1 << (imm_size + 3)) / 8;
}
