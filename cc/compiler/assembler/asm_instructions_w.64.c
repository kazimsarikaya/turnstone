/**
 * @file asm_insts_w.64.c
 * @brief x86_64 assembly instruction mnemonics starting with 'w'
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/asm_instructions.h>

MODULE("turnstone.compiler.assembler");

const asm_instruction_t asm_instructions_w[] = {
    {"wrmsr", true, true,
     1, {ASM_INSTRUCTION_MNEMONIC_WRMSR, },
     2, {0x0f, 0x30, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },

    // end of table
    {NULL, false, false, 0, {0, }, 0, {0, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
};
