/**
 * @file asm_insts_u.64.c
 * @brief x86_64 assembly instruction mnemonics starting with 'u'
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/asm_instructions.h>

MODULE("turnstone.compiler.assembler");

const asm_instruction_t asm_instructions_u[] = {
    {"ud2", true, true,
     1, {ASM_INSTRUCTION_MNEMONIC_UD2, },
     2, {0x0f, 0x0b, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },

    // end of table
    {NULL, false, false, 0, {0, }, 0, {0, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
};
