/**
 * @file acpi_aml_exec_conversions.64.c
 * @brief acpi aml exec object conversions methods
 */

#include <acpi/aml_internal.h>
#include <video.h>

int8_t acpi_aml_exec_object_type(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
	acpi_aml_object_t* op1 = opcode->operands[0];

	if(op1 == NULL) {
		return -1;
	}

	uint8_t type = op1->type;

	if(type > 16) {
		type = 0;
	}

	acpi_aml_object_t* res = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

	if(res == NULL) {
		return -1;
	}

	res->type = ACPI_AML_OT_NUMBER;
	res->number.value = type;
	res->number.bytecnt = 1; // i don't known one or 8?

	opcode->return_obj = res;

	return 0;
}

int8_t acpi_aml_exec_to_bcd(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
	acpi_aml_object_t* src = opcode->operands[0];
	acpi_aml_object_t* dst = opcode->operands[1];

	if(src == NULL || dst == NULL) {
		return -1;
	}

	int64_t ival = 0;
	if(acpi_aml_read_as_integer(ctx, src, &ival) != 0) {
		return -1;
	}

	uint64_t ires = 0;

	int64_t rem = 0;
	int8_t bitshift = 0;

	while(ival > 0) {
		rem = ival % 10;

		ires |= rem << bitshift;

		bitshift += 4;

		ival = ival / 10;
	}

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
	res->number.bytecnt = (ctx->revision == 2 ? 8 : 4);

	opcode->return_obj = res;

	return 0;
}

int8_t acpi_aml_exec_from_bcd(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
	acpi_aml_object_t* src = opcode->operands[0];
	acpi_aml_object_t* dst = opcode->operands[1];

	if(src == NULL || dst == NULL) {
		return -1;
	}

	int64_t ival = 0;
	if(acpi_aml_read_as_integer(ctx, src, &ival) != 0) {
		return -1;
	}

	uint64_t ires = 0;

	int64_t rem = 0;
	int8_t bitshift = 0;

	while(ival > 0) {
		rem = (ival >> bitshift) & 0xF;

		ires += rem * (bitshift / 4);

		bitshift += 4;

		ival = ival >> 4;
	}

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
	res->number.bytecnt = (ctx->revision == 2 ? 8 : 4);

	opcode->return_obj = res;

	return 0;
}


#define UNIMPLEXEC(name) \
	int8_t acpi_aml_exec_ ## name(acpi_aml_parser_context_t * ctx, acpi_aml_opcode_t * opcode){ \
		UNUSED(ctx); \
		printf("ACPIAML: FATAL method %s for opcode 0x%04x not implemented\n", #name, opcode->opcode); \
		return -1; \
	}


UNIMPLEXEC(to_buffer);
UNIMPLEXEC(to_decimalstring);
UNIMPLEXEC(to_hexstring);
UNIMPLEXEC(to_integer);
UNIMPLEXEC(to_string);
