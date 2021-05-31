/**
 * @file acpi_aml_parser_opcodes.64.c
 * @brief acpi aml opcode parser methods
 */

 #include <acpi/aml.h>

int8_t acpi_aml_parse_op_code_with_cnt(uint16_t oc, uint8_t opcnt, acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed, acpi_aml_object_t* preop){
	uint64_t r_consumed = 0;
	uint8_t idx = 0;
	int8_t res = -1;

	apci_aml_opcode_t* opcode = memory_malloc(sizeof(apci_aml_opcode_t));
	opcode->opcode = oc;

	if(preop != NULL) {
		opcode->operand_count = 1 + opcnt;
		opcode->operands[0] = preop;
		idx = 1;
	} else {
		opcode->operand_count = opcnt;
	}

	for(; idx < opcnt; idx++) {
		uint64_t t_consumed = 0;
		acpi_aml_object_t* op = memory_malloc(sizeof(acpi_aml_object_t));

		if(acpi_aml_parse_one_item(ctx, (void**)&op, &t_consumed) != 0) {
			goto cleanup;
		}

		opcode->operands[idx] = op;
		r_consumed += t_consumed;
	}

	if(acpi_aml_executor_opcode(ctx, opcode) != 0) {
		goto cleanup;
	}

	if(data != NULL) {
		acpi_aml_object_t* resobj = (acpi_aml_object_t*)*data;
		resobj->type = ACPI_AML_OT_OPCODE_EXEC_RETURN;
		resobj->opcode_exec_return = opcode->return_obj;
	}

	if(consumed != NULL) {
		*consumed += r_consumed;
	}

	res = 0;

cleanup:

	for(uint8_t i = 0; i < idx; i++) {
		if(opcode->operands[i] != NULL && opcode->operands[i]->refcount == 0) {
			memory_free(opcode->operands[i]);
		}
	}
	memory_free(opcode);

	return res;
}

#define OPCODEPARSER(num) \
	int8_t acpi_aml_parse_opcnt_ ## num(acpi_aml_parser_context_t * ctx, void** data, uint64_t * consumed){ \
		uint64_t t_consumed = 1; \
		uint8_t oc = *ctx->data; \
		ctx->data++; \
		ctx->remaining--; \
     \
		if(acpi_aml_parse_op_code_with_cnt(oc, num, ctx, data, &t_consumed, NULL) != 0) { \
			return -1; \
		} \
     \
		if(consumed != NULL) { \
			*consumed = t_consumed; \
		} \
     \
		return 0; \
	}

OPCODEPARSER(0);
OPCODEPARSER(1);
OPCODEPARSER(2);
OPCODEPARSER(3);
OPCODEPARSER(4);

#define EXTOPCODEPARSER(num) \
	int8_t acpi_aml_parse_extopcnt_ ## num(acpi_aml_parser_context_t * ctx, void** data, uint64_t * consumed){ \
		uint64_t t_consumed = 1; \
		uint16_t oc = 0x5b00; \
		uint8_t t_oc = *ctx->data; \
		oc |= t_oc; \
		ctx->data++; \
		ctx->remaining--; \
     \
		if(acpi_aml_parse_op_code_with_cnt(oc, num, ctx, data, &t_consumed, NULL) != 0) { \
			return -1; \
		} \
     \
		if(consumed != NULL) { \
			*consumed = t_consumed; \
		} \
     \
		return 0; \
	}

EXTOPCODEPARSER(0);
EXTOPCODEPARSER(1);
EXTOPCODEPARSER(2);
EXTOPCODEPARSER(6);

int8_t acpi_aml_parse_logic_ext(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(ctx);
	uint64_t t_consumed = 0;
	uint16_t oc = 0;

	uint8_t oc_t = *ctx->data;
	ctx->data++;
	ctx->remaining--;
	oc = oc_t;

	oc_t = *ctx->data;
	if(oc_t == ACPI_AML_LEQUAL || oc_t == ACPI_AML_LGREATER || oc_t == ACPI_AML_LLESS) {
		ctx->data++;
		ctx->remaining--;
		t_consumed = 2;
		oc |= ((uint16_t)oc_t) << 8;
		if(acpi_aml_parse_op_code_with_cnt(oc, 2, ctx, data, &t_consumed, NULL) != 0) {
			return -1;
		}
	} else {
		t_consumed = 1;
		if(acpi_aml_parse_op_code_with_cnt(oc, 1, ctx, data, &t_consumed, NULL) != 0) {
			return -1;
		}
	}

	if(consumed != NULL) {
		*consumed = t_consumed;
	}

	return -1;
}

UNIMPLPARSER(fatal);
UNIMPLPARSER(op_match);

UNIMPLPARSER(op_if);
UNIMPLPARSER(op_else);
UNIMPLPARSER(op_while);
