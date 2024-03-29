/**
 * @file acpi_aml_exec_load_store.64.c
 * @brief acpi aml load and store executor methods
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <acpi/aml_internal.h>
#include <logging.h>

MODULE("turnstone.kernel.hw.acpi");

int8_t acpi_aml_exec_store(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode) {
    acpi_aml_object_t* src = opcode->operands[0];
    acpi_aml_object_t* dst = opcode->operands[1];

    if(dst == NULL || src == NULL) {
        ctx->flags.fatal = 1;
        PRINTLOG(ACPIAML, LOG_FATAL, "store op with null dst/src %i", dst == NULL?0:1);
        return -1;
    }

    src = acpi_aml_get_if_arg_local_obj(ctx, src, 0, 0);
    acpi_aml_object_t* original_dst = dst;
    dst = acpi_aml_get_if_arg_local_obj(ctx, dst, 0, 0);

    if(src->type == ACPI_AML_OT_REFOF && !(dst->type == ACPI_AML_OT_UNINITIALIZED || dst->type == ACPI_AML_OT_DEBUG)) {
        PRINTLOG(ACPIAML, LOG_FATAL, "writing refof to the non uninitiliazed variable");
        ctx->flags.fatal = 1;
        return -1;
    }

    acpi_aml_object_type_t dst_type = dst->type;

    if(dst_type == ACPI_AML_OT_UNINITIALIZED) {
        dst_type = src->type;
    }

    if(dst_type == ACPI_AML_OT_REFOF) {
        dst = dst->refof_target;

        if(dst == NULL) {
            PRINTLOG(ACPIAML, LOG_FATAL, "writing refof target is non uninitiliazed variable");
            ctx->flags.fatal = 1;
            return -1;
        }

        dst_type = dst->type;
    }

    if(dst_type == ACPI_AML_OT_FIELD) {

        if(dst->type == ACPI_AML_OT_FIELD) {
            dst_type = dst->field.related_object->type;
        } else {
            dst_type = src->field.related_object->type;
        }

        if(dst_type == ACPI_AML_OT_OPREGION) {
            dst_type = ACPI_AML_OT_NUMBER;
        }

        if(dst_type == ACPI_AML_OT_FIELD) {
            dst_type = ACPI_AML_OT_NUMBER;
        }
    }

    if(dst_type == ACPI_AML_OT_BUFFERFIELD) {
        dst_type = ACPI_AML_OT_NUMBER;
    }

    int8_t res = -1;
    int64_t ival = 0;

    switch (dst_type) {
    case ACPI_AML_OT_NUMBER:
        res = acpi_aml_read_as_integer(ctx, src, &ival);
        if(res != 0) {
            PRINTLOG(ACPIAML, LOG_ERROR, "read as integer of src failed %i", src->type);
            break;
        }
        res = acpi_aml_write_as_integer(ctx, ival, original_dst);
        break;
    case ACPI_AML_OT_STRING:
        res = acpi_aml_write_as_string(ctx, src, original_dst);
        break;
    case ACPI_AML_OT_BUFFER:
    case ACPI_AML_OT_DEBUG:
        res = acpi_aml_write_as_buffer(ctx, src, original_dst);
        break;
    default:
        PRINTLOG(ACPIAML, LOG_ERROR, "store unknown dest %i src is %i remaining %lli", dst->type, src->type, ctx->remaining);
        acpi_aml_print_object(ctx, dst);
        return -1;
    }

    if(res == 0) {
        opcode->return_obj = dst;
    }

    return res;
}


#define UNIMPLEXEC(name) \
        int8_t acpi_aml_exec_ ## name(acpi_aml_parser_context_t * ctx, acpi_aml_opcode_t * opcode){ \
            UNUSED(ctx); \
            PRINTLOG(ACPIAML, LOG_ERROR, "method %s for opcode 0x%04x not implemented", #name, opcode->opcode); \
            return -1; \
        }

UNIMPLEXEC(load_table);
UNIMPLEXEC(load);
