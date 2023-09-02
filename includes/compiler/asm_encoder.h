/**
 * @file asm_encoder.h
 * @brief assembler encoder functions and types
 */

#ifndef ___ASM_ENCODER_H
/*! prevent duplicate header error macro */
#define ___ASM_ENCODER_H 0

#include <types.h>
#include <buffer.h>
#include <linkedlist.h>
#include <linker.h>

#define ASM_INSTRUCTION_PREFIX_LOCK         0xF0
#define ASM_INSTRUCTION_PREFIX_REP          0xF3
#define ASM_INSTRUCTION_PREFIX_REPNE        0xF2
#define ASM_INSTRUCTION_PREFIX_OPERAND_SIZE 0x66
#define ASM_INSTRUCTION_PREFIX_ADDRESS_SIZE 0x67

#define ASM_INSTRUCTION_ESCAPE_BYTE         0x0F

typedef enum asm_instruction_param_type_t {
    ASM_INSTRUCTION_PARAM_TYPE_REGISTER,
    ASM_INSTRUCTION_PARAM_TYPE_MEMORY,
    ASM_INSTRUCTION_PARAM_TYPE_IMMEDIATE,
} asm_instruction_param_type_t;

typedef enum asm_register_type_t {
    ASM_REGISTER_TYPE_NORMAL,
    ASM_REGISTER_TYPE_BASE,
    ASM_REGISTER_TYPE_INDEX,
} asm_register_type_t;

typedef struct asm_intruction_param_t {
    asm_instruction_param_type_t type;
    struct {
        uint8_t   register_index;
        uint8_t   register_size;
        boolean_t force_rex;
    }        registers[3];
    uint64_t immediate;
    uint8_t  immediate_size;
    uint8_t  signed_immediate_size;
    uint64_t displacement;
    uint8_t  displacement_size;
    uint8_t  signed_displacement_size;
    uint64_t scale;
    char_t*  label;
} asm_instruction_param_t;

typedef struct asm_relocation_t {
    linker_relocation_type_t type;
    uint64_t                 offset;
    uint64_t                 addend;
    uint8_t                  size;
    char_t*                  label;
} asm_relocation_t;

boolean_t asm_encode_instructions(linkedlist_t tokens, buffer_t out, linkedlist_t relocs);
void      asm_encoder_print_relocs(linkedlist_t relocs);
void      asm_encoder_destroy_relocs(linkedlist_t relocs);

boolean_t asm_encode_adc(iterator_t* it, buffer_t out, linkedlist_t relocs);

#endif /* asm_encoder.h */
