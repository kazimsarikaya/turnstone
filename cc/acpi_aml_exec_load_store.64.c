/**
 * @file acpi_aml_exec_load_store.64.c
 * @brief acpi aml load and store executor methods
 */

#include <acpi/aml_internal.h>
#include <video.h>

int8_t acpi_aml_exec_store(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode) {
	acpi_aml_object_t* src = opcode->operands[0];
	acpi_aml_object_t* dst = opcode->operands[1];

	src->refcount++;
	dst->refcount++;


	src = acpi_aml_get_if_arg_local_obj(ctx, src, 0, 0);
	dst = acpi_aml_get_if_arg_local_obj(ctx, dst, 1, 0);

	if(dst == NULL || src == NULL) {
		ctx->flags.fatal = 1;
		return -1;
	}

	if(src->type == ACPI_AML_OT_REFOF && !(dst->type == ACPI_AML_OT_UNINITIALIZED || dst->type == ACPI_AML_OT_DEBUG)) {
		printf("ACPIAML: Error at writing refof to the non uninitiliazed variable\n");
		ctx->flags.fatal = 1;
		return -1;
	}

	if(dst->type == ACPI_AML_OT_UNINITIALIZED) {
		dst->type = src->type;
	}

	if(dst->type == ACPI_AML_OT_REFOF) {
		dst = dst->refof_target;

		if(dst == NULL) {
			ctx->flags.fatal = 1;
			return -1;
		}
	}

	int8_t res = -1;
	int64_t ival = 0;

	switch (dst->type) {
	case ACPI_AML_OT_NUMBER:
		res = acpi_aml_read_as_integer(ctx, src, &ival);
		if(res != 0) {
			break;
		}
		res = acpi_aml_write_as_integer(ctx, ival, dst);
		break;
	case ACPI_AML_OT_DEBUG:
		acpi_aml_print_object(ctx, src);
		res = 0;
		break;
	default:
		acpi_aml_print_object(ctx, dst);
		printf("store unknown dest %i src is %i\n", dst->type, src->type);
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
		printf("ACPIAML: FATAL method %s for opcode 0x%04x not implemented\n", #name, opcode->opcode); \
		return -1; \
	}

UNIMPLEXEC(load_table);
UNIMPLEXEC(load);
