/**
 * @file asm_inst_mnemonics.64.c
 * @brief x86_64 assembly instruction mnemonics
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/asm_instructions.h>

MODULE("turnstone.compiler.assembler");

const asm_instruction_mnemonic_map_t asm_instruction_mnemonic_map[] = {
    {"adc", ASM_INSTRUCTION_MNEMONIC_ADC, 3, 3, 32, 32, 0},
    {"add", ASM_INSTRUCTION_MNEMONIC_ADD, 3, 3, 32, 32, 0},
    {"addsd", ASM_INSTRUCTION_MNEMONIC_ADDSD, 3, 3, 64, 64, 64},
    {"and", ASM_INSTRUCTION_MNEMONIC_AND, 3, 3, 32, 32, 0},


    {"cli", ASM_INSTRUCTION_MNEMONIC_CLI, 1, 1, 0, 0, 0},



    {"cmp", ASM_INSTRUCTION_MNEMONIC_CMP, 3, 3, 32, 32, 0},



    {"cli", ASM_INSTRUCTION_MNEMONIC_CPUID, 1, 1, 0, 0, 0},



    {"hlt", ASM_INSTRUCTION_MNEMONIC_HLT, 1, 1, 0, 0, 0},



    {"lea", ASM_INSTRUCTION_MNEMONIC_LEA, 3, 3, 32, 64, 0},
    {"leave", ASM_INSTRUCTION_MNEMONIC_LEAVE, 1, 1, 0, 0, 0},

    {"lgdt", ASM_INSTRUCTION_MNEMONIC_LGDT, 2, 2, 0, 0, 0},
    {"lidt", ASM_INSTRUCTION_MNEMONIC_LIDT, 2, 2, 0, 0, 0},

    {"lret", ASM_INSTRUCTION_MNEMONIC_LRET, 1, 2, 0, 0, 0},

    {"ltr", ASM_INSTRUCTION_MNEMONIC_LTR, 2, 2, 16, 16, 16},



    {"or", ASM_INSTRUCTION_MNEMONIC_OR, 3, 3, 32, 32, 0},

    {"pop", ASM_INSTRUCTION_MNEMONIC_POP, 2, 2, 16, 64, 0},
    {"popf", ASM_INSTRUCTION_MNEMONIC_POPF, 1, 1, 0, 0, 0},
    {"popfd", ASM_INSTRUCTION_MNEMONIC_POPFD, 1, 1, 0, 0, 0},
    {"popfq", ASM_INSTRUCTION_MNEMONIC_POPFQ, 1, 1, 0, 0, 0},
    {"push", ASM_INSTRUCTION_MNEMONIC_PUSH, 2, 2, 16, 64, 0},
    {"pushf", ASM_INSTRUCTION_MNEMONIC_PUSHF, 1, 1, 0, 0, 0},
    {"pushfd", ASM_INSTRUCTION_MNEMONIC_PUSHFD, 1, 1, 0, 0, 0},
    {"pushfq", ASM_INSTRUCTION_MNEMONIC_PUSHFQ, 1, 1, 0, 0, 0},

    {"ret", ASM_INSTRUCTION_MNEMONIC_RET, 1, 2, 0, 0, 0},


    {"sgdt", ASM_INSTRUCTION_MNEMONIC_SGDT, 2, 2, 0, 0, 0},
    {"sidt", ASM_INSTRUCTION_MNEMONIC_SIDT, 2, 2, 0, 0, 0},

    {"sti", ASM_INSTRUCTION_MNEMONIC_STI, 1, 1, 0, 0, 0},

    {"str", ASM_INSTRUCTION_MNEMONIC_STR, 2, 2, 16, 16, 16},


    {"sub", ASM_INSTRUCTION_MNEMONIC_SUB, 3, 3, 32, 32, 0},

    {"ud2", ASM_INSTRUCTION_MNEMONIC_UD2, 1, 1, 0, 0, 0},


    {"xor", ASM_INSTRUCTION_MNEMONIC_XOR, 3, 3, 32, 32, 0},


    {NULL, ASM_INSTRUCTION_MNEMONIC_NULL, 0, 0, 0, 0, 0},
};


