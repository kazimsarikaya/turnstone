/**
 * @file acpi_aml_executor.64.c
 * @brief acpi aml executor methods
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <acpi/aml_internal.h>
#include <video.h>
#include <bplustree.h>



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
		PRINTLOG(ACPIAML, LOG_FATAL, "unknown op code 0x%04x for execution", opcode->opcode);
		return -1;
	}

	acpi_aml_exec_f exec_f = acpi_aml_exec_fs[idx];

	if(exec_f == NULL) {
		PRINTLOG(ACPIAML, LOG_FATAL, "unwanted op code for execution");
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
	} else if (obj->type == ACPI_AML_OT_BUFFERFIELD) {
		int64_t data = 0;
		if(acpi_aml_read_as_integer(ctx, obj, &data) != 0) {
			return -1;
		}

		acpi_aml_object_t* tmp = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);
		tmp->type = ACPI_AML_OT_NUMBER;
		tmp->number.bytecnt = 1;
		tmp->number.value = data;

		obj = tmp;

	} else {
		return -1;
	}

	opcode->return_obj = obj;

	return 0;
}

int8_t acpi_aml_exec_mth_return(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
	acpi_aml_object_t* obj = opcode->operands[0];

	if(ctx->method_context == NULL) {
		ctx->flags.fatal = 1;
		return -1;
	}

	ctx->flags.method_return = 1;

	acpi_aml_method_context_t* mthctx = ctx->method_context;

	obj = acpi_aml_get_if_arg_local_obj(ctx, obj, 0, 0);

	mthctx->mthobjs[15] = obj; // acpi_aml_duplicate_object(ctx, obj);
	opcode->return_obj = obj;

	PRINTLOG(ACPIAML, LOG_TRACE, "ctx %s method return obj type %i", ctx->scope_prefix, obj->type)

	return 0;
}

int8_t acpi_aml_execute(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* mth, acpi_aml_object_t** return_obj, ...) {
	acpi_aml_opcode_t* opcode = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_opcode_t), 0);
	opcode->opcode = ACPI_AML_METHODCALL;
	opcode->operands[0] = mth;
	opcode->operand_count = 1;

	va_list args;
	va_start(args, return_obj);

	for(uint32_t i = 0; i < mth->method.arg_count; i++) {
		opcode->operands[i + 1] = va_arg(args, acpi_aml_object_t*);
		opcode->operand_count++;
	}

	va_end(args);

	int8_t res = acpi_aml_exec_method(ctx, opcode);

	if(return_obj) {
		if(opcode->return_obj) {
			PRINTLOG(ACPIAML, LOG_TRACE, "return obj type %i", opcode->return_obj->type);
		}

		*return_obj = opcode->return_obj;
	} else {
		acpi_aml_destroy_object(ctx, opcode->return_obj);
	}

	memory_free_ext(ctx->heap, opcode);

	return res;
}

int8_t acpi_aml_exec_method(acpi_aml_parser_context_t* ctx, acpi_aml_opcode_t* opcode){
	int8_t res = -1;

	acpi_aml_object_t** mthobjs = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t*) * 16, 0x0);

	if(mthobjs == NULL) {
		return -1;
	}

	for(uint8_t i = 0; i < opcode->operand_count - 1; i++) {
		mthobjs[8 + i] = opcode->operands[1 + i];
	}

	acpi_aml_method_context_t* mthctx = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_method_context_t), 0x0);

	if(mthctx == NULL) {
		memory_free_ext(ctx->heap, mthobjs);
		return -1;
	}

	mthctx->mthobjs = mthobjs;
	mthctx->arg_count = opcode->operand_count - 1;   //first op is method call object


	char_t* old_scope_prefix = ctx->scope_prefix;
	uint8_t inside_method = ctx->flags.inside_method;
	uint8_t* old_data = ctx->data;
	uint64_t old_length = ctx->length;
	uint64_t old_remaining = ctx->remaining;
	acpi_aml_method_context_t* old_mthctx =  ctx->method_context;
	index_t* old_local_symbols = ctx->local_symbols;

	acpi_aml_object_t* mth = opcode->operands[0];


	ctx->scope_prefix = mth->name;
	ctx->flags.inside_method = 1;
	ctx->data = mth->method.termlist;
	ctx->length = mth->method.termlist_length;
	ctx->remaining = mth->method.termlist_length;
	ctx->method_context = mthctx;
	ctx->local_symbols = bplustree_create_index_with_heap(ctx->heap, 20, acpi_aml_object_name_comparator);

	res = acpi_aml_parse_all_items(ctx, NULL, NULL);


	iterator_t* iter = ctx->local_symbols->create_iterator(ctx->local_symbols);

	while(iter->end_of_iterator(iter) != 0) {
		acpi_aml_object_t* tmp = iter->get_item(iter);

		if(tmp == mthobjs[15]) {
			tmp = acpi_aml_duplicate_object(ctx, tmp);
			memory_free_ext(ctx->heap, tmp->name);
			tmp->name = NULL;
			mthobjs[15] = tmp;
		}

		iter = iter->next(iter);
	}

	iter->destroy(iter);

	for(uint32_t i = 0; i < 7; i++) {
		if(mthctx->dirty_args[i]) {
			acpi_aml_destroy_object(ctx, mthobjs[i + 8]);
		}
	}

	acpi_aml_destroy_symbol_table(ctx, 1);

	if(res == 0 && ctx->flags.fatal == 0 && ctx->flags.method_return == 1) {
		PRINTLOG(ACPIAML, LOG_TRACE, "return obj type %i", mthobjs[15]->type);
		opcode->return_obj = mthobjs[15];
		res = 0;

		if(opcode->return_obj) {
			PRINTLOG(ACPIAML, LOG_TRACE, "ctx %s method execution finished. obj name %s type %i", ctx->scope_prefix, opcode->return_obj->name, opcode->return_obj->type);
		}

	}

	for(uint8_t i = 0; i < 8; i++) {
		if(mthobjs[i] && mthobjs[i] != opcode->return_obj) {
			acpi_aml_destroy_object(ctx, mthobjs[i]);
		}
	}

	memory_free_ext(ctx->heap, mthobjs);
	memory_free_ext(ctx->heap, mthctx);

	ctx->scope_prefix = old_scope_prefix;
	ctx->flags.inside_method = inside_method;
	ctx->flags.method_return = 0;
	ctx->data = old_data;
	ctx->length = old_length;
	ctx->remaining = old_remaining;
	ctx->method_context = old_mthctx;
	ctx->local_symbols = old_local_symbols;

	return res;
}

#define UNIMPLEXEC(name) \
	int8_t acpi_aml_exec_ ## name(acpi_aml_parser_context_t * ctx, acpi_aml_opcode_t * opcode){ \
		UNUSED(ctx); \
		PRINTLOG(ACPIAML, LOG_ERROR, "method %s for opcode 0x%04x not implemented", #name, opcode->opcode); \
		return -1; \
	}

UNIMPLEXEC(copy);

UNIMPLEXEC(notify);

UNIMPLEXEC(stall);
UNIMPLEXEC(sleep);
