/**
 * @file acpi_aml_utils.64.c
 * @brief acpi parser utils
 */
#include <acpi/aml_internal.h>
#include <strings.h>
#include <video.h>


int8_t acpi_aml_is_null_target(acpi_aml_object_t* obj) {
	if(obj == NULL) {
		return 0;
	}

	if(obj->name == NULL && obj->type == ACPI_AML_OT_NUMBER && obj->number.value == 0) {
		return 0;
	}

	return -1;
}

acpi_aml_object_t* acpi_aml_get_if_arg_local_obj(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj, uint8_t write, uint8_t copy) {
	if(obj && obj->type == ACPI_AML_OT_LOCAL_OR_ARG) {
		if(ctx->method_context == NULL) {
			return NULL;
		}

		acpi_aml_method_context_t* mthctx = ctx->method_context;

		uint8_t laidx = obj->local_or_arg.idx_local_or_arg;

		if(laidx > 14) {
			acpi_aml_print_object(ctx, obj);
			printf("ACPIAML: FATAL local/arg index out of bounds %lx\n", laidx);
			return NULL;
		}

		acpi_aml_object_t* la_obj = mthctx->mthobjs[laidx];

		if(la_obj == NULL && laidx <= 7) { // if localX does not exists create it
			la_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);
			la_obj->type = ACPI_AML_OT_UNINITIALIZED;
			mthctx->mthobjs[laidx] = la_obj;
		}

		if(la_obj == NULL) {
			printf("ACPIAML: FATAL local/arg does not exists %lx\n", laidx);
			return NULL;
		}

		if(laidx >= 8) {
			if(write && (mthctx->dirty_args[laidx - 8] == 0 || copy == 1)) {
				la_obj = acpi_aml_duplicate_object(ctx, la_obj);
				memory_free_ext(ctx->heap, la_obj->name);
				la_obj->name = NULL;
				mthctx->mthobjs[laidx] = la_obj;
				mthctx->dirty_args[laidx - 8] = 1;
			}

			if(write == 0 && copy == 1 && mthctx->dirty_args[laidx - 8] == 0) {
				la_obj = acpi_aml_duplicate_object(ctx, la_obj);
				memory_free_ext(ctx->heap, la_obj->name);
				la_obj->name = NULL;
				mthctx->dirty_args[laidx - 8] = 1;
			}
		}

		obj = la_obj;
	}
	return obj;
}

int8_t acpi_aml_read_as_integer(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj, int64_t* res){
	obj = acpi_aml_get_if_arg_local_obj(ctx, obj, 0, 0);

	if(obj == NULL || res == NULL) {
		return -1;
	}

	int64_t ival = 0;
	char_t* strptr;

	switch (obj->type) {
	case ACPI_AML_OT_NUMBER:
		*res = obj->number.value;
		break;
	case ACPI_AML_OT_STRING:
		strptr = obj->string;
		if(strptr[0] == '0' && (strptr[1] == 'x' || strptr[1] == 'X')) {
			strptr += 2;
		}
		while(1) {
			if(*strptr >= 'A' && *strptr <= 'F') {
				ival |= *strptr - 'A' + 10;
			} else if(*strptr >= 'a' && *strptr <= 'f') {
				ival |= *strptr - 'a' + 10;
			} else if(*strptr >= '0' && *strptr <= '9') {
				ival |= *strptr - '0';
			} else {
				ival >>= 4;
				break;
			}
			ival <<= 4;
			strptr++;
		}
		*res = ival;
		break;
	case ACPI_AML_OT_BUFFER:
		*res =  *((int64_t*)obj->buffer.buf);
		break;
	case ACPI_AML_OT_OPCODE_EXEC_RETURN:
		return acpi_aml_read_as_integer(ctx, obj->opcode_exec_return, res);
	default:
		printf("read as integer objtype %li\n", obj->type);
		return -1;
		break;
	}

	return 0;
}


int8_t acpi_aml_write_as_integer(acpi_aml_parser_context_t* ctx, int64_t val, acpi_aml_object_t* obj) {
	obj = acpi_aml_get_if_arg_local_obj(ctx, obj, 1, 0);

	if(obj == NULL) {
		return -1;
	}

	if(obj->type == ACPI_AML_OT_UNINITIALIZED) {
		obj->type = ACPI_AML_OT_NUMBER;
	}

	switch (obj->type) {
	case ACPI_AML_OT_NUMBER:
		obj->number.value = val;
		break;
	default:
		printf("write as integer objtype %li\n", obj->type);
		return -1;
	}

	return 0;
}

int8_t acpi_aml_write_as_string(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* src, acpi_aml_object_t* dst) {
	UNUSED(ctx);
	UNUSED(src);
	UNUSED(dst);
	return -1;
}

int8_t acpi_aml_write_as_buffer(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* src, acpi_aml_object_t* dst) {
	UNUSED(ctx);
	UNUSED(src);
	UNUSED(dst);
	return -1;
}


int8_t acpi_aml_is_lead_name_char(uint8_t* c) {
	if(('A' <= *c && *c <= 'Z') || *c == '_') {
		return 0;
	}
	return -1;
}

int8_t acpi_aml_is_digit_char(uint8_t* c) {
	if('0' <= *c && *c <= '9') {
		return 0;
	}
	return -1;
}

int8_t acpi_aml_is_name_char(uint8_t* data) {
	if(acpi_aml_is_lead_name_char(data) == 0 ||  acpi_aml_is_digit_char(data) == 0 ) {
		return 0;
	}
	return -1;
}

int8_t acpi_aml_is_root_char(uint8_t* c) {
	if (*c == ACPI_AML_ROOT_CHAR) {
		return 0;
	}
	return -1;
}

int8_t acpi_aml_is_parent_prefix_char(uint8_t* c){
	if (*c == ACPI_AML_PARENT_CHAR) {
		return 0;
	}
	return -1;
}



char_t* acpi_aml_normalize_name(acpi_aml_parser_context_t* ctx, char_t* prefix, char_t* name) {
	uint64_t max_len = strlen(prefix) + strlen(name) + 1;
	char_t* dst_name = memory_malloc_ext(ctx->heap, sizeof(char_t) * max_len, 0x0);

	if(dst_name == NULL) {
		return NULL;
	}

	if(acpi_aml_is_root_char((uint8_t*)name) == 0) {
		strcpy(name + 1, dst_name);
	} else {
		uint64_t prefix_cnt = 0;
		char_t* tmp = name;
		while(acpi_aml_is_parent_prefix_char((uint8_t*)tmp) == 0) {
			tmp++;
			prefix_cnt++;
		}
		uint64_t src_len = strlen(prefix) - (prefix_cnt * 4);
		memory_memcopy(prefix, dst_name, src_len);
		strcpy(name + prefix_cnt, dst_name + src_len);
	}
	char_t* nomname = memory_malloc_ext(ctx->heap, sizeof(char_t) * strlen(dst_name) + 1, 0x0);

	if(nomname == NULL) {
		memory_free_ext(ctx->heap, dst_name);
		return NULL;
	}

	strcpy(dst_name, nomname);

	memory_free_ext(ctx->heap, dst_name);

	return nomname;
}



int8_t acpi_aml_is_nameseg(uint8_t* data) {
	if(acpi_aml_is_lead_name_char(data) == 0 && acpi_aml_is_name_char(data + 1) == 0 && acpi_aml_is_name_char(data + 2) == 0
	   && acpi_aml_is_name_char(data + 3) == 0) {
		return 0;
	}
	return -1;
}

int8_t acpi_aml_is_namestring_start(uint8_t* data){
	if(acpi_aml_is_lead_name_char(data) == 0 || acpi_aml_is_root_char(data) == 0 || acpi_aml_is_parent_prefix_char(data) == 0) {
		return 0;
	}
	if(*data == ACPI_AML_DUAL_PREFIX) {
		data++;
		for(int8_t i = 0; i < 8; i++) {
			if(acpi_aml_is_name_char(data + i) != 0) {
				return -1;
			}
		}
		return 0;
	}
	if(*data == ACPI_AML_MULTI_PREFIX) {
		data++;
		uint8_t segcnt = *data;
		data++;
		for(int8_t i = 0; i < 4 * segcnt; i++) {
			if(acpi_aml_is_name_char(data + i) != 0) {
				return -1;
			}
		}
		return 0;
	}
	return -1;
}

uint64_t acpi_aml_parse_package_length(acpi_aml_parser_context_t* ctx){
	uint8_t pkgleadbyte = *ctx->data;
	ctx->data++;
	uint8_t bytecnt = pkgleadbyte >> 6;
	uint8_t usedbytes = 1 + bytecnt;
	uint64_t pkglen = 0;

	if(bytecnt == 0) {
		pkglen = pkgleadbyte & 0x3F;
	}else {
		pkglen = pkgleadbyte & 0x0F;
		uint8_t tmp8 = 0;
		uint64_t tmp64 = 0;
		if(bytecnt > 0) {
			tmp8 = *ctx->data;
			tmp64 = tmp8;
			pkglen = (tmp64 << 4) | pkglen;
			ctx->data++;
			bytecnt--;
		}
		if(bytecnt > 0) {
			tmp8 = *ctx->data;
			tmp64 = tmp8;
			pkglen = (tmp64 << 12) | pkglen;
			ctx->data++;
			bytecnt--;
		}
		if(bytecnt > 0) {
			tmp8 = *ctx->data;
			tmp64 = tmp8;
			pkglen = (tmp64 << 20) | pkglen;
			ctx->data++;
			bytecnt--;
		}
	}

	ctx->remaining -= usedbytes;
	return pkglen - usedbytes;
}

uint64_t acpi_aml_len_namestring(acpi_aml_parser_context_t* ctx){
	uint64_t res = 0;
	uint8_t* local_data = ctx->data;

	while(acpi_aml_is_root_char(local_data) == 0 || acpi_aml_is_parent_prefix_char(local_data) == 0) {
		res++;
		local_data++;
	}

	if(*local_data == ACPI_AML_DUAL_PREFIX) {
		return res + 8;
	}else if(*local_data == ACPI_AML_MULTI_PREFIX) {
		local_data++;
		return res + 4 * (*local_data);
	} else if(*local_data == ACPI_AML_ZERO) {
		return res;
	}

	return res + 4;
}

acpi_aml_object_t* acpi_aml_symbol_lookup_at_table(acpi_aml_parser_context_t* ctx, linkedlist_t table, char_t* prefix, char_t* symbol_name){
	char_t* tmp_prefix = memory_malloc_ext(ctx->heap, strlen(prefix) + 1, 0x0);

	if(tmp_prefix == NULL) {
		return NULL;
	}

	strcpy(prefix, tmp_prefix);
	while(1) {
		char_t* nomname = acpi_aml_normalize_name(ctx, tmp_prefix, symbol_name);

		if(nomname == NULL) {
			memory_free_ext(ctx->heap, tmp_prefix);
			return NULL;
		}

		for(int64_t len = linkedlist_size(table) - 1; len >= 0; len--) {
			acpi_aml_object_t* obj = (acpi_aml_object_t*)linkedlist_get_data_at_position(table, len);

			if(strcmp(obj->name, nomname) == 0) {
				memory_free_ext(ctx->heap, nomname);
				memory_free_ext(ctx->heap, tmp_prefix);
				return obj;
			}
		}

		if(strlen(tmp_prefix) == 0) {
			memory_free_ext(ctx->heap, nomname);
			break;
		}

		tmp_prefix[strlen(tmp_prefix) - 4] = NULL;

		memory_free_ext(ctx->heap, nomname);
	}

	memory_free_ext(ctx->heap, tmp_prefix);

	return NULL;
}

acpi_aml_object_t* acpi_aml_symbol_lookup(acpi_aml_parser_context_t* ctx, char_t* symbol_name){
	if(ctx->local_symbols != NULL) {
		acpi_aml_object_t* obj = acpi_aml_symbol_lookup_at_table(ctx, ctx->local_symbols, ctx->scope_prefix, symbol_name);

		if(obj != NULL) {
			return obj;
		}

	}

	return acpi_aml_symbol_lookup_at_table(ctx, ctx->symbols, ctx->scope_prefix, symbol_name);
}

int8_t acpi_aml_add_obj_to_symboltable(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj) {
	acpi_aml_object_t* oldsym = acpi_aml_symbol_lookup(ctx, obj->name);
	if(oldsym != NULL) {
		if(obj->type == ACPI_AML_OT_SCOPE && (
				 oldsym->type == ACPI_AML_OT_SCOPE ||
				 oldsym->type == ACPI_AML_OT_DEVICE ||
				 oldsym->type == ACPI_AML_OT_POWERRES ||
				 oldsym->type == ACPI_AML_OT_PROCESSOR ||
				 oldsym->type == ACPI_AML_OT_THERMALZONE
				 )
		   ) {
			acpi_aml_destroy_object(ctx, obj);
			return 0;
		}
		acpi_aml_destroy_object(ctx, obj);
		return -1;
	}

	if(ctx->local_symbols != NULL) {
		linkedlist_list_insert(ctx->local_symbols, obj);
	}else {
		linkedlist_list_insert(ctx->symbols, obj);
	}

	return 0;
}

uint8_t acpi_aml_get_index_of_extended_code(uint8_t code) {
	if(code >= 0x80) {
		return code - 0x6d;
	}

	if(code >= 0x30) {
		return code - 0x21;
	}

	if(code >= 0x1f) {
		return code - 0x1b;
	}

	if(code >= 0x12) {
		return code - 0x10;
	}

	return code - 0x01;
}

void acpi_aml_destroy_symbol_table(acpi_aml_parser_context_t* ctx, uint8_t local){
	uint64_t item_count = 0;
	iterator_t* iter = NULL;
	linkedlist_t symtbl;

	if(local) {
		symtbl = ctx->local_symbols;
	} else {
		symtbl = ctx->symbols;
	}

	iter = linkedlist_iterator_create(symtbl);

	if(iter == NULL) {
		return;
	}

	while(iter->end_of_iterator(iter) != 0) {
		acpi_aml_object_t* sym = iter->get_item(iter);
		acpi_aml_destroy_object(ctx, sym);
		item_count++;
		iter = iter->next(iter);
	}

	iter->destroy(iter);
	linkedlist_destroy(symtbl);
}

void acpi_aml_destroy_object(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj){
	if(obj == NULL) {
		return;
	}

	iterator_t* iter = NULL;

	switch (obj->type) {
	case ACPI_AML_OT_STRING:
		memory_free_ext(ctx->heap, obj->string);
		break;
	case ACPI_AML_OT_BUFFER:
		memory_free_ext(ctx->heap, obj->buffer.buf);
		break;
	case ACPI_AML_OT_PACKAGE:
		iter = linkedlist_iterator_create(obj->package.elements);

		while(iter->end_of_iterator(iter) != 0) {
			acpi_aml_object_t* obj = iter->get_item(iter);

			if(obj->name == NULL || obj->type == ACPI_AML_OT_RUNTIMEREF) {
				acpi_aml_destroy_object(ctx, obj);
			}

			iter = iter->next(iter);
		}

		iter->destroy(iter);

		linkedlist_destroy(obj->package.elements); // TODO :recurive items may be package too

		if(obj->package.pkglen->name == NULL) {
			memory_free_ext(ctx->heap, obj->package.pkglen);
		}
		break;
	case ACPI_AML_OT_DATAREGION:
		memory_free_ext(ctx->heap, obj->dataregion.signature);
		memory_free_ext(ctx->heap, obj->dataregion.oemid);
		memory_free_ext(ctx->heap, obj->dataregion.oemtableid);
		break;
	case ACPI_AML_OT_OPCODE_EXEC_RETURN:
		if(obj->opcode_exec_return->name == NULL) {
			acpi_aml_destroy_object(ctx, obj->opcode_exec_return);
		}
		break;
	case ACPI_AML_OT_NUMBER:
	case ACPI_AML_OT_EVENT:
	case ACPI_AML_OT_MUTEX:
	case ACPI_AML_OT_OPREGION:
	case ACPI_AML_OT_POWERRES:
	case ACPI_AML_OT_PROCESSOR:
	case ACPI_AML_OT_THERMALZONE:
	case ACPI_AML_OT_EXTERNAL:
	case ACPI_AML_OT_METHOD:
	case ACPI_AML_OT_TIMER:
	case ACPI_AML_OT_SCOPE:
	case ACPI_AML_OT_FIELD:
	case ACPI_AML_OT_BUFFERFIELD:
	case ACPI_AML_OT_DEVICE:
	case ACPI_AML_OT_ALIAS:
	case ACPI_AML_OT_RUNTIMEREF:
		break;
	default:
		printf("ACPIAML: Warning object destroy may be required %li\n", obj->type);
		break;
	}

	memory_free_ext(ctx->heap, obj->name);
	memory_free_ext(ctx->heap, obj);
}

void acpi_aml_print_symbol_table(acpi_aml_parser_context_t* ctx){
	uint64_t item_count = 0;
	iterator_t* iter = linkedlist_iterator_create(ctx->symbols);
	while(iter->end_of_iterator(iter) != 0) {
		acpi_aml_object_t* sym = iter->get_item(iter);
		acpi_aml_print_object(ctx, sym);
		item_count++;
		iter = iter->next(iter);
	}
	iter->destroy(iter);

	printf("totoal syms %i\n", item_count );;
}

void acpi_aml_print_object(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj){

	if(obj == NULL) {
		printf("ACPIAML: FATAL null object\n");
		return;
	}

	if(obj->type == ACPI_AML_OT_OPCODE_EXEC_RETURN) {
		obj = obj->opcode_exec_return;
		if(obj == NULL) {
			printf("ACPIAML: FATAL null object\n");
			return;
		}
	}

	int64_t len = 0;
	int64_t ival = 0;

	printf("object id=%p name=%s type=", obj, obj->name);

	switch (obj->type) {
	case ACPI_AML_OT_NUMBER:
		printf("number value=0x%lx bytecnt=%i\n", obj->number.value, obj->number.bytecnt );
		break;
	case ACPI_AML_OT_STRING:
		printf("string value=%s\n", obj->string );
		break;
	case ACPI_AML_OT_ALIAS:
		printf("alias value=%s\n", obj->alias_target->name );
		break;
	case ACPI_AML_OT_BUFFER:
		printf("buffer buflen=%i\n", obj->buffer.buflen );
		break;
	case ACPI_AML_OT_PACKAGE:
		acpi_aml_read_as_integer(ctx, obj->package.pkglen, &ival);
		printf("pkg pkglen=%i initial pkglen=%i\n", len, linkedlist_size(obj->package.elements) );
		break;
	case ACPI_AML_OT_SCOPE:
		printf("scope\n");
		break;
	case ACPI_AML_OT_DEVICE:
		printf("device\n");
		break;
	case ACPI_AML_OT_POWERRES:
		printf("powerres system_level=%i resource_order=%i\n", obj->powerres.system_level, obj->powerres.resource_order);
		break;
	case ACPI_AML_OT_PROCESSOR:
		printf("processor procid=%i pblk_addr=0x%x pblk_len=%i\n", obj->processor.procid, obj->processor.pblk_addr, obj->processor.pblk_len);
		break;
	case ACPI_AML_OT_THERMALZONE:
		printf("thermalzone\n");
		break;
	case ACPI_AML_OT_MUTEX:
		printf("mutex syncflags=%i\n", obj->mutex_sync_flags);
		break;
	case ACPI_AML_OT_EVENT:
		printf("event\n");
		break;
	case ACPI_AML_OT_DATAREGION:
		printf("dataregion signature=%s oemid=%s oemtableid=%s\n", obj->dataregion.signature, obj->dataregion.oemid, obj->dataregion.oemtableid);
		break;
	case ACPI_AML_OT_OPREGION:
		printf("opregion region_space=0x%x region_offset=0x%lx region_len=0x%lx\n", obj->opregion.region_space, obj->opregion.region_offset, obj->opregion.region_len);
		break;
	case ACPI_AML_OT_METHOD:
		printf("method argcount=%i serflag=%i synclevel=%i termlistlen=%i\n", obj->method.arg_count, obj->method.serflag,
		       obj->method.sync_level, obj->method.termlist_length);
		break;
	case ACPI_AML_OT_EXTERNAL:
		printf("external objecttype=%i argcount=%i\n", obj->external.object_type, obj->external.arg_count);
		break;
	case ACPI_AML_OT_FIELD:
	case ACPI_AML_OT_BUFFERFIELD:
		printf("field related_object=%s offset=0x%lx sizeasbit=%i\n", obj->field.related_object->name, obj->field.offset, obj->field.sizeasbit);
		break;
	case ACPI_AML_OT_LOCAL_OR_ARG:
		printf("%s%i\n", obj->local_or_arg.idx_local_or_arg > 7 ? "ARG" : "LOCAL", obj->local_or_arg.idx_local_or_arg > 7 ? obj->local_or_arg.idx_local_or_arg - 1 : obj->local_or_arg.idx_local_or_arg);
		break;
	case ACPI_AML_OT_REFOF:
		printf("refof ");
		acpi_aml_print_object(ctx, obj->refof_target);
		break;
	default:
		printf("unknown object %li\n", obj->type);
	}
}

acpi_aml_object_t* acpi_aml_duplicate_object(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj){
	acpi_aml_object_t* new_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

	if(new_obj == NULL) {
		return NULL;
	}

	memory_memcopy(obj, new_obj, sizeof(acpi_aml_object_t));

	new_obj->name = strdup_at_heap(ctx->heap, obj->name);

	switch (obj->type) {
	case ACPI_AML_OT_STRING:
		new_obj->string = strdup_at_heap(ctx->heap, obj->string);
		break;
	case ACPI_AML_OT_BUFFER:
		new_obj->buffer.buf = memory_malloc_ext(ctx->heap, obj->buffer.buflen, 0x0);
		memory_memcopy(obj->buffer.buf, new_obj->buffer.buf, obj->buffer.buflen);
		break;
	case ACPI_AML_OT_NUMBER:
	case ACPI_AML_OT_EVENT:
	case ACPI_AML_OT_MUTEX:
	case ACPI_AML_OT_OPREGION:
	case ACPI_AML_OT_POWERRES:
	case ACPI_AML_OT_PROCESSOR:
	case ACPI_AML_OT_EXTERNAL:
	case ACPI_AML_OT_METHOD:
	case ACPI_AML_OT_TIMER:
	case ACPI_AML_OT_LOCAL_OR_ARG:
		break;
	case ACPI_AML_OT_REFOF:
		printf("ACPIAML: Warning duplicate of refof\n");
		break;
	default: // TODO: other pointers
		printf("ACPIAML: Fatal not implemented\n");
		break;
	}

	return new_obj;
}

acpi_aml_object_t* acpi_aml_get_real_object(acpi_aml_parser_context_t* ctx, acpi_aml_object_t* obj) {
	while(obj && (obj->type == ACPI_AML_OT_ALIAS || obj->type == ACPI_AML_OT_RUNTIMEREF || obj->type == ACPI_AML_OT_OPCODE_EXEC_RETURN)) {
		if(obj->type == ACPI_AML_OT_ALIAS) {
			if(obj->alias_target == NULL) {
				return NULL;
			}

			obj = obj->alias_target;
		} else if(obj->type == ACPI_AML_OT_RUNTIMEREF) {
			obj = acpi_aml_symbol_lookup(ctx, obj->name);
		} else if(obj->type == ACPI_AML_OT_OPCODE_EXEC_RETURN) {
			obj = obj->opcode_exec_return;
		}
	}

	return obj;
}
