/**
 * @file asm_instructions.64.c
 * @brief Test for asm instructions
 */

#include <compiler/asm_instructions.h>
#include <strings.h>
#include <logging.h>

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
    {"ltr", ASM_INSTRUCTION_MNEMONIC_LTR, 2, 2, 16, 16, 16},



    {"or", ASM_INSTRUCTION_MNEMONIC_OR, 3, 3, 32, 32, 0},

    {"pop", ASM_INSTRUCTION_MNEMONIC_POP, 2, 2, 16, 64, 0},
    {"popf", ASM_INSTRUCTION_MNEMONIC_POPF, 1, 1, 0, 0, 0},
    {"push", ASM_INSTRUCTION_MNEMONIC_PUSH, 2, 2, 16, 64, 0},
    {"pushf", ASM_INSTRUCTION_MNEMONIC_PUSHF, 1, 1, 0, 0, 0},


    {"sgdt", ASM_INSTRUCTION_MNEMONIC_SGDT, 2, 2, 0, 0, 0},
    {"sidt", ASM_INSTRUCTION_MNEMONIC_SIDT, 2, 2, 0, 0, 0},

    {"sti", ASM_INSTRUCTION_MNEMONIC_STI, 1, 1, 0, 0, 0},

    {"str", ASM_INSTRUCTION_MNEMONIC_STR, 2, 2, 16, 16, 16},


    {"sub", ASM_INSTRUCTION_MNEMONIC_SUB, 3, 3, 32, 32, 0},



    {"xor", ASM_INSTRUCTION_MNEMONIC_XOR, 3, 3, 32, 32, 0},


    {NULL, ASM_INSTRUCTION_MNEMONIC_NULL, 0, 0, 0, 0, 0},
};

const asm_instruction_t asm_instructions[] = {
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_AL, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x14, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_AX, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x15, }, false, false, 0, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_EAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x15, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_RAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x15, }, false, false, 0, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 2, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 2, false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x81, }, true, false, 2, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 2, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 2, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 2, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 2, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 2, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x10, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x10, }, true, false, 'r', false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_R_16},
     1, {0x11, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_R_32},
     1, {0x11, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_R_64},
     1, {0x11, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x12, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x12, }, true, false, 'r', false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_16, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16},
     1, {0x13, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"adc", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_32, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32},
     1, {0x13, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"adc", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADC, ASM_INSTRUCTION_MNEMONIC_R_64, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64},
     1, {0x13, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"add", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_AL, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x04, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"add", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_AX, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x05, }, false, false, 0, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"add", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_EAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x05, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"add", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_RAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x05, }, false, false, 0, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"add", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"add", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 0, false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"add", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x81, }, true, false, 0, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"add", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"add", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 0, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"add", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 0, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"add", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"add", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 0, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"add", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x00, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"add", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x00, }, true, false, 'r', false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"add", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_R_16},
     1, {0x01, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"add", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_R_32},
     1, {0x01, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"add", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_R_64},
     1, {0x01, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"add", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x02, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"add", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x02, }, true, false, 'r', false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"add", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_16, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16},
     1, {0x03, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"add", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_32, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32},
     1, {0x03, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"add", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_ADD, ASM_INSTRUCTION_MNEMONIC_R_64, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64},
     1, {0x03, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"addsd", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADDSD, ASM_INSTRUCTION_MNEMONIC_R_128, ASM_INSTRUCTION_MNEMONIC_R_128},
     3, {0xF2, 0x0F, 0x58}, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"addsd", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_ADDSD, ASM_INSTRUCTION_MNEMONIC_R_128, ASM_INSTRUCTION_MNEMONIC_M_64},
     3, {0xF2, 0x0F, 0x58}, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"and", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_AL, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x24, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"and", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_AX, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x25, }, false, false, 0, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"and", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_EAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x25, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"and", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_RAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x25, }, false, false, 0, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"and", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 4, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"and", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 4, false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"and", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x81, }, true, false, 4, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"and", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 4, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"and", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 4, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"and", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 4, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"and", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 4, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"and", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 4, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"and", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x20, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"and", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x20, }, true, false, 'r', false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"and", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_R_16},
     1, {0x21, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"and", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_R_32},
     1, {0x21, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"and", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_R_64},
     1, {0x21, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"and", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x22, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"and", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x22, }, true, false, 'r', false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"and", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_16, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16},
     1, {0x23, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"and", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_32, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32},
     1, {0x23, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"and", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_AND, ASM_INSTRUCTION_MNEMONIC_R_64, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64},
     1, {0x23, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },




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



    {"hlt", true, true,
     1, {ASM_INSTRUCTION_MNEMONIC_HLT, },
     1, {0xf4, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },


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

    {"ltr", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_LTR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16},
     2, {0x0f, 0x00, }, true, false, 3, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM, },



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


    {"pop", true, false,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_GS},
     2, {0x0f, 0xa9, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"pop", false, true,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_GS},
     2, {0x0f, 0xa9, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"pop", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_GS},
     2, {0x0f, 0xa9, }, false, false, 0, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"pop", true, false,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_FS},
     2, {0x0f, 0xa1, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"pop", false, true,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_FS},
     2, {0x0f, 0xa1, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"pop", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_FS},
     2, {0x0f, 0xa1, }, false, false, 0, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"pop", false, true,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_SS},
     1, {0x17, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"pop", false, true,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_ES},
     1, {0x07, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"pop", false, true,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_DS},
     1, {0x1f, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"pop", true, false,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_R_64},
     1, {0x58, }, false, true, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"pop", false, true,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_R_32},
     1, {0x58, }, false, true, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"pop", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_R_16},
     1, {0x58, }, false, true, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"pop", true, false,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64},
     1, {0x8f, }, true, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"pop", false, true,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32},
     1, {0x8f, }, true, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"pop", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_POP, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16},
     1, {0x8f, }, true, false, 0, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },





    {"push", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_GS},
     2, {0x0f, 0xa8, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"push", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_FS},
     2, {0x0f, 0xa0, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"push", false, true,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_ES},
     1, {0x06, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"push", false, true,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_DS},
     1, {0x1e, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"push", false, true,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_SS},
     1, {0x16, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"push", false, true,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_CS},
     1, {0x0e, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"push", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x6A, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"push", true, false,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x68, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"push", false, true,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x68, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"push", true, false,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_R_64},
     1, {0x50, }, false, true, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"push", false, true,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_R_32},
     1, {0x50, }, false, true, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"push", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_R_16},
     1, {0x50, }, false, true, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
    {"push", true, false,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64},
     1, {0xff, }, true, false, 6, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"push", false, true,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32},
     1, {0xff, }, true, false, 6, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"push", true, true,
     2, {ASM_INSTRUCTION_MNEMONIC_PUSH, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16},
     1, {0xff, }, true, false, 6, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },











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



    {"xor", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_AL, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x34, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"xor", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_AX, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x35, }, false, false, 0, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"xor", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_EAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x35, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"xor", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_RAX, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x35, }, false, false, 0, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_IMM, },
    {"xor", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 6, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"xor", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x80, }, true, false, 6, false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"xor", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_16},
     1, {0x81, }, true, false, 6, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"xor", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 6, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"xor", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_32},
     1, {0x81, }, true, false, 6, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"xor", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 6, true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"xor", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 6, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"xor", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_IMM_8},
     1, {0x83, }, true, false, 6, false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM, },
    {"xor", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x30, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"xor", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8, ASM_INSTRUCTION_MNEMONIC_R_8},
     1, {0x30, }, true, false, 'r', false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"xor", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16, ASM_INSTRUCTION_MNEMONIC_R_16},
     1, {0x31, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"xor", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32, ASM_INSTRUCTION_MNEMONIC_R_32},
     1, {0x31, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"xor", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64, ASM_INSTRUCTION_MNEMONIC_R_64},
     1, {0x31, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG, },
    {"xor", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x32, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"xor", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_8, ASM_INSTRUCTION_MNEMONIC_R_OR_M_8},
     1, {0x32, }, true, false, 'r', false, false, true, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"xor", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_16, ASM_INSTRUCTION_MNEMONIC_R_OR_M_16},
     1, {0x33, }, true, false, 'r', true, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"xor", true, true,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_32, ASM_INSTRUCTION_MNEMONIC_R_OR_M_32},
     1, {0x33, }, true, false, 'r', false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },
    {"xor", true, false,
     3, {ASM_INSTRUCTION_MNEMONIC_XOR, ASM_INSTRUCTION_MNEMONIC_R_64, ASM_INSTRUCTION_MNEMONIC_R_OR_M_64},
     1, {0x33, }, true, false, 'r', false, false, true, true, ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM, },





    // end of table
    {NULL, false, false, 0, {0, }, 0, {0, }, false, false, 0, false, false, false, false, ASM_INSTRUCTION_OPERAND_ENCODE_NULL, },
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
