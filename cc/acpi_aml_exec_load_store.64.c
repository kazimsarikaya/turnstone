/**
 * @file acpi_aml_exec_load_store.64.c
 * @brief acpi aml load and store executor methods
 */

#include <acpi/aml_internal.h>
#include <video.h>

int8_t acpi_aml_exec_store(acpi_aml_parser_context_t* ctx, apci_aml_opcode_t* opcode) {
	acpi_aml_object_t* src = opcode->operands[0];
	acpi_aml_object_t* dst = opcode->operands[1];

	src->refcount++;
	dst->refcount++;

	int8_t res = -1;
	int64_t ival = 0;

	switch (dst->type) {
	case ACPI_AML_OT_NUMBER:
		ival = acpi_aml_read_as_integer(src);
		res = acpi_aml_write_as_integer(ctx, ival, dst);
		printf("ival %li writen to -%s-\n", ival, dst->name);
		break;
	case ACPI_AML_OT_OPCODE_EXEC_RETURN:
		if(dst->opcode_exec_return == NULL) { // debug
			acpi_aml_print_object(src);
			res = 0;
		}
		break;
	default:
		printf("unknown dest %i\n", dst->type);
		return -1;
	}

	if(res == 0) {
		opcode->return_obj = dst;
	}

	return res;
}


#define UNIMPLEXEC(name) \
	int8_t acpi_aml_exec_ ## name(acpi_aml_parser_context_t * ctx, apci_aml_opcode_t * opcode){ \
		UNUSED(ctx); \
		printf("ACPIAML: FATAL method %s for opcode 0x%04x not implemented\n", #name, opcode->opcode); \
		return -1; \
	}

UNIMPLEXEC(load_table);
UNIMPLEXEC(load);
