/**
 * @file asm_insts_o.64.c
 * @brief x86_64 assembly instruction mnemonics starting with 'o'
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/asm_instructions.h>

MODULE("turnstone.compiler.assembler");

const asm_instruction_t asm_instructions_o[] = {
    {"or", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_AL, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x0c, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"or", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_AX, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x0d, }, false, false, 0, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"or", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_EAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x0d, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"or", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_RAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x0d, }, false, false, 0, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"or", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 1, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"or", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 1, false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"or", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x81, }, true, false, 1, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"or", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 1, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"or", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 1, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"or", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 1, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"or", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 1, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"or", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 1, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"or", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x08, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"or", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x08, }, true, false, 'r', false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"or", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_R_16},
     1, {0x09, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"or", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_R_32},
     1, {0x09, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"or", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_R_64},
     1, {0x09, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"or", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x0a, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"or", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x0a, }, true, false, 'r', false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"or", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_16, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16},
     1, {0x0b, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"or", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_32, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32},
     1, {0x0b, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"or", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_OR, ASM_INSTRUCTION_MNEMONIC_R_64, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64},
     1, {0x0b, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },

    // end of table
    {NULL, false, false, 0, {0, }, 0, {0, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },

};
