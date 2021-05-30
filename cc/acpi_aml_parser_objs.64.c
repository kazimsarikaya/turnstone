/**
 * @file acpi_aml_parser_objs.64.c
 * @brief acpi aml object parser methods
 */

#include <acpi/aml.h>
#include <strings.h>


int8_t acpi_aml_parse_namestring(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	char_t* name = (char_t*)*data;
	uint64_t idx = 0;
	uint64_t t_consumed = 0;

	while(acpi_aml_is_root_char(ctx->data) == 0 || acpi_aml_is_parent_prefix_char(ctx->data) == 0) {
		name[idx++] = *ctx->data;

		ctx->data++;
		ctx->remaining--;
		t_consumed++;
	}

	if(*ctx->data == ACPI_AML_DUAL_PREFIX) {
		ctx->data++;
		ctx->remaining--;

		memory_memcopy(ctx->data, name + idx, 8);

		ctx->data += 8;
		ctx->remaining -= 8;
		t_consumed += 9;
	}else if(*ctx->data == ACPI_AML_MULTI_PREFIX) {
		ctx->data++;
		ctx->remaining--;

		uint8_t size = *ctx->data;
		size *= 4;

		ctx->data++;
		ctx->remaining--;

		memory_memcopy(ctx->data, name + idx, size);

		ctx->data += size;
		ctx->remaining -= size;
		t_consumed += 2 + size;

	} else if(*ctx->data == ACPI_AML_ZERO) {
		ctx->data++;
		ctx->remaining--;
		t_consumed++;
	} else {
		memory_memcopy(ctx->data, name + idx, 4);

		ctx->data += 4;
		ctx->remaining -= 4;
		t_consumed += 4;
	}

	if(consumed != NULL) {
		*consumed = t_consumed;
	}

	return 0;
}

int8_t acpi_aml_parse_const_data(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	acpi_aml_object_t* obj = (acpi_aml_object_t*)*data;
	uint8_t op_code = *ctx->data;
	uint64_t len;
	uint64_t t_consumed = 0;
	acpi_aml_object_t* _rev;

	ctx->data++;
	ctx->remaining--;
	t_consumed++;

	switch (op_code) {
	case ACPI_AML_ZERO:
		obj->type = ACPI_AML_OT_NUMBER;
		obj->number = 0;
		break;
	case ACPI_AML_ONE:
		obj->type = ACPI_AML_OT_NUMBER;
		obj->number = 1;
		break;
	case ACPI_AML_ONES:
		obj->type = ACPI_AML_OT_NUMBER;

		_rev = acpi_aml_symbol_lookup(ctx, "_REV");
		if(_rev->number >= 0x02) {
			obj->number = 0xFFFFFFFFFFFFFFFF;
		} else {
			obj->number = 0xFFFFFFFF;
		}

		break;
	case ACPI_AML_BYTE_PREFIX:
		obj->type = ACPI_AML_OT_NUMBER;
		obj->number = *ctx->data;

		ctx->data++;
		ctx->remaining--;
		t_consumed++;
		break;
	case ACPI_AML_WORD_PREFIX:
		obj->type = ACPI_AML_OT_NUMBER;
		obj->number = *((uint16_t*)(ctx->data));

		ctx->data += 2;
		ctx->remaining -= 2;
		t_consumed += 2;
		break;
	case ACPI_AML_DWORD_PREFIX:
		obj->type = ACPI_AML_OT_NUMBER;
		obj->number = *((uint32_t*)(ctx->data));

		ctx->data += 4;
		ctx->remaining -= 4;
		t_consumed += 4;
		break;
	case ACPI_AML_QWORD_PREFIX:
		obj->type = ACPI_AML_OT_NUMBER;
		obj->number = *((uint64_t*)(ctx->data));

		ctx->data += 8;
		ctx->remaining -= 8;
		t_consumed += 8;
		break;
	case ACPI_AML_STRING_PREFIX:
		len = strlen((char_t*)ctx->data);
		char_t* str = memory_malloc(sizeof(char_t) * len + 1);
		strcpy((char_t*)ctx->data, str);

		obj->type = ACPI_AML_OT_STRING;
		obj->string = str;
		ctx->data += len + 1;
		ctx->remaining -= len + 1;
		t_consumed += len + 1;
		break;

	default:
		return -1;
	}

	if(consumed != NULL) {
		*consumed = t_consumed;
	}

	return 0;
}

int8_t acpi_aml_parse_byte_data(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	acpi_aml_object_t* obj = (acpi_aml_object_t*)*data;
	uint64_t t_consumed = 1;

	obj->type = ACPI_AML_OT_NUMBER;
	obj->number = *ctx->data;

	ctx->data++;
	ctx->remaining--;


	if(consumed != NULL) {
		*consumed = t_consumed;
	}

	return 0;
}

int8_t acpi_aml_parse_alias(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(consumed);

	ctx->data++;
	ctx->remaining--;

	uint64_t namelen = acpi_aml_len_namestring(ctx);
	char_t* srcname = memory_malloc(sizeof(char_t) * namelen + 1);

	if(acpi_aml_parse_namestring(ctx, (void**)&srcname, NULL) != 0) {
		memory_free(srcname);
		return -1;
	}

	acpi_aml_object_t* src_obj = acpi_aml_symbol_lookup(ctx, srcname);
	if(src_obj == NULL) {
		memory_free(srcname);
		return -1;
	}
	src_obj->refcount++;


	namelen = acpi_aml_len_namestring(ctx);
	char_t* dstname = memory_malloc(sizeof(char_t) * namelen + 1);

	if(acpi_aml_parse_namestring(ctx, (void**)&dstname, NULL) != 0) {
		memory_free(dstname);
		return -1;
	}


	char_t* dstnomname = acpi_aml_normalize_name(ctx->scope_prefix, dstname);
	memory_free(dstname);

	acpi_aml_object_t* obj = memory_malloc(sizeof(acpi_aml_object_t));

	obj->type = ACPI_AML_OT_ALIAS;
	obj->name = dstnomname;
	obj->alias_target = src_obj;

	acpi_aml_add_obj_to_symboltable(ctx, obj);

	return 0;
}

int8_t acpi_aml_parse_scope(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(consumed);

	uint8_t opcode = *ctx->data;
	ctx->data++;
	ctx->remaining--;

	acpi_aml_object_type_t obj_type;

	switch (opcode) {
	case ACPI_AML_SCOPE:
		obj_type = ACPI_AML_OT_SCOPE;
		break;
	case ACPI_AML_DEVICE:
		obj_type = ACPI_AML_OT_DEVICE;
		break;
	case ACPI_AML_POWERRES:
		obj_type = ACPI_AML_OT_POWERRES;
		break;
	case ACPI_AML_PROCESSOR:
		obj_type = ACPI_AML_OT_PROCESSOR;
		break;
	case ACPI_AML_THERMALZONE:
		obj_type = ACPI_AML_OT_THERMALZONE;
		break;
	default:
		return -1;
	}


	uint64_t pkglen = acpi_aml_parse_package_length(ctx);

	uint64_t namelen = acpi_aml_len_namestring(ctx);

	char_t* name = memory_malloc(sizeof(char_t) * namelen + 1);

	int64_t tmp_start = ctx->remaining;

	if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
		memory_free(name);
		return -1;
	}

	pkglen -= (tmp_start - ctx->remaining);

	char_t* nomname = acpi_aml_normalize_name(ctx->scope_prefix, name);
	memory_free(name);

	acpi_aml_object_t* obj = memory_malloc(sizeof(acpi_aml_object_t));

	obj->type = obj_type;
	obj->name = nomname;


	if(obj_type == ACPI_AML_OT_POWERRES) {
		obj->powerres.system_level = *ctx->data;
		ctx->data++;
		ctx->remaining--;
		obj->powerres.resource_order = *((uint16_t*)ctx->data);
		ctx->data += 2;
		ctx->remaining -= 2;
		pkglen -= 3;
	} else if(obj_type == ACPI_AML_OT_PROCESSOR) {
		obj->processor.procid = *ctx->data;
		ctx->data++;
		ctx->remaining--;
		obj->processor.pblk_addr = *((uint32_t*)ctx->data);
		ctx->data += 4;
		ctx->remaining -= 4;
		obj->processor.pblk_len = *ctx->data;
		ctx->data++;
		ctx->remaining--;
		pkglen -= 6;
	}

	acpi_aml_add_obj_to_symboltable(ctx, obj);

	uint64_t old_length = ctx->length;
	uint64_t old_remaining = ctx->remaining;
	char_t* old_scope_prefix = ctx->scope_prefix;

	ctx->length = pkglen;
	ctx->remaining = pkglen;
	ctx->scope_prefix = nomname;

	if(acpi_aml_parse_all_items(ctx, NULL, NULL) != 0) {
		return -1;
	}

	ctx->length = old_length;
	ctx->remaining = old_remaining - pkglen;
	ctx->scope_prefix = old_scope_prefix;

	return 0;
}



int8_t acpi_aml_parse_buffer(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(consumed);
	acpi_aml_object_t* buf = (acpi_aml_object_t*)*data;
	uint64_t t_consumed = 0;
	uint64_t r_consumed = 1;


	ctx->data++;
	ctx->remaining--;

	r_consumed += ctx->remaining;
	uint64_t plen = acpi_aml_parse_package_length(ctx);
	r_consumed -= ctx->remaining;
	r_consumed += plen;


	acpi_aml_object_t* buflenobj = memory_malloc(sizeof(acpi_aml_object_t));
	if(acpi_aml_parse_one_item(ctx, (void**)&buflenobj, &t_consumed) != 0) {
		memory_free(buflenobj);
		return -1;
	}
	plen -= t_consumed;
	t_consumed = 0;

	int64_t buflen = acpi_aml_cast_as_integer(buflenobj);

	buf->type = ACPI_AML_OT_BUFFER;
	buf->buffer.buflen = buflen;
	buf->buffer.buf = memory_malloc(sizeof(uint8_t) * buflen);

	memory_memcopy(ctx->data, buf->buffer.buf, plen);

	ctx->data += plen;
	ctx->remaining -= plen;

	if(consumed != NULL) {
		*consumed = r_consumed;
	}

	return 0;
}

int8_t acpi_aml_parse_package(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	uint64_t t_consumed = 0;
	uint64_t r_consumed = 1;

	acpi_aml_object_t* pkg = (acpi_aml_object_t*)*data;
	pkg->type = ACPI_AML_OT_PACKAGE;

	ctx->data++;
	ctx->remaining--;

	r_consumed += ctx->remaining;
	uint64_t plen = acpi_aml_parse_package_length(ctx);
	r_consumed -= ctx->remaining;
	r_consumed += plen;

	acpi_aml_object_t* pkglen = memory_malloc(sizeof(acpi_aml_object_t));
	if(acpi_aml_parse_byte_data(ctx, (void**)&pkglen, NULL) != 0) {
		memory_free(pkglen);
		return -1;
	}
	plen--;

	pkg->package.pkglen = pkglen;
	pkg->package.elements = linkedlist_create_list_with_heap(NULL);

	while(plen > 0) {
		t_consumed = 0;
		acpi_aml_object_t* tmp_obj = memory_malloc(sizeof(acpi_aml_object_t));
		if(acpi_aml_parse_one_item(ctx, (void**)&tmp_obj, &t_consumed) != 0) {
			memory_free(tmp_obj);
			return -1;
		}
		linkedlist_list_insert(pkg->package.elements, tmp_obj);
		plen -= t_consumed;
	}


	if(consumed != NULL) {
		*consumed = r_consumed;
	}

	return 0;
}

int8_t acpi_aml_parse_varpackage(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	uint64_t t_consumed = 0;
	uint64_t r_consumed = 1;

	acpi_aml_object_t* pkg = (acpi_aml_object_t*)*data;
	pkg->type = ACPI_AML_OT_PACKAGE;

	ctx->data++;
	ctx->remaining--;

	r_consumed += ctx->remaining;
	uint64_t plen = acpi_aml_parse_package_length(ctx);
	r_consumed -= ctx->remaining;
	r_consumed += plen;

	acpi_aml_object_t* pkglen = memory_malloc(sizeof(acpi_aml_object_t));
	if(acpi_aml_parse_one_item(ctx, (void**)&pkglen, &t_consumed) != 0) {
		memory_free(pkglen);
		return -1;
	}
	plen -= t_consumed;
	t_consumed = 0;

	pkg->package.pkglen = pkglen;
	pkg->package.elements = linkedlist_create_list_with_heap(NULL);

	while(plen > 0) {
		t_consumed = 0;
		acpi_aml_object_t* tmp_obj = memory_malloc(sizeof(acpi_aml_object_t));
		if(acpi_aml_parse_one_item(ctx, (void**)&tmp_obj, &t_consumed) != 0) {
			memory_free(tmp_obj);
			return -1;
		}
		linkedlist_list_insert(pkg->package.elements, tmp_obj);
		plen -= t_consumed;
	}

	t_consumed = 0;
	if(consumed != NULL) {
		*consumed = r_consumed;
	}

	return 0;
}



int8_t acpi_aml_parse_method(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	uint64_t r_consumed = 1;

	ctx->data++;
	ctx->remaining--;

	r_consumed += ctx->remaining;
	uint64_t plen = acpi_aml_parse_package_length(ctx);
	r_consumed -= ctx->remaining;
	r_consumed += plen;

	uint64_t namelen = acpi_aml_len_namestring(ctx);
	char_t* name = memory_malloc(sizeof(char_t) * namelen + 1);

	if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
		memory_free(name);
		return -1;
	}
	r_consumed += namelen;
	plen -= namelen;

	char_t* nomname = acpi_aml_normalize_name(ctx->scope_prefix, name);
	memory_free(name);


	uint8_t flags = *ctx->data;
	ctx->data++;
	ctx->remaining--;
	r_consumed++;
	plen--;


	acpi_aml_object_t* obj = memory_malloc(sizeof(acpi_aml_object_t));

	obj->name = nomname;
	obj->type = ACPI_AML_OT_METHOD;
	obj->method.arg_count = flags & 0x03;
	obj->method.serflag = flags & 0x04;
	obj->method.sync_level = flags >> 4;
	obj->method.termlist_length = plen;
	obj->method.termlist = ctx->data;

	acpi_aml_add_obj_to_symboltable(ctx, obj);

	ctx->data += plen;
	ctx->remaining -= plen;
	r_consumed += plen;

	if(consumed != NULL) {
		*consumed = r_consumed;
	}

	return 0;
}

int8_t acpi_aml_parse_external(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	uint64_t t_consumed = 0;

	ctx->data++;
	ctx->remaining--;
	t_consumed++;

	uint64_t namelen = acpi_aml_len_namestring(ctx);
	char_t* name = memory_malloc(sizeof(char_t) * namelen + 1);

	if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
		memory_free(name);
		return -1;
	}
	t_consumed += namelen;

	char_t* nomname = acpi_aml_normalize_name(ctx->scope_prefix, name);
	memory_free(name);


	acpi_aml_object_t* obj = memory_malloc(sizeof(acpi_aml_object_t));

	obj->name = nomname;
	obj->type = ACPI_AML_OT_EXTERNAL;

	uint8_t flags;

	flags = *ctx->data;
	ctx->data++;
	ctx->remaining--;
	t_consumed++;
	obj->external.object_type = flags;

	flags = *ctx->data;
	ctx->data++;
	ctx->remaining--;
	t_consumed++;
	obj->external.arg_count = flags;

	acpi_aml_add_obj_to_symboltable(ctx, obj);

	if(consumed != NULL) {
		*consumed = t_consumed;
	}

	return 0;
}

int8_t acpi_aml_parse_mutex(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	uint64_t t_consumed = 0;

	ctx->data++;
	ctx->remaining--;
	t_consumed++;

	uint64_t namelen = acpi_aml_len_namestring(ctx);
	char_t* name = memory_malloc(sizeof(char_t) * namelen + 1);

	if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
		memory_free(name);
		return -1;
	}
	t_consumed += namelen;

	char_t* nomname = acpi_aml_normalize_name(ctx->scope_prefix, name);
	memory_free(name);

	uint8_t flags = *ctx->data;
	ctx->data++;
	ctx->remaining--;
	t_consumed++;


	acpi_aml_object_t* obj = memory_malloc(sizeof(acpi_aml_object_t));

	obj->name = nomname;
	obj->type = ACPI_AML_OT_MUTEX;
	obj->mutex_sync_flags = flags;

	acpi_aml_add_obj_to_symboltable(ctx, obj);

	if(consumed != NULL) {
		*consumed = t_consumed;
	}

	return 0;
}

int8_t acpi_aml_parse_event(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(consumed);

	ctx->data++;
	ctx->remaining--;

	uint64_t namelen = acpi_aml_len_namestring(ctx);
	char_t* name = memory_malloc(sizeof(char_t) * namelen + 1);

	if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
		memory_free(name);
		return -1;
	}

	char_t* nomname = acpi_aml_normalize_name(ctx->scope_prefix, name);
	memory_free(name);

	acpi_aml_object_t* obj = memory_malloc(sizeof(acpi_aml_object_t));

	obj->name = nomname;
	obj->type = ACPI_AML_OT_EVENT;

	acpi_aml_add_obj_to_symboltable(ctx, obj);

	return 0;
}

int8_t acpi_aml_parse_region(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(consumed);

	uint8_t opcode = *ctx->data;
	ctx->data++;
	ctx->remaining--;

	acpi_aml_object_type_t obj_type;

	switch (opcode) {
	case ACPI_AML_DATAREGION:
		obj_type = ACPI_AML_OT_DATAREGION;
		break;
	case ACPI_AML_OPREGION:
		obj_type = ACPI_AML_OT_OPREGION;
		break;
	default:
		return -1;
	}

	uint64_t namelen = acpi_aml_len_namestring(ctx);
	char_t* name = memory_malloc(sizeof(char_t) * namelen + 1);

	if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
		memory_free(name);
		return -1;
	}

	char_t* nomname = acpi_aml_normalize_name(ctx->scope_prefix, name);
	memory_free(name);

	acpi_aml_object_t* obj = memory_malloc(sizeof(acpi_aml_object_t));

	obj->name = nomname;
	obj->type = obj_type;

	acpi_aml_object_t* tmp_obj;

	if(obj_type == ACPI_AML_OT_DATAREGION) {
		tmp_obj = memory_malloc(sizeof(acpi_aml_object_t));
		if(acpi_aml_parse_one_item(ctx, (void**)&tmp_obj, NULL) != 0) {
			memory_free(tmp_obj);
			return -1;
		}

		obj->dataregion.signature = tmp_obj->string;
		memory_free(tmp_obj);

		tmp_obj = memory_malloc(sizeof(acpi_aml_object_t));
		if(acpi_aml_parse_one_item(ctx, (void**)&tmp_obj, NULL) !=  0) {
			memory_free(tmp_obj);
			return -1;
		}

		obj->dataregion.oemid = tmp_obj->string;
		memory_free(tmp_obj);

		tmp_obj = memory_malloc(sizeof(acpi_aml_object_t));
		if(acpi_aml_parse_one_item(ctx, (void**)&tmp_obj, NULL) !=  0) {
			memory_free(tmp_obj);
			return -1;
		}

		obj->dataregion.oemtableid = tmp_obj->string;
		memory_free(tmp_obj);
	} else {
		tmp_obj = memory_malloc(sizeof(acpi_aml_object_t));
		if(acpi_aml_parse_byte_data(ctx, (void**)&tmp_obj, NULL) !=  0) {
			memory_free(tmp_obj);
			return -1;
		}

		obj->opregion.region_space = tmp_obj->number;
		memory_free(tmp_obj);

		tmp_obj = memory_malloc(sizeof(acpi_aml_object_t));
		if(acpi_aml_parse_one_item(ctx, (void**)&tmp_obj, NULL) !=  0) {
			memory_free(tmp_obj);
			return -1;
		}

		obj->opregion.region_offset = acpi_aml_cast_as_integer(tmp_obj);
		memory_free(tmp_obj);

		tmp_obj = memory_malloc(sizeof(acpi_aml_object_t));
		if(acpi_aml_parse_one_item(ctx, (void**)&tmp_obj, NULL) !=  0) {
			memory_free(tmp_obj);
			return -1;
		}

		obj->opregion.region_len = acpi_aml_cast_as_integer(tmp_obj);
		memory_free(tmp_obj);
	}

	acpi_aml_add_obj_to_symboltable(ctx, obj);

	return 0;
}


#ifndef ___TESTMODE
UNIMPLPARSER(name);
UNIMPLPARSER(one_item);
#endif

UNIMPLPARSER(field);
UNIMPLPARSER(indexfield);
UNIMPLPARSER(bankfield);
UNIMPLPARSER(fatal);
