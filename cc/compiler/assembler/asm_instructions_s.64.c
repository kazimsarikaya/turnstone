/**
 * @file asm_insts_s.64.c
 * @brief x86_64 assembly instruction mnemonics starting with 's'
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/asm_instructions.h>

MODULE("turnstone.compiler.assembler");

const asm_instruction_t asm_instructions_s[] = {
    {"sgdt", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_SGDT, ASM_INSTRUCTION_MNEMONIC_M_64},
     2, {0x0f, 0x01, }, true, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM, },

    {"sidt", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_SIDT, ASM_INSTRUCTION_MNEMONIC_M_64},
     2, {0x0f, 0x01, }, true, false, 1, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM, },


    {"sti", true, true,
     1, {ASM_INSTRUCTION_MNEMONIC_STI, },
     1, {0xfb, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },

    {"str", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_STR, ASM_INSTRUCTION_MNEMONIC_M_16},
     2, {0x0f, 0x00, }, true, false, 1, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM, },
    {"str", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_STR, ASM_INSTRUCTION_MNEMONIC_R_16},
     2, {0x0f, 0x00, }, true, false, 1, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM, },


    {"sub", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_AL, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x2c, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"sub", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_AX, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x2d, }, false, false, 0, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"sub", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_EAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x2d, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"sub", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_RAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x2d, }, false, false, 0, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"sub", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 5, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"sub", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 5, false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"sub", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x81, }, true, false, 5, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"sub", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 5, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"sub", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 5, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"sub", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 5, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"sub", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 5, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"sub", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 5, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"sub", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x28, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"sub", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x28, }, true, false, 'r', false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"sub", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_R_16},
     1, {0x29, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"sub", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_R_32},
     1, {0x29, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"sub", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_R_64},
     1, {0x29, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"sub", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x2a, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"sub", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x2a, }, true, false, 'r', false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"sub", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_16, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16},
     1, {0x2b, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"sub", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_32, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32},
     1, {0x2b, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"sub", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_SUB, ASM_INSTRUCTION_MNEMONIC_R_64, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64},
     1, {0x2b, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },

    // end of table
    {NULL, false, false, 0, {0, }, 0, {0, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },

};
