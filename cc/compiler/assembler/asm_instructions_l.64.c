/**
 * @file asm_insts_l.64.c
 * @brief x86_64 assembly instruction mnemonics starting with 'l'
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/asm_instructions.h>

MODULE("turnstone.compiler.assembler");

const asm_instruction_t asm_instructions_l[] = {
    {"lea", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_LEA, ASM_INSTRUCTION_MNEMONIC_R_16, ASM_INSTRUCTION_MNEMONIC_M_16},
     1, {0x8d, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"lea", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_LEA, ASM_INSTRUCTION_MNEMONIC_R_32, ASM_INSTRUCTION_MNEMONIC_M_32},
     1, {0x8d, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"lea", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_LEA, ASM_INSTRUCTION_MNEMONIC_R_64, ASM_INSTRUCTION_MNEMONIC_M_64},
     1, {0x8d, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },

    {"leave", true, true,
     1, {ASM_INSTRUCTION_MNEMONIC_LEAVE, },
     1, {0xc9, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },

    {"lgdt", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_LGDT, ASM_INSTRUCTION_MNEMONIC_M_64},
     2, {0x0f, 0x01, }, true, false, 2, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM, },

    {"lidt", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_LIDT, ASM_INSTRUCTION_MNEMONIC_M_64},
     2, {0x0f, 0x01, }, true, false, 3, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM, },

    {"lret", true, true,
     1, {ASM_INSTRUCTION_MNEMONIC_LRET, },
     1, {0xcb, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"lret", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_LRET, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0xca, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },

    {"ltr", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_LTR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16},
     2, {0x0f, 0x00, }, true, false, 3, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM, },


    // end of table
    {NULL, false, false, 0, {0, }, 0, {0, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
};
