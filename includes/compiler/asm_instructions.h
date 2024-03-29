/**
 * @file asm_instructions.h
 * @brief assembler instruction functions and types
 */

#ifndef ___ASM_INSTRUCTIONS_H
/*! prevent duplicate header error macro */
#define ___ASM_INSTRUCTIONS_H 0

#include <types.h>
#include <compiler/asm_encoder.h>

typedef enum asm_instruction_mnemonic_t {
    ASM_INSTRUCTION_MNEMONIC_NULL,
    ASM_INSTRUCTION_MNEMONIC_IMM_8,
    ASM_INSTRUCTION_MNEMONIC_IMM_16,
    ASM_INSTRUCTION_MNEMONIC_IMM_32,
    ASM_INSTRUCTION_MNEMONIC_IMM_64,
    ASM_INSTRUCTION_MNEMONIC_AL,
    ASM_INSTRUCTION_MNEMONIC_AX,
    ASM_INSTRUCTION_MNEMONIC_EAX,
    ASM_INSTRUCTION_MNEMONIC_RAX,
    ASM_INSTRUCTION_MNEMONIC_XMM1,
    ASM_INSTRUCTION_MNEMONIC_XMM2,
    ASM_INSTRUCTION_MNEMONIC_R_OR_M_8,
    ASM_INSTRUCTION_MNEMONIC_R_OR_M_16,
    ASM_INSTRUCTION_MNEMONIC_R_OR_M_32,
    ASM_INSTRUCTION_MNEMONIC_R_OR_M_64,
    ASM_INSTRUCTION_MNEMONIC_R_OR_M_128,
    ASM_INSTRUCTION_MNEMONIC_XMM2_OR_MEM_64,
    ASM_INSTRUCTION_MNEMONIC_XMM2_OR_MEM_128,
    ASM_INSTRUCTION_MNEMONIC_R_8,
    ASM_INSTRUCTION_MNEMONIC_R_16,
    ASM_INSTRUCTION_MNEMONIC_R_32,
    ASM_INSTRUCTION_MNEMONIC_R_64,
    ASM_INSTRUCTION_MNEMONIC_R_128,
    ASM_INSTRUCTION_MNEMONIC_M_8,
    ASM_INSTRUCTION_MNEMONIC_M_16,
    ASM_INSTRUCTION_MNEMONIC_M_32,
    ASM_INSTRUCTION_MNEMONIC_M_64,
    ASM_INSTRUCTION_MNEMONIC_M_128,
    ASM_INSTRUCTION_MNEMONIC_M_XX,


    ASM_INSTRUCTION_MNEMONIC_ADC,
    ASM_INSTRUCTION_MNEMONIC_ADD,
    ASM_INSTRUCTION_MNEMONIC_ADDSD,
    ASM_INSTRUCTION_MNEMONIC_AND,


    ASM_INSTRUCTION_MNEMONIC_CLI,


    ASM_INSTRUCTION_MNEMONIC_CMP,


    ASM_INSTRUCTION_MNEMONIC_CPUID,


    ASM_INSTRUCTION_MNEMONIC_HLT,


    ASM_INSTRUCTION_MNEMONIC_LEA,
    ASM_INSTRUCTION_MNEMONIC_LEAVE,
    ASM_INSTRUCTION_MNEMONIC_LGDT,
    ASM_INSTRUCTION_MNEMONIC_LIDT,
    ASM_INSTRUCTION_MNEMONIC_LTR,



    ASM_INSTRUCTION_MNEMONIC_OR,

    ASM_INSTRUCTION_MNEMONIC_SGDT,
    ASM_INSTRUCTION_MNEMONIC_SIDT,

    ASM_INSTRUCTION_MNEMONIC_STI,

    ASM_INSTRUCTION_MNEMONIC_STR,

    ASM_INSTRUCTION_MNEMONIC_SUB,


    ASM_INSTRUCTION_MNEMONIC_XOR,

} asm_instruction_mnemonic_t;

typedef struct asm_instruction_mnemonic_map_t {
    const char_t*              mnemonic_string;
    asm_instruction_mnemonic_t mnemonic;
    uint8_t                    min_opcode_count;
    uint8_t                    max_opcode_count;
    uint8_t                    default_operand_size;
    uint8_t                    max_operand_size;
    uint8_t                    default_mem_operand_size;
} asm_instruction_mnemonic_map_t;

typedef enum asm_opcode_t {
    ASM_OPCODE_NULL=256,
    ASM_OPCODE_REX,
    ASM_OPCODE_REX_W,
    ASM_OPCODE_IMM_8,
    ASM_OPCODE_IMM_16,
    ASM_OPCODE_IMM_32,
    ASM_OPCODE_IMM_64,
    ASM_OPCODE_AL,
    ASM_OPCODE_AX,
    ASM_OPCODE_EAX,
    ASM_OPCODE_RAX,
    ASM_OPCODE_R_OR_M_8,
    ASM_OPCODE_R_OR_M_16,
    ASM_OPCODE_R_OR_M_32,
    ASM_OPCODE_R_OR_M_64,
    ASM_OPCODE_R_8,
    ASM_OPCODE_R_16,
    ASM_OPCODE_R_32,
    ASM_OPCODE_R_64,
} asm_opcode_t;

typedef enum asm_operand_encode_t {
    ASM_INSTRUCTION_OPERAND_ENCODE_NULL,
    ASM_INSTRUCTION_OPERAND_ENCODE_IMM,
    ASM_INSTRUCTION_OPERAND_ENCODE_MEM,
    ASM_INSTRUCTION_OPERAND_ENCODE_MEM_IMM,
    ASM_INSTRUCTION_OPERAND_ENCODE_RM_REG,
    ASM_INSTRUCTION_OPERAND_ENCODE_REG_RM,
} asm_operand_encode_t;

typedef struct asm_instruction_t {
    const char_t*              general_mnemonic; // add, sub, mov, ...
    boolean_t                  valid_long;
    boolean_t                  valid_compat_or_legacy;
    uint8_t                    instruction_length;
    asm_instruction_mnemonic_t mnemonics[5];
    uint8_t                    opcode_length;
    uint8_t                    opcodes[4];
    boolean_t                  has_modrm;
    boolean_t                  merge_modrm;
    uint8_t                    modrm;
    boolean_t                  has_operand_size_override;
    boolean_t                  has_address_size_override;
    boolean_t                  has_rex;
    boolean_t                  has_rex_w;
    asm_operand_encode_t       operand_encode;
} asm_instruction_t;

asm_instruction_mnemonic_t            asm_instruction_mnemonic_get_by_param(asm_instruction_param_t* param, uint8_t operand_size, uint8_t mem_operand_size);
const asm_instruction_mnemonic_map_t* asm_instruction_mnemonic_get(const char_t* mnemonic_string);
const asm_instruction_t*              asm_instruction_get(const asm_instruction_mnemonic_map_t* map, uint8_t instruction_length, asm_instruction_mnemonic_t mnemonics[5]);
uint8_t                               asm_instruction_get_imm_size(const asm_instruction_t* instr);

#endif /* asm_instructions.h */
