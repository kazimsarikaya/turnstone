/**
 * @file asm_insts_r.64.c
 * @brief x86_64 assembly instruction mnemonics starting with 'r'
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/asm_instructions.h>

MODULE("turnstone.compiler.assembler");

const asm_instruction_t asm_instructions_r[] = {
    {"rdmsr", true, true,
     1, {ASM_INSTRUCTION_MNEMONIC_RDMSR, },
     2, {0x0f, 0x32, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },

    {"ret", true, true,
     1, {ASM_INSTRUCTION_MNEMONIC_RET, },
     1, {0xc3, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"ret", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_RET, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0xc2, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },

    // end of table
    {NULL, false, false, 0, {0, }, 0, {0, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },

};
