/**
 * @file asm_instructions.64.c
 * @brief Test for asm instructions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/asm_instructions.h>
#include <strings.h>
#include <logging.h>

MODULE("turnstone.compiler.assembler");


extern const asm_instruction_mnemonic_map_t asm_instruction_mnemonic_map[];
extern const asm_instruction_t asm_instructions_a[];
extern const asm_instruction_t asm_instructions_b[];
extern const asm_instruction_t asm_instructions_c[];
extern const asm_instruction_t asm_instructions_d[];
extern const asm_instruction_t asm_instructions_e[];
extern const asm_instruction_t asm_instructions_f[];
extern const asm_instruction_t asm_instructions_g[];
extern const asm_instruction_t asm_instructions_h[];
extern const asm_instruction_t asm_instructions_i[];
extern const asm_instruction_t asm_instructions_j[];
extern const asm_instruction_t asm_instructions_k[];
extern const asm_instruction_t asm_instructions_l[];
extern const asm_instruction_t asm_instructions_m[];
extern const asm_instruction_t asm_instructions_n[];
extern const asm_instruction_t asm_instructions_o[];
extern const asm_instruction_t asm_instructions_p[];
extern const asm_instruction_t asm_instructions_q[];
extern const asm_instruction_t asm_instructions_r[];
extern const asm_instruction_t asm_instructions_s[];
extern const asm_instruction_t asm_instructions_t[];
extern const asm_instruction_t asm_instructions_u[];
extern const asm_instruction_t asm_instructions_v[];
extern const asm_instruction_t asm_instructions_w[];
extern const asm_instruction_t asm_instructions_x[];
extern const asm_instruction_t asm_instructions_y[];
extern const asm_instruction_t asm_instructions_z[];

const asm_instruction_t*const asm_instructions_map[] = {
    asm_instructions_a,
    asm_instructions_b,
    asm_instructions_c,
    asm_instructions_d,
    asm_instructions_e,
    asm_instructions_f,
    asm_instructions_g,
    asm_instructions_h,
    asm_instructions_i,
    asm_instructions_j,
    asm_instructions_k,
    asm_instructions_l,
    asm_instructions_m,
    asm_instructions_n,
    asm_instructions_o,
    asm_instructions_p,
    asm_instructions_q,
    asm_instructions_r,
    asm_instructions_s,
    asm_instructions_t,
    asm_instructions_u,
    asm_instructions_v,
    asm_instructions_w,
    asm_instructions_x,
    asm_instructions_y,
    asm_instructions_z,
};

asm_instruction_mnemonic_t asm_instruction_mnemonic_get_by_param(asm_instruction_param_t* param, uint8_t operand_size, uint8_t mem_operand_size) {
    PRINTLOG(COMPILER_ASSEMBLER, LOG_TRACE, "param->type: %d", param->type);

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
        if(param->registers[0].is_control) {
            switch(param->registers[0].register_index) {
            case 0:
                return ASM_INSTRUCTION_MNEMONIC_CR0;
            case 1:
                return ASM_INSTRUCTION_MNEMONIC_CR1;
            case 2:
                return ASM_INSTRUCTION_MNEMONIC_CR2;
            case 3:
                return ASM_INSTRUCTION_MNEMONIC_CR3;
            case 4:
                return ASM_INSTRUCTION_MNEMONIC_CR4;
            case 5:
                return ASM_INSTRUCTION_MNEMONIC_CR5;
            case 6:
                return ASM_INSTRUCTION_MNEMONIC_CR6;
            case 7:
                return ASM_INSTRUCTION_MNEMONIC_CR7;
            case 8:
                return ASM_INSTRUCTION_MNEMONIC_CR8;
            case 9:
                return ASM_INSTRUCTION_MNEMONIC_CR9;
            case 10:
                return ASM_INSTRUCTION_MNEMONIC_CR10;
            case 11:
                return ASM_INSTRUCTION_MNEMONIC_CR11;
            case 12:
                return ASM_INSTRUCTION_MNEMONIC_CR12;
            case 13:
                return ASM_INSTRUCTION_MNEMONIC_CR13;
            case 14:
                return ASM_INSTRUCTION_MNEMONIC_CR14;
            case 15:
                return ASM_INSTRUCTION_MNEMONIC_CR15;
            default:
                return ASM_INSTRUCTION_MNEMONIC_NULL;
            }
        }

        if(param->registers[0].is_debug) {
            switch(param->registers[0].register_index) {
            case 0:
                return ASM_INSTRUCTION_MNEMONIC_DR0;
            case 1:
                return ASM_INSTRUCTION_MNEMONIC_DR1;
            case 2:
                return ASM_INSTRUCTION_MNEMONIC_DR2;
            case 3:
                return ASM_INSTRUCTION_MNEMONIC_DR3;
            case 4:
                return ASM_INSTRUCTION_MNEMONIC_DR4;
            case 5:
                return ASM_INSTRUCTION_MNEMONIC_DR5;
            case 6:
                return ASM_INSTRUCTION_MNEMONIC_DR6;
            case 7:
                return ASM_INSTRUCTION_MNEMONIC_DR7;
            case 8:
                return ASM_INSTRUCTION_MNEMONIC_DR8;
            case 9:
                return ASM_INSTRUCTION_MNEMONIC_DR9;
            case 10:
                return ASM_INSTRUCTION_MNEMONIC_DR10;
            case 11:
                return ASM_INSTRUCTION_MNEMONIC_DR11;
            case 12:
                return ASM_INSTRUCTION_MNEMONIC_DR12;
            case 13:
                return ASM_INSTRUCTION_MNEMONIC_DR13;
            case 14:
                return ASM_INSTRUCTION_MNEMONIC_DR14;
            case 15:
                return ASM_INSTRUCTION_MNEMONIC_DR15;
            default:
                return ASM_INSTRUCTION_MNEMONIC_NULL;
            }
        }

        if(param->registers[0].is_segment) {
            switch(param->registers[0].register_index) {
            case 0:
                return ASM_INSTRUCTION_MNEMONIC_ES;
            case 1:
                return ASM_INSTRUCTION_MNEMONIC_CS;
            case 2:
                return ASM_INSTRUCTION_MNEMONIC_SS;
            case 3:
                return ASM_INSTRUCTION_MNEMONIC_DS;
            case 4:
                return ASM_INSTRUCTION_MNEMONIC_FS;
            case 5:
                return ASM_INSTRUCTION_MNEMONIC_GS;
            default:
                return ASM_INSTRUCTION_MNEMONIC_NULL;
            }
        }

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

        if(param->registers[0].register_size == 128) {
            switch(param->registers[0].register_index) {
            case 0:
                return ASM_INSTRUCTION_MNEMONIC_XMM0;
            case 1:
                return ASM_INSTRUCTION_MNEMONIC_XMM1;
            case 2:
                return ASM_INSTRUCTION_MNEMONIC_XMM2;
            default:
                break;
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
        case 128:
            return ASM_INSTRUCTION_MNEMONIC_R_128;
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
    case 128:
        return ASM_INSTRUCTION_MNEMONIC_M_128;
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

    uint32_t idx = map->mnemonic_string[0] - 'a';

    if (idx > 25) {
        return NULL;
    }

    const asm_instruction_t* asm_instructions = asm_instructions_map[idx];

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

                    if(asm_instructions[i].mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_R_128 &&
                       mnemonics[j] >= ASM_INSTRUCTION_MNEMONIC_XMM1 && mnemonics[j] <= ASM_INSTRUCTION_MNEMONIC_XMM2) {
                        continue;
                    }

                    if(asm_instructions[i].mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_XMM2_OR_MEM_64) {
                        if(mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_XMM2 ||
                           mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_M_64) {
                            continue;
                        }
                    } else if(asm_instructions[i].mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_XMM2_OR_MEM_128) {
                        if(mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_XMM2 ||
                           mnemonics[j] == ASM_INSTRUCTION_MNEMONIC_M_128) {
                            continue;
                        }
                    }

                    int16_t real_mnemonic_idx = asm_instructions[i].mnemonics[j] - ASM_INSTRUCTION_MNEMONIC_R_OR_M_8;

                    if (real_mnemonic_idx < 0 || real_mnemonic_idx > 4) {
                        found = false;
                        break;
                    }


                    int16_t req_mnemonic_idx_for_mem = mnemonics[j] - ASM_INSTRUCTION_MNEMONIC_M_8;
                    int16_t req_mnemonic_idx_for_reg = mnemonics[j] - ASM_INSTRUCTION_MNEMONIC_R_8;

                    if ((req_mnemonic_idx_for_mem < 0 || req_mnemonic_idx_for_mem > 4) && (req_mnemonic_idx_for_reg < 0 || req_mnemonic_idx_for_reg > 4)) {
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
