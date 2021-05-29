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
		obj->number = 0xFF;
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


	linkedlist_list_insert(ctx->symbols, obj);

	return 0;
}

int8_t acpi_aml_parse_scope(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(consumed);

	ctx->data++;
	ctx->remaining--;

	uint64_t pkglen = acpi_aml_parse_package_length(ctx);

	uint64_t namelen = acpi_aml_len_namestring(ctx);

	pkglen -= namelen;

	char_t* name = memory_malloc(sizeof(char_t) * namelen + 1);

	if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
		memory_free(name);
		return -1;
	}

	char_t* nomname = acpi_aml_normalize_name(ctx->scope_prefix, name);
	memory_free(name);

	uint64_t old_length = ctx->length;
	uint64_t old_remaining = ctx->remaining;
	char_t* old_scope_prefix = ctx->scope_prefix;

	ctx->length = pkglen;
	ctx->remaining = pkglen;
	ctx->scope_prefix = nomname;

	if(acpi_aml_parse_all_items(ctx, NULL, NULL) != 0) {
		memory_free(nomname);
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

#ifndef ___TESTMODE

int8_t acpi_aml_parse_external(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(ctx);
	UNUSED(consumed);
	return -1;
}
int8_t acpi_aml_parse_name(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(ctx);
	UNUSED(consumed);
	return -1;
}
int8_t acpi_aml_parse_method(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(ctx);
	UNUSED(consumed);
	return -1;
}
int8_t acpi_aml_parse_one_item(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(ctx);
	UNUSED(consumed);
	return -1;
}
int8_t acpi_aml_parse_op_extended(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	UNUSED(data);
	UNUSED(ctx);
	UNUSED(consumed);
	return -1;
}
#endif