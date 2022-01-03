/**
 * @file acpi_aml_parser_opcodes.64.c
 * @brief acpi aml opcode parser methods
 */

 #include <acpi/aml_internal.h>
 #include <video.h>


int8_t acpi_aml_parse_byte_data(acpi_aml_parser_context_t*, void**, uint64_t*);
int8_t acpi_aml_parse_op_else(acpi_aml_parser_context_t*, void**, uint64_t*);

int8_t acpi_aml_parse_op_code_with_cnt(uint16_t oc, uint8_t opcnt, acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed, acpi_aml_object_t* preop){
	uint64_t r_consumed = 0;
	uint8_t idx = 0;
	int8_t res = -1;
	acpi_aml_opcode_t* opcode = NULL;
	acpi_aml_object_t* return_obj = NULL;
	boolean_t not_destroy[6];

	if(oc == (ACPI_AML_EXTOP_PREFIX << 8 | ACPI_AML_REVISION)) {
		return_obj = acpi_aml_symbol_lookup_at_table(ctx, ctx->symbols, "\\", "_REV");
		res = 0;

	} else if(oc == (ACPI_AML_EXTOP_PREFIX << 8 | ACPI_AML_DEBUG)) {
		return_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

		if(return_obj != NULL) {
			return_obj->type = ACPI_AML_OT_DEBUG;
			res = 0;
		}

	} else if(oc == (ACPI_AML_EXTOP_PREFIX << 8 | ACPI_AML_TIMER)) {
		return_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

		if(return_obj != NULL) {
			return_obj->type = ACPI_AML_OT_TIMER;
			return_obj->timer_value = ctx->timer;
			res = 0;
		}

	} else if(oc >= ACPI_AML_LOCAL0 && oc <= ACPI_AML_ARG6) {
		return_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

		if(return_obj != NULL) {
			return_obj->type = ACPI_AML_OT_LOCAL_OR_ARG;
			return_obj->local_or_arg.idx_local_or_arg = oc - 0x60;
			res = 0;
		}

	} else if(oc == ACPI_AML_CONTINUE) {
		ctx->flags.while_cont = 1;

	} else if(oc == ACPI_AML_BREAK) {
		ctx->flags.while_break = 1;

	} else if(oc == ACPI_AML_NOOP || oc == ACPI_AML_BREAKPOINT) {
		res = 0;

	} else {
		opcode = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_opcode_t), 0x0);

		if(opcode == NULL) {
			PRINTLOG(ACPIAML, LOG_ERROR, "Cannot allocate memory for opcode", 0);
			return -1;
		}

		opcode->opcode = oc;

		if(preop != NULL) {
			opcode->operand_count = 1 + opcnt;
			opcode->operands[0] = preop;
			idx = 1;
		} else {
			opcode->operand_count = opcnt;
		}

		PRINTLOG(ACPIAML, LOG_TRACE, "scope %s opcode 0x%04x", ctx->scope_prefix, opcode->opcode);

		if(oc == (ACPI_AML_EXTOP_PREFIX << 8 | ACPI_AML_CONDREF)) {
			ctx->flags.dismiss_execute_method = 1;
		}

		for(; idx < opcode->operand_count; idx++) {
			uint64_t t_consumed = 0;
			acpi_aml_object_t* op = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

			if(op == NULL) {
				PRINTLOG(ACPIAML, LOG_ERROR, "Cannot allocate memory for op", 0);
				res = -1;
				goto cleanup;
			}

			if(acpi_aml_parse_one_item(ctx, (void**)&op, &t_consumed) != 0) {
				PRINTLOG(ACPIAML, LOG_ERROR, "Cannot parse the op %i for opcode", idx);
				res = -1;
				goto cleanup;
			}

			if(op->type == ACPI_AML_OT_OPCODE_EXEC_RETURN) {
				acpi_aml_object_t* tmp = op->opcode_exec_return;
				memory_free_ext(ctx->heap, op); // free only object's self not reference.
				op = tmp;
			}

			if(op == NULL) {
				PRINTLOG(ACPIAML, LOG_FATAL, "scope %s null operand at exec return rem=%i", ctx->scope_prefix, ctx->remaining);
				res = -1;
				goto cleanup;
			}

			if(op->type == ACPI_AML_OT_LOCAL_OR_ARG) {
				not_destroy[idx] = 1;
			}

			if(oc == ACPI_AML_METHODCALL) {
				op = acpi_aml_get_if_arg_local_obj(ctx, op, 0, 0);
			}

			op = acpi_aml_get_real_object(ctx, op);

			if(op == NULL) {
				PRINTLOG(ACPIAML, LOG_FATAL, "scope %s null operand", ctx->scope_prefix);
				res = -1;
				goto cleanup;
			} else {
				PRINTLOG(ACPIAML, LOG_TRACE, "scope %s param name %s type %i %i", ctx->scope_prefix, op->name, op->type, op->type == ACPI_AML_OT_LOCAL_OR_ARG?op->local_or_arg.idx_local_or_arg:-1);
			}

			opcode->operands[idx] = op;
			r_consumed += t_consumed;
		}


		if(oc == (ACPI_AML_EXTOP_PREFIX << 8 | ACPI_AML_CONDREF)) {
			ctx->flags.dismiss_execute_method = 0;
		}

		if(acpi_aml_executor_opcode(ctx, opcode) != 0) {
			PRINTLOG(ACPIAML, LOG_ERROR, "Cannot execute opcode", 0);
			res = -1;
			goto cleanup;
		}

		return_obj = opcode->return_obj;

		if(return_obj) {
			PRINTLOG(ACPIAML, LOG_TRACE, "scope %s return name %s type %i", ctx->scope_prefix, return_obj->name, return_obj->type);
		} else {
			PRINTLOG(ACPIAML, LOG_TRACE, "scope %s nulll return", ctx->scope_prefix);
		}


		res = 0;
	}

	if(res == 0 && data != NULL) {
		acpi_aml_object_t* resobj = (acpi_aml_object_t*)*data;
		resobj->type =  ACPI_AML_OT_OPCODE_EXEC_RETURN;
		resobj->opcode_exec_return = return_obj;
	}  else {
		if(return_obj && return_obj->name == NULL) {
			// FIXME: when tgt and return_obj same never destroy obj
			// acpi_aml_destroy_object(ctx, return_obj);
		}
	}

	if(consumed != NULL) {
		*consumed += r_consumed;
	}

cleanup:
	if(opcode == NULL) {
		return res;
	}

	if(res != 0) {
		PRINTLOG(ACPIAML, LOG_ERROR, "Cannot parse opcode", 0);
	}

	for(uint8_t i = 0; i < idx; i++) {
		if(not_destroy[i] == 0 && opcode->operands[i] != NULL && opcode->operands[i]->name == NULL) {
			if(opcode->operands[i]->type == ACPI_AML_OT_OPCODE_EXEC_RETURN) {
				acpi_aml_object_t* tmp = opcode->operands[i]->opcode_exec_return;

				if(tmp && tmp->name == NULL && return_obj != tmp) {
					acpi_aml_destroy_object(ctx, tmp);
				}
			}

			if(return_obj != opcode->operands[i]) {
				acpi_aml_destroy_object(ctx, opcode->operands[i]);
			}

		}
	}
	memory_free_ext(ctx->heap, opcode);

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

	return 0;
}

int8_t acpi_aml_parse_op_if(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	uint64_t t_consumed = 0;
	uint64_t r_consumed = 1;
	uint64_t plen;

	ctx->data++;
	ctx->remaining--;

	r_consumed += ctx->remaining;
	plen = acpi_aml_parse_package_length(ctx);
	r_consumed -= ctx->remaining;
	r_consumed += plen;

	t_consumed = 0;
	acpi_aml_object_t* predic = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

	if(predic == NULL) {
		return -1;
	}

	if(acpi_aml_parse_one_item(ctx, (void**)&predic, &t_consumed) != 0) {
		memory_free_ext(ctx->heap, predic);
		return -1;
	}

	plen -= t_consumed;

	int64_t res = 0;

	if(acpi_aml_read_as_integer(ctx, predic, &res) != 0) {
		return -1;
	}

	if(predic->type == ACPI_AML_OT_OPCODE_EXEC_RETURN) {
		if(predic->opcode_exec_return->name == NULL) {
			acpi_aml_destroy_object(ctx, predic->opcode_exec_return);
		}
	}

	if(predic->name == NULL) {
		acpi_aml_destroy_object(ctx, predic);
	}

	if(res != 0) {

		uint64_t old_length = ctx->length;
		uint64_t old_remaining = ctx->remaining;

		ctx->length = plen;
		ctx->remaining = plen;

		if(acpi_aml_parse_all_items(ctx, NULL, NULL) != 0) {
			return -1;
		}

		ctx->length = old_length;
		ctx->remaining = old_remaining - plen;

	} else { // discard if part
		ctx->data += plen;
		ctx->remaining -= plen;
	}

	if(ctx->remaining && *ctx->data == ACPI_AML_ELSE) {
		if(res != 0) { // discard else when if executed
			// pop else op code
			ctx->data++;
			ctx->remaining--;
			r_consumed++;

			r_consumed += ctx->remaining;
			plen = acpi_aml_parse_package_length(ctx);
			r_consumed -= ctx->remaining;
			r_consumed += plen;

			// discard else part
			ctx->data += plen;
			ctx->remaining -= plen;
		} else { // parse else part
			t_consumed = 0;
			if(acpi_aml_parse_op_else(ctx, data, &t_consumed) != 0) {
				return -1;
			}
			r_consumed += t_consumed;
		}
	}

	if(consumed != NULL) {
		*consumed = r_consumed;
	}

	return 0;
}

int8_t acpi_aml_parse_op_else(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	uint64_t r_consumed = 1;
	uint64_t plen;

	ctx->data++;
	ctx->remaining--;

	r_consumed += ctx->remaining;
	plen = acpi_aml_parse_package_length(ctx);
	r_consumed -= ctx->remaining;
	r_consumed += plen;



	uint64_t old_length = ctx->length;
	uint64_t old_remaining = ctx->remaining;

	ctx->length = plen;
	ctx->remaining = plen;

	if(acpi_aml_parse_all_items(ctx, NULL, NULL) != 0) {
		return -1;
	}

	ctx->length = old_length;
	ctx->remaining = old_remaining - plen;


	if(consumed != NULL) {
		*consumed = r_consumed;
	}

	return 0;
}

int8_t acpi_aml_parse_fatal(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(consumed);

	ctx->data++;
	ctx->remaining--;

	// get fatal type 1 byte
	ctx->fatal_error.type = *ctx->data;
	ctx->data++;
	ctx->remaining--;

	// get fatal code 4 byte
	ctx->fatal_error.type = *((uint32_t*)(ctx->data));
	ctx->data += 4;
	ctx->remaining -= 4;

	acpi_aml_object_t* arg = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

	if(arg == NULL) {
		return -1;
	}

	if(acpi_aml_parse_one_item(ctx, (void**)&arg, NULL) != 0) {
		memory_free_ext(ctx->heap, arg);
		return -1;
	}

	int64_t fatal_ival = 0;

	acpi_aml_read_as_integer(ctx, arg, &fatal_ival);

	memory_free_ext(ctx->heap, arg);

	ctx->fatal_error.arg = fatal_ival;

	ctx->flags.fatal = 1;

	return -1; // fatal always -1 because it is fatal :)
}

int8_t acpi_aml_parse_op_match(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	uint64_t r_consumed = 1;
	uint64_t t_consumed = 0;
	int8_t res = -1;


	acpi_aml_opcode_t* opcode = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_opcode_t), 0x0);

	if(opcode == NULL) {
		return -1;
	}

	opcode->opcode = *ctx->data;
	opcode->operand_count = 6;

	ctx->data++;
	ctx->remaining--;

	uint8_t idx = 0;

	for(uint8_t i = 0; i < 2; i++) {
		acpi_aml_object_t* op = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

		if(op == NULL) {
			return -1;
		}

		t_consumed = 0;
		if(acpi_aml_parse_one_item(ctx, (void**)&op, &t_consumed) != 0) {
			if(op->name == NULL) {
				acpi_aml_destroy_object(ctx, op);
			}
			return -1;
		}
		r_consumed += t_consumed;
		opcode->operands[idx++] = op;

		acpi_aml_object_t* moc = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

		if(moc == NULL) {
			return -1;
		}

		t_consumed = 0;
		if(acpi_aml_parse_byte_data(ctx, (void**)&moc, &t_consumed) != 0) {
			memory_free_ext(ctx->heap, moc);
			return -1;
		}
		r_consumed += t_consumed;
		opcode->operands[idx++] = moc;
	}

	for(uint8_t i = 0; i < 2; i++) {
		acpi_aml_object_t* op = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

		if(op == NULL) {
			return -1;
		}

		t_consumed = 0;
		if(acpi_aml_parse_one_item(ctx, (void**)&op, &t_consumed) != 0) {
			if(op->name == NULL) {
				acpi_aml_destroy_object(ctx, op);
			}
			return -1;
		}
		r_consumed += t_consumed;
		opcode->operands[idx++] = op;
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
		if(opcode->operands[i] != NULL && opcode->operands[i]->name == NULL) {
			memory_free_ext(ctx->heap, opcode->operands[i]);
		}
	}
	memory_free_ext(ctx->heap, opcode);

	return res;
}

int8_t acpi_aml_parse_op_while(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	uint64_t t_consumed = 0;
	uint64_t r_consumed = 1;
	uint64_t plen;

	ctx->data++;
	ctx->remaining--;

	r_consumed += ctx->remaining;
	plen = acpi_aml_parse_package_length(ctx);
	r_consumed -= ctx->remaining;
	r_consumed += plen;


	uint64_t old_length = ctx->length;
	uint64_t next_remaining = ctx->remaining - plen;

	uint8_t* old_data = ctx->data;
	uint8_t* next_data = old_data + plen;

	acpi_aml_object_t* predic = NULL;

	while(1) {
		ctx->length = plen;
		ctx->remaining = plen;
		ctx->data = old_data;

		predic = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

		if(predic == NULL) {
			return -1;
		}

		t_consumed = 0;

		if(acpi_aml_parse_one_item(ctx, (void**)&predic, &t_consumed) != 0) {
			memory_free_ext(ctx->heap, predic);
			return -1;
		}

		int64_t predic_res = 0;

		if(acpi_aml_read_as_integer(ctx, predic, &predic_res) != 0) {
			memory_free_ext(ctx->heap, predic);
			return -1;
		}

		if(predic->type == ACPI_AML_OT_OPCODE_EXEC_RETURN) {
			if(predic->opcode_exec_return->name == NULL) {
				acpi_aml_destroy_object(ctx, predic->opcode_exec_return);
			}
		}

		if(predic->name == NULL) {
			acpi_aml_destroy_object(ctx, predic);
		}

		if(predic_res == 0) {
			break;
		}

		int8_t res = acpi_aml_parse_all_items(ctx, NULL, NULL);

		if(res == -1) {
			if(ctx->flags.while_break == 1) {
				ctx->flags.while_break = 0;
				break; // while loop ended
			}
			if(ctx->flags.while_cont == 1) {
				ctx->flags.while_cont = 0;
				continue;   // while loop restarted
			}
			return -1; // error at parsing
		}

		if(ctx->flags.method_return) {
			break;
		}
	}

	ctx->length = old_length;
	ctx->remaining = next_remaining;
	ctx->data = next_data;


	if(consumed != NULL) {
		*consumed += r_consumed;
	}

	return 0;
}
