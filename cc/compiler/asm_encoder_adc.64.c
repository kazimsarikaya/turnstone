/**
 * @file asm_encoder_adc.64.c
 * @brief amd64 adc encoder.
 */

#include <compiler/asm_encoder.h>
#include <compiler/asm_parser.h>
#include <strings.h>
#include <video.h>

MODULE("turnstone.compiler.assembler");


boolean_t asm_parse_instruction_param(const asm_token_t* tok, asm_instruction_param_t* param);
boolean_t asm_encode_modrm_sib(asm_instruction_param_t op, boolean_t* need_sib, uint8_t* modrm, uint8_t* sib,
                               boolean_t* need_rex, uint8_t* rex,
                               boolean_t* has_displacement, uint8_t* disp_size);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
boolean_t asm_encode_adc(iterator_t* it, buffer_t out, linkedlist_t relocs) {
    const asm_token_t* adc_code = it->get_item(it);
    it = it->next(it);
    const asm_token_t* t_op2 = it->get_item(it);
    it = it->next(it);
    const asm_token_t* t_op1 = it->get_item(it);

    uint8_t default_reg_size = 32;

    if(adc_code->token_type != ASM_TOKEN_TYPE_INSTRUCTION) {
        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid instruction");

        return false;
    }

    if(t_op1->token_type != ASM_TOKEN_TYPE_PARAMETER || t_op2->token_type != ASM_TOKEN_TYPE_PARAMETER) {
        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid instruction parameters");

        return false;
    }

    if(strlen(adc_code->token_value) == 4) {
        if(adc_code->token_value[3] == 'b') {
            default_reg_size = 8;
        } else if(adc_code->token_value[3] == 'w') {
            default_reg_size = 16;
        } else if(adc_code->token_value[3] == 'l') {
            default_reg_size = 32;
        } else if(adc_code->token_value[3] == 'q') {
            default_reg_size = 64;
        } else {
            PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid instruction");

            return false;
        }
    }

    asm_instruction_param_t op1 = {0};
    asm_instruction_param_t op2 = {0};

    if(!asm_parse_instruction_param(t_op1, &op1)) {
        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to parse instruction parameter op1 %s", t_op1->token_value);

        return false;
    }

    if(!asm_parse_instruction_param(t_op2, &op2)) {
        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to parse instruction parameter op2 %s", t_op2->token_value);

        return false;
    }

    if(op1.type == ASM_INSTRUCTION_PARAM_TYPE_MEMORY && op2.type == ASM_INSTRUCTION_PARAM_TYPE_MEMORY) {
        PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid instruction parameters");

        return false;
    }

    if(op1.type == ASM_INSTRUCTION_PARAM_TYPE_REGISTER && op1.registers[0].register_index == 0 && op2.type == ASM_INSTRUCTION_PARAM_TYPE_IMMEDIATE) {
        if(op1.registers[0].register_size == 8 && (op2.signed_immediate_size == 8 || op2.label)) {
            buffer_append_byte(out, 0x14);

            if(op2.label) {
                asm_relocation_t* reloc = memory_malloc(sizeof(asm_relocation_t));

                if(reloc == NULL) {
                    PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to allocate memory for relocation");

                    return false;
                }

                reloc->type = LINKER_RELOCATION_TYPE_64_8;
                reloc->offset = buffer_get_position(out);
                reloc->label = op2.label;

                linkedlist_queue_push(relocs, reloc);
            }

            buffer_append_byte(out, op2.immediate);
        } else if(op1.registers[0].register_size == 16 && op2.signed_immediate_size == 8) {
            buffer_append_byte(out, 0x66);
            buffer_append_byte(out, 0x83);
            buffer_append_byte(out, 0xd0);

            if(op2.label) {
                asm_relocation_t* reloc = memory_malloc(sizeof(asm_relocation_t));

                if(reloc == NULL) {
                    PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to allocate memory for relocation");

                    return false;
                }

                reloc->type = LINKER_RELOCATION_TYPE_64_8;
                reloc->offset = buffer_get_position(out);
                reloc->label = op2.label;

                linkedlist_queue_push(relocs, reloc);
            }

            buffer_append_byte(out, op2.immediate);
        } else if(op1.registers[0].register_size == 16 && (op2.signed_immediate_size == 16 || op2.label)) {
            buffer_append_byte(out, 0x66);
            buffer_append_byte(out, 0x15);

            if(op2.label) {
                asm_relocation_t* reloc = memory_malloc(sizeof(asm_relocation_t));

                if(reloc == NULL) {
                    PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to allocate memory for relocation");

                    return false;
                }

                reloc->type = LINKER_RELOCATION_TYPE_64_16;
                reloc->offset = buffer_get_position(out);
                reloc->label = op2.label;

                linkedlist_queue_push(relocs, reloc);
            }

            buffer_append_bytes(out, (uint8_t*)&op2.immediate, 2);
        } else if(op1.registers[0].register_size == 32 && (op2.signed_immediate_size == 32 || op2.label)) {
            buffer_append_byte(out, 0x15);

            if(op2.label) {
                asm_relocation_t* reloc = memory_malloc(sizeof(asm_relocation_t));

                if(reloc == NULL) {
                    PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to allocate memory for relocation");

                    return false;
                }

                reloc->type = LINKER_RELOCATION_TYPE_64_32;
                reloc->offset = buffer_get_position(out);
                reloc->label = op2.label;

                linkedlist_queue_push(relocs, reloc);
            }

            buffer_append_bytes(out, (uint8_t*)&op2.immediate, 4);
        } else if(op1.registers[0].register_size == 64 && (op2.signed_immediate_size == 32 || op2.label)) {
            buffer_append_byte(out, 0x48);
            buffer_append_byte(out, 0x15);

            if(op2.label) {
                asm_relocation_t* reloc = memory_malloc(sizeof(asm_relocation_t));

                if(reloc == NULL) {
                    PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to allocate memory for relocation");

                    return false;
                }

                reloc->type = LINKER_RELOCATION_TYPE_64_32S;
                reloc->offset = buffer_get_position(out);
                reloc->label = op2.label;

                linkedlist_queue_push(relocs, reloc);
            }

            buffer_append_bytes(out, (uint8_t*)&op2.immediate, 4);
        } else {
            PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid immediate size %llx %d", op2.immediate, op2.signed_immediate_size);
            PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid register size %d", op1.registers[0].register_size);

            return false;
        }

    } else if(op2.type == ASM_INSTRUCTION_PARAM_TYPE_IMMEDIATE) {
        boolean_t need_rex = false;
        uint8_t rex = 0;
        uint8_t modrm = (op1.type == ASM_INSTRUCTION_PARAM_TYPE_REGISTER ? 0xc0 : 0x04) | (2 << 3);
        uint8_t opcode = 0x80;
        uint8_t imm_size = 0;
        boolean_t need_sib = false;
        uint8_t sib = 0;

        if(op2.signed_immediate_size == 64) {
            PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid immediate size %llx %d", op2.immediate, op2.signed_immediate_size);

            return false;
        }

        boolean_t has_displacement = false;
        uint8_t disp_size = 0;

        if(!asm_encode_modrm_sib(op1, &need_sib, &modrm, &sib, &need_rex, &rex, &has_displacement, &disp_size)) {
            return false;
        }

        if(op1.registers[0].register_size == 64) {
            need_rex = true;
            rex |= 0x40;
        }

        if(op1.registers[0].register_index > 7) {
            need_rex = true;
            rex |= 0x01;
        }

        if(op1.registers[0].register_size == 0) {
            opcode = 0x81;

            if(default_reg_size == 8) {
                opcode = 0x80;
                imm_size = 1;
            } else if(default_reg_size == 16) {
                buffer_append_byte(out, 0x66);
                imm_size = 2;
            } else if(default_reg_size == 32) {
                imm_size = 4;
            } else if(default_reg_size == 64) {
                need_rex = true;
                rex |= 0x08;
                imm_size = 4;
            }
        }

        if(op1.registers[0].register_size == 8) {

            if(op2.signed_immediate_size == 8) {
                imm_size = 1;
            } else {
                PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid immediate size %llx %d", op2.immediate, op2.signed_immediate_size);
                PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid register size %d", op1.registers[0].register_size);

                return false;
            }
        }

        if(op1.registers[0].register_size == 16) {
            buffer_append_byte(out, 0x66);

            if(op2.signed_immediate_size == 8) {
                opcode = 0x83;
                imm_size = 1;
            } else if(op2.signed_immediate_size == 16) {
                opcode = 0x81;
                imm_size = 2;
            } else {
                PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid immediate size %llx %d", op2.immediate, op2.signed_immediate_size);
                PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid register size %d", op1.registers[0].register_size);

                return false;
            }
        }

        if(op1.registers[0].register_size == 32) {
            if(op2.signed_immediate_size == 8) {
                opcode = 0x83;
                imm_size = 1;
            } else if(op2.signed_immediate_size <= 32) {
                opcode = 0x81;
                imm_size = 4;
            } else {
                PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid immediate size %llx %d", op2.immediate, op2.signed_immediate_size);
                PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid register size %d", op1.registers[0].register_size);

                return false;
            }
        }

        if(op1.registers[0].register_size == 64) {
            rex |= 0x08;

            if(op2.signed_immediate_size == 8) {
                opcode = 0x83;
                imm_size = 1;
            } else if(op2.signed_immediate_size <= 32) {
                opcode = 0x81;
                imm_size = 4;
            } else {
                PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid immediate size %llx %d", op2.immediate, op2.signed_immediate_size);
                PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Invalid register size %d", op1.registers[0].register_size);

                return false;
            }
        }

        if(need_rex) {
            rex |= 0x40;
            buffer_append_byte(out, rex);
        }

        modrm |= op1.registers[0].register_index & 0x07;

        buffer_append_byte(out, opcode);
        buffer_append_byte(out, modrm);

        if(need_sib) {
            buffer_append_byte(out, sib);
        }

        if(has_displacement) {
            if(op1.label) {
                asm_relocation_t* reloc = memory_malloc(sizeof(asm_relocation_t));

                if(reloc == NULL) {
                    PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to allocate memory for relocation");

                    return false;
                }

                reloc->addend = 0;
                reloc->offset = buffer_get_position(out);
                reloc->type = disp_size == 1?LINKER_RELOCATION_TYPE_64_8:
                              disp_size == 4?LINKER_RELOCATION_TYPE_64_32:LINKER_RELOCATION_TYPE_64_64;
                reloc->label = op1.label;

                linkedlist_queue_push(relocs, reloc);
            }

            buffer_append_bytes(out, (uint8_t*)&op1.displacement, disp_size);
        }

        if(op2.label) {
            asm_relocation_t* reloc = memory_malloc(sizeof(asm_relocation_t));

            if(reloc == NULL) {
                PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to allocate memory for relocation");

                return false;
            }

            reloc->addend = 0;
            reloc->offset = buffer_get_position(out);
            reloc->type = imm_size == 2?LINKER_RELOCATION_TYPE_64_32:
                          imm_size == 4?LINKER_RELOCATION_TYPE_64_32:LINKER_RELOCATION_TYPE_64_64;
            reloc->label = op2.label;

            linkedlist_queue_push(relocs, reloc);
        }

        buffer_append_bytes(out, (uint8_t*)&op2.immediate, imm_size);

    } else {
        boolean_t need_rex = false;
        uint8_t rex = 0;
        uint8_t modrm = 0;
        uint8_t opcode = 0x10;
        boolean_t need_sib = false;
        uint8_t sib = 0;

        boolean_t has_displacement = false;
        uint8_t disp_size = 0;

        if(op1.type == ASM_INSTRUCTION_PARAM_TYPE_MEMORY) {
            if(!asm_encode_modrm_sib(op1, &need_sib, &modrm, &sib, &need_rex, &rex, &has_displacement, &disp_size)) {
                return false;
            }

            if(op2.registers[0].register_index > 7) {
                need_rex = true;
                rex |= 0x04;
            }

            modrm |= (op2.registers[0].register_index & 0x07) << 3 | 0x04;

            if(op2.registers[0].register_size == 16) {
                buffer_append_byte(out, 0x66);
                opcode = 0x11;
            }

            if(op2.registers[0].register_size == 32) {
                opcode = 0x11;
            }

            if(op2.registers[0].register_size == 64) {
                need_rex = true;
                rex |= 0x08;
                opcode = 0x11;
            }

        } else if(op2.type == ASM_INSTRUCTION_PARAM_TYPE_MEMORY) {
            if(!asm_encode_modrm_sib(op2, &need_sib, &modrm, &sib, &need_rex, &rex, &has_displacement, &disp_size)) {
                return false;
            }

            if(op1.registers[0].register_index > 7) {
                need_rex = true;
                rex |= 0x04;
            }

            modrm |= (op1.registers[0].register_index & 0x07) << 3 | 0x04;

            if(op1.registers[0].register_size == 16) {
                buffer_append_byte(out, 0x66);
                opcode = 0x13;
            }

            if(op1.registers[0].register_size == 32) {
                opcode = 0x13;
            }

            if(op1.registers[0].register_size == 64) {
                need_rex = true;
                rex |= 0x08;
                opcode = 0x13;
            }

        } else {
            modrm |= 0xc0 | (op1.registers[0].register_index & 0x07) << 3 | (op2.registers[0].register_index & 0x07);

            if(op1.registers[0].register_index > 7) {
                need_rex = true;
                rex |= 0x04;
            }

            if(op2.registers[0].register_index > 7) {
                need_rex = true;
                rex |= 0x01;
            }

            if(op2.registers[0].register_size == 8) {
                need_rex = true;
                opcode = 0x12;
            }

            if(op2.registers[0].register_size == 16) {
                buffer_append_byte(out, 0x66);
                opcode = 0x13;
            }

            if(op1.registers[0].register_size == 64) {
                need_rex = true;
                rex |= 0x08;
                opcode = 0x13;
            }
        }

        if(need_rex) {
            rex |= 0x40;
            buffer_append_byte(out, rex);
        }

        buffer_append_byte(out, opcode);
        buffer_append_byte(out, modrm);

        if(need_sib) {
            buffer_append_byte(out, sib);
        }

        if(has_displacement) {
            asm_instruction_param_t op = {0};

            if(op1.type == ASM_INSTRUCTION_PARAM_TYPE_MEMORY) {
                op = op1;
            } else if(op2.type == ASM_INSTRUCTION_PARAM_TYPE_MEMORY) {
                op = op2;
            }

            if(disp_size == 0) {
                disp_size = op.signed_displacement_size / 8;

                if(disp_size == 0) {
                    disp_size = default_reg_size / 8;
                }
            }

            if(op.label) {
                asm_relocation_t* reloc = memory_malloc(sizeof(asm_relocation_t));

                if(reloc == NULL) {
                    PRINTLOG(COMPILER_ASSEMBLER, LOG_ERROR, "Failed to allocate memory for relocation");

                    return false;
                }

                reloc->addend = 0;
                reloc->offset = buffer_get_position(out);
                reloc->type = disp_size == 1?LINKER_RELOCATION_TYPE_64_8:
                              disp_size == 4?LINKER_RELOCATION_TYPE_64_32:LINKER_RELOCATION_TYPE_64_64;
                reloc->label = op.label;

                linkedlist_queue_push(relocs, reloc);
            }

            buffer_append_bytes(out, (uint8_t*)&op.displacement, disp_size);
        }
    }

    return true;
}
#pragma GCC diagnostic pop

