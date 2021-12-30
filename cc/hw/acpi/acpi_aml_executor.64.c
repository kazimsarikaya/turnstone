/**
 * @file acpi_aml_executor.64.c
 * @brief acpi aml executor methods
 */

#include <acpi/aml_internal.h>
#include <video.h>



typedef int8_t (* acpi_aml_exec_f)(acpi_aml_parser_context_t*, acpi_aml_opcode_t*);

#define CREATE_EXEC_F(name) int8_t acpi_aml_exec_ ## name(acpi_aml_parser_context_t*, acpi_aml_opcode_t*);
#define EXEC_F_NAME(name) acpi_aml_exec_ ## name


CREATE_EXEC_F(store);
CREATE_EXEC_F(refof);
CREATE_EXEC_F(concat);
CREATE_EXEC_F(findsetbit);
CREATE_EXEC_F(derefof);
CREATE_EXEC_F(concatres);
CREATE_EXEC_F(notify);
CREATE_EXEC_F(op_sizeof);
CREATE_EXEC_F(index);
CREATE_EXEC_F(match);
CREATE_EXEC_F(object_type);
CREATE_EXEC_F(to_buffer);
CREATE_EXEC_F(to_decimalstring);
CREATE_EXEC_F(to_hexstring);
CREATE_EXEC_F(to_integer);
CREATE_EXEC_F(to_string);

CREATE_EXEC_F(op1_tgt0_maths);
CREATE_EXEC_F(op1_tgt1_maths);
CREATE_EXEC_F(op2_tgt1_maths);
CREATE_EXEC_F(op2_tgt2_maths);

CREATE_EXEC_F(op2_logic);


CREATE_EXEC_F(copy);
CREATE_EXEC_F(mid);

CREATE_EXEC_F(mth_return);

CREATE_EXEC_F(condrefof);
CREATE_EXEC_F(load_table);
CREATE_EXEC_F(load);
CREATE_EXEC_F(stall);
CREATE_EXEC_F(sleep);
CREATE_EXEC_F(acquire);
CREATE_EXEC_F(signal);
CREATE_EXEC_F(wait);
CREATE_EXEC_F(reset);
CREATE_EXEC_F(release);
CREATE_EXEC_F(from_bcd);
CREATE_EXEC_F(to_bcd);

CREATE_EXEC_F(method);

acpi_aml_exec_f acpi_aml_exec_fs[] = {
	EXEC_F_NAME(store), // 0x70
	EXEC_F_NAME(refof),
	EXEC_F_NAME(op2_tgt1_maths),
	EXEC_F_NAME(concat),
	EXEC_F_NAME(op2_tgt1_maths),
	EXEC_F_NAME(op1_tgt0_maths),
	EXEC_F_NAME(op1_tgt0_maths),
	EXEC_F_NAME(op2_tgt1_maths),
	EXEC_F_NAME(op2_tgt2_maths),
	EXEC_F_NAME(op2_tgt1_maths),
	EXEC_F_NAME(op2_tgt1_maths), // 0x7A
	EXEC_F_NAME(op2_tgt1_maths),
	EXEC_F_NAME(op2_tgt1_maths),
	EXEC_F_NAME(op2_tgt1_maths),
	EXEC_F_NAME(op2_tgt1_maths),
	EXEC_F_NAME(op2_tgt1_maths),
	EXEC_F_NAME(op1_tgt1_maths), // 0x80
	EXEC_F_NAME(findsetbit),
	EXEC_F_NAME(findsetbit),
	EXEC_F_NAME(derefof),
	EXEC_F_NAME(concatres),
	EXEC_F_NAME(op2_tgt1_maths),
	EXEC_F_NAME(notify),
	EXEC_F_NAME(op_sizeof),
	EXEC_F_NAME(index),
	EXEC_F_NAME(match),
	NULL, // 0x8A
	NULL,
	NULL,
	NULL,
	EXEC_F_NAME(object_type),
	NULL,
	EXEC_F_NAME(op2_logic), // 0x90
	EXEC_F_NAME(op2_logic),
	EXEC_F_NAME(op2_logic),
	EXEC_F_NAME(op2_logic),
	EXEC_F_NAME(op2_logic),
	EXEC_F_NAME(op2_logic),
	EXEC_F_NAME(to_buffer),
	EXEC_F_NAME(to_decimalstring),
	EXEC_F_NAME(to_hexstring),
	EXEC_F_NAME(to_integer),
	NULL,
	NULL,
	EXEC_F_NAME(to_string),
	EXEC_F_NAME(copy),
	EXEC_F_NAME(mid), // 0x9f -> index 46

	EXEC_F_NAME(mth_return), // 0xa4 -> index 47

	EXEC_F_NAME(condrefof), // 0x5b12 -> index 48
	EXEC_F_NAME(load_table), // 0x5b1f -> index 49
	EXEC_F_NAME(load), // 0x5b20
	EXEC_F_NAME(stall), // 0x5b21
	EXEC_F_NAME(sleep), // 0x5b22
	EXEC_F_NAME(acquire), // 0x5b23
	EXEC_F_NAME(signal), // 0x5b24
	EXEC_F_NAME(wait), // 0x5b25
	EXEC_F_NAME(reset), // 0x5b26
	EXEC_F_NAME(release), // 0x5b27
	EXEC_F_NAME(from_bcd), // 0x5b28
	EXEC_F_NAME(to_bcd), // 0x5b29 -> index 59

	EXEC_F_NAME(method) // 0xFE -> index 60
};


int8_t acpi_aml_executor_opcode(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
	int16_t idx = -1;

	if((opcode->opcode & 0x5b00) == 0x5b00) { // if extended ?
		int16_t tmp = opcode->opcode & 0xFF;

		if(tmp == 0x12) {
			idx = 48;
		} else if(tmp >= 0x1f && tmp <= 0x29) {
			idx = tmp - 49;
		}
	} else { // or
		int16_t tmp = opcode->opcode & 0xFF;
		if(tmp == 0xA4) {
			idx = 47;
		} else if(tmp == 0xFE) {
			idx = 60;
		} else if(tmp >= 0x70 && tmp <= 0xA5) {
			idx = tmp - 0x70;
		}
	}

	if(idx == -1) {
		printf("ACPIAML: FATAL unknown op code 0x%04x for execution\n", opcode->opcode);
		return -1;
	}

	acpi_aml_exec_f exec_f = acpi_aml_exec_fs[idx];

	if(exec_f == NULL) {
		printf("ACPIAML: FATAL unwanted op code for execution\n");
		return -1;
	}

	return exec_f(ctx, opcode);
}

int8_t acpi_aml_exec_condrefof(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){

	acpi_aml_object_t* src = opcode->operands[0];
	acpi_aml_object_t* dst = opcode->operands[1];

	src = acpi_aml_get_if_arg_local_obj(ctx, src, 0, 1);

	acpi_aml_object_t* res = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

	if(res == NULL) {
		return -1;
	}

	res->type = ACPI_AML_OT_NUMBER;
	res->number.bytecnt = 1;

	if(src == NULL) {
		res->number.value = 0;
	} else {
		dst = acpi_aml_get_if_arg_local_obj(ctx, dst, 1, 1);

		if(acpi_aml_is_null_target(dst) != 0) {
			dst->type = ACPI_AML_OT_REFOF;
			dst->refof_target = src;
		}

		res->number.value = 1;
	}

	opcode->return_obj = res;

	return 0;
}

int8_t acpi_aml_exec_refof(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
	UNUSED(ctx);

	acpi_aml_object_t* obj = opcode->operands[0];
	obj = acpi_aml_get_if_arg_local_obj(ctx, obj, 0, 0);

	if(obj == NULL) {
		return -1;
	}

	acpi_aml_object_t* refof = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

	if(refof == NULL) {
		return -1;
	}

	refof->type = ACPI_AML_OT_REFOF;
	refof->refof_target = obj;

	opcode->return_obj = refof;

	return 0;
}

int8_t acpi_aml_exec_derefof(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
	UNUSED(ctx);

	acpi_aml_object_t* obj = opcode->operands[0];

	obj = acpi_aml_get_if_arg_local_obj(ctx, obj, 0, 0);

	if(obj == NULL) {
		return -1;
	}

	if(obj->type == ACPI_AML_OT_STRING) {
		obj = acpi_aml_symbol_lookup(ctx, obj->string);
	} else if (obj->type == ACPI_AML_OT_REFOF) {
		obj = obj->refof_target;
	} else {
		return -1;
	}

	opcode->return_obj = obj;

	return 0;
}

int8_t acpi_aml_exec_mth_return(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
	acpi_aml_object_t* obj = opcode->operands[0];

	ctx->flags.method_return = 1;

	if(ctx->method_context == NULL) {
		ctx->flags.fatal = 1;
		return -1;
	}

	acpi_aml_method_context_t* mthctx = ctx->method_context;

	obj = acpi_aml_get_if_arg_local_obj(ctx, obj, 0, 0);

	mthctx->mthobjs[15] = obj; //acpi_aml_duplicate_object(ctx, obj);

	return -1;
}

int8_t acpi_aml_exec_method(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
	int8_t res = -1;
	int8_t lsymtbl_created_by_me = 0;

	acpi_aml_method_context_t* mthctx = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_method_context_t), 0x0);

	if(mthctx == NULL) {
		return -1;
	}

	mthctx->arg_count = opcode->operand_count - 1; //first op is method call object

	acpi_aml_object_t* mthobjs = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t) * 16, 0x0);

	if(mthobjs == NULL) {
		memory_free_ext(ctx->heap, mthctx);
		return -1;
	}

	for(uint8_t i = 0; i < opcode->operand_count - 1; i++) {
		mthobjs[8 + i] = opcode->operands[1 + i];
	}

	mthctx->mthobjs = mthobjs;

	acpi_aml_object_t* mth = opcode->operands[0];

	uint8_t inside_method = ctx->flags.inside_method;
	uint8_t* old_data = ctx->data;
	uint64_t old_length = ctx->length;
	uint64_t old_remaining = ctx->remaining;
	acpi_aml_method_context_t* old_mthctx =  ctx->method_context;

	ctx->flags.inside_method = 1;
	ctx->data = mth->method.termlist;
	ctx->length = mth->method.termlist_length;
	ctx->remaining = mth->method.termlist_length;
	ctx->method_context = mthctx;
	linkedlist_t local_symbols = ctx->local_symbols;


	ctx->local_symbols = linkedlist_create_sortedlist_with_heap(ctx->heap, acpi_aml_object_name_comparator);

	res = acpi_aml_parse_all_items(ctx, NULL, NULL);

	if(res == -1 && ctx->flags.fatal == 0 && ctx->flags.method_return == 1) {
		opcode->return_obj = mthobjs[15];
		res = 0;
	}

	ctx->flags.inside_method = inside_method;
	ctx->data = old_data;
	ctx->length = old_length;
	ctx->remaining = old_remaining;
	ctx->method_context = old_mthctx;

	acpi_aml_destroy_symbol_table(ctx, 1);
	ctx->local_symbols = local_symbols;


	for(uint8_t i = 0; i < 8; i++) {
		if(mthobjs[i]) {
			acpi_aml_destroy_object(ctx, mthobjs[i]);
		}
	}

	memory_free_ext(ctx->heap, mthobjs);
	memory_free_ext(ctx->heap, mthctx);

	return res;
}

#define UNIMPLEXEC(name) \
	int8_t acpi_aml_exec_ ## name(acpi_aml_parser_context_t * ctx, acpi_aml_opcode_t * opcode){ \
		UNUSED(ctx); \
		printf("ACPIAML: FATAL method %s for opcode 0x%04x not implemented\n", #name, opcode->opcode); \
		return -1; \
	}

UNIMPLEXEC(copy);

UNIMPLEXEC(notify);

UNIMPLEXEC(stall);
UNIMPLEXEC(sleep);
