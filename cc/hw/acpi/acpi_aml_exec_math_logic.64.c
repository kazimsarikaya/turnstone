/**
 * @file acpi_aml_exec_math_logic.64.c
 * @brief acpi aml math and logic executor methods
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <acpi/aml_internal.h>
#include <video.h>

MODULE("turnstone.kernel.hw.acpi");

int8_t acpi_aml_exec_op2_logic(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
    int64_t op1 = 0;
    int64_t op2 = 0;

    acpi_aml_object_t* op1op = opcode->operands[0];
    acpi_aml_object_t* op2op = opcode->operands[1];

    if(acpi_aml_read_as_integer(ctx, op1op, &op1) != 0) {
        return -1;
    }


    int64_t ires = 0;

    if(op2op != NULL) {
        if(acpi_aml_read_as_integer(ctx, op2op, &op2) != 0) {
            return -1;
        }
    }

    switch (opcode->opcode) {
    case ACPI_AML_LAND:
        PRINTLOG(ACPIAML, LOG_TRACE, "scope %s %lli && %lli", ctx->scope_prefix, op1, op2);
        ires = op1 && op2;
        break;
    case ACPI_AML_LOR:
        PRINTLOG(ACPIAML, LOG_TRACE, "scope %s %lli || %lli", ctx->scope_prefix, op1, op2);
        ires = op1 || op2;
        break;
    case ACPI_AML_LNOT:
        PRINTLOG(ACPIAML, LOG_TRACE, "scope %s ! %lli", ctx->scope_prefix, op1);
        ires = !op1;
        break;
    case ACPI_AML_LEQUAL << 8 | ACPI_AML_LNOT:
            PRINTLOG(ACPIAML, LOG_TRACE, "scope %s %lli != %lli", ctx->scope_prefix, op1, op2);
        ires = op1 != op2;
        break;
    case ACPI_AML_LGREATER << 8 | ACPI_AML_LNOT:
            PRINTLOG(ACPIAML, LOG_TRACE, "scope %s %lli <= %lli", ctx->scope_prefix, op1, op2);
        ires = !(op1 > op2);
        break;
    case ACPI_AML_LLESS << 8 | ACPI_AML_LNOT:
            PRINTLOG(ACPIAML, LOG_TRACE, "scope %s %lli >= %lli", ctx->scope_prefix, op1, op2);
        ires = !(op1 < op2);
        break;
    case ACPI_AML_LEQUAL:
        PRINTLOG(ACPIAML, LOG_TRACE, "scope %s %lli == %lli", ctx->scope_prefix, op1, op2);
        ires = op1 == op2;
        break;
    case ACPI_AML_LGREATER:
        PRINTLOG(ACPIAML, LOG_TRACE, "scope %s %lli > %lli", ctx->scope_prefix, op1, op2);
        ires = op1 > op2;
        break;
    case ACPI_AML_LLESS:
        PRINTLOG(ACPIAML, LOG_TRACE, "scope %s %lli < %lli", ctx->scope_prefix, op1, op2);
        ires = op1 < op2;
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown logic op code 0x%04x", opcode->opcode);
        return -1;
    }

    acpi_aml_object_t* res = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(res == NULL) {
        return -1;
    }

    res->type = ACPI_AML_OT_NUMBER;
    res->number.value = ires;
    res->number.bytecnt = 1;

    opcode->return_obj = res;

    return 0;
}


int8_t acpi_aml_exec_op1_tgt0_maths(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
    int64_t op1 = 0;
    acpi_aml_object_t* op1op = opcode->operands[0];

    if(acpi_aml_read_as_integer(ctx, op1op, &op1) != 0) {
        return -1;
    }

    switch (opcode->opcode) {
    case ACPI_AML_INCREMENT:
        op1++;
        break;
    case ACPI_AML_DECREMENT:
        op1--;
        break;
    default:
        return -1;
    }

    if(acpi_aml_write_as_integer(ctx, op1, op1op) != 0) {
        return -1;
    }

    opcode->return_obj = op1op;

    return 0;
}

int8_t acpi_aml_exec_op1_tgt1_maths(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
    if(opcode->opcode != ACPI_AML_NOT) {
        return -1;
    }

    acpi_aml_object_t* src = opcode->operands[0];
    acpi_aml_object_t* dst = opcode->operands[1];

    int64_t op1 = 0;
    if(acpi_aml_read_as_integer(ctx, src, &op1) != 0) {
        return -1;
    }

    int64_t ires = ~op1;

    if(acpi_aml_is_null_target(dst) != 0) {
        if(acpi_aml_write_as_integer(ctx, ires, dst) != 0) {
            return -1;
        }
    }

    acpi_aml_object_t* res = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(res == NULL) {
        return -1;
    }

    res->type = ACPI_AML_OT_NUMBER;
    res->number.value = ires;
    res->number.bytecnt = 1; // i don't known one or 8?

    opcode->return_obj = res;

    return 0;
}

int8_t acpi_aml_exec_op2_tgt1_maths(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
    acpi_aml_object_t* op1op = opcode->operands[0];
    acpi_aml_object_t* op2op = opcode->operands[1];
    acpi_aml_object_t* target = opcode->operands[2];

    if(op1op == NULL || op2op == NULL) {
        PRINTLOG(ACPIAML, LOG_ERROR, "one of empty operands for math op 0x%04x", opcode->opcode);
        return -1;
    }


    int64_t op1 = 0;
    int64_t op2 = 0;

    if(acpi_aml_read_as_integer(ctx, op1op, &op1) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "cannot read integer data op1 %i for math op 0x%04x", op1op->type, opcode->opcode);
        return -1;
    }

    if(acpi_aml_read_as_integer(ctx, op2op, &op2) != 0) {
        PRINTLOG(ACPIAML, LOG_ERROR, "cannot read integer data op2 %i for math op 0x%04x", op2op->type, opcode->opcode);
        return -1;
    }

    PRINTLOG(ACPIAML, LOG_TRACE, "ctx %s math op 0x%x with 0x%llx 0x%llx", ctx->scope_prefix, opcode->opcode, op1, op2);

    int64_t ires = 0;

    switch (opcode->opcode) {
    case ACPI_AML_ADD:
        ires = op1 + op2;
        break;
    case ACPI_AML_SUBTRACT:
        ires = op1 - op2;
        break;
    case ACPI_AML_MULTIPLY:
        ires = op1 * op2;
        break;
    case ACPI_AML_AND:
        ires = op1 & op2;
        break;
    case ACPI_AML_NAND:
        ires = !(op1 & op2);
        break;
    case ACPI_AML_OR:
        ires = op1 | op2;
        break;
    case ACPI_AML_NOR:
        ires = !(op1 | op2);
        break;
    case ACPI_AML_XOR:
        ires = op1 ^ op2;
        break;
    case ACPI_AML_MOD:
        ires = op1 % op2;
        break;
    case ACPI_AML_SHR:
        ires = op1 >> op2;
        break;
    case ACPI_AML_SHL:
        ires = op1 << op2;
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "Unknown math op code 0x%04x", opcode->opcode);
        return -1;
    }

    if(acpi_aml_is_null_target(target) != 0) {
        if(acpi_aml_write_as_integer(ctx, ires, target) != 0) {
            return -1;
        }
    }

    acpi_aml_object_t* res = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(res == NULL) {
        return -1;
    }

    res->type = ACPI_AML_OT_NUMBER;
    res->number.value = ires;
    res->number.bytecnt = 8; // i don't known one or 8?

    opcode->return_obj = res;

    return 0;
}

int8_t acpi_aml_exec_op2_tgt2_maths(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
    acpi_aml_object_t* dividendop = opcode->operands[0];
    acpi_aml_object_t* divisorop = opcode->operands[1];
    acpi_aml_object_t* remainderop = opcode->operands[2];
    acpi_aml_object_t* resultop = opcode->operands[3];

    int64_t dividend = 0;
    int64_t divisor = 0;

    if(acpi_aml_read_as_integer(ctx, dividendop, &dividend) != 0) {
        return -1;
    }

    if(acpi_aml_read_as_integer(ctx, divisorop, &divisor) != 0) {
        return -1;
    }

    if(divisor == 0) {
        ctx->flags.fatal = 1;
        return -1;
    }

    int64_t irem = dividend % divisor;
    int64_t ires = dividend / divisor;

    if(acpi_aml_is_null_target(remainderop) != 0) {
        if(acpi_aml_write_as_integer(ctx, irem, remainderop) != 0) {
            return -1;
        }
    }

    if(acpi_aml_is_null_target(resultop) != 0) {
        if(acpi_aml_write_as_integer(ctx, ires, resultop) != 0) {
            return -1;
        }
    }

    acpi_aml_object_t* res = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

    if(res == NULL) {
        return -1;
    }

    res->type = ACPI_AML_OT_NUMBER;
    res->number.value = ires;
    res->number.bytecnt = 1; // i don't known one or 8?

    opcode->return_obj = res;

    return 0;
}
