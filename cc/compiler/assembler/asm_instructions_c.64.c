/**
 * @file asm_insts_c.64.c
 * @brief x86_64 assembly instruction mnemonics starting with 'c'
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/asm_instructions.h>

MODULE("turnstone.compiler.assembler");

const asm_instruction_t asm_instructions_c[] = {
    {"cli", true, true,
     1, {ASM_INSTRUCTION_MNEMONIC_CLI, },
     1, {0xfa, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },



    {"cmp", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_AL, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x3c, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"cmp", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_AX, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x3d, }, false, false, 0, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"cmp", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_EAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x3d, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"cmp", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_RAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x3d, }, false, false, 0, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"cmp", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 7, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"cmp", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 7, false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"cmp", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x81, }, true, false, 7, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"cmp", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 7, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"cmp", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 7, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"cmp", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 7, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"cmp", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 7, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"cmp", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 7, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"cmp", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x38, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"cmp", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x38, }, true, false, 'r', false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"cmp", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_R_16},
     1, {0x39, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"cmp", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_R_32},
     1, {0x39, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"cmp", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_R_64},
     1, {0x39, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"cmp", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x3a, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"cmp", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x3a, }, true, false, 'r', false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"cmp", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_16, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16},
     1, {0x3b, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"cmp", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_32, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32},
     1, {0x3b, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"cmp", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_CMP, ASM_INSTRUCTION_MNEMONIC_R_64, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64},
     1, {0x3b, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },



    {"cpuid", true, true,
     1, {ASM_INSTRUCTION_MNEMONIC_CPUID, },
     2, {0x0f, 0xa2, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },

    // end of table
    {NULL, false, false, 0, {0, }, 0, {0, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },

};
