/**
 * @file acpi_aml_parser.64.c
 * @brief acpi parser methos
 */

 #include <acpi/aml_internal.h>
 #include <video.h>
 #include <strings.h>



typedef int8_t (* acpi_aml_parse_f)(acpi_aml_parser_context_t* ctx, void**, uint64_t*);

#define CREATE_PARSER_F(name) int8_t acpi_aml_parse_ ## name(acpi_aml_parser_context_t*, void**, uint64_t*);

#define PARSER_F_NAME(name) acpi_aml_parse_ ## name

// parser methods

CREATE_PARSER_F(namestring);

CREATE_PARSER_F(alias);
CREATE_PARSER_F(name);
CREATE_PARSER_F(scope);
CREATE_PARSER_F(const_data);

CREATE_PARSER_F(opcnt_0);
CREATE_PARSER_F(opcnt_1);
CREATE_PARSER_F(opcnt_2);
CREATE_PARSER_F(opcnt_3);
CREATE_PARSER_F(opcnt_4);

CREATE_PARSER_F(op_match);
CREATE_PARSER_F(logic_ext);

CREATE_PARSER_F(op_if);
CREATE_PARSER_F(op_else);
CREATE_PARSER_F(op_while);

CREATE_PARSER_F(create_field);

CREATE_PARSER_F(op_extended);

CREATE_PARSER_F(buffer);
CREATE_PARSER_F(package);
CREATE_PARSER_F(varpackage);
CREATE_PARSER_F(method);
CREATE_PARSER_F(external);

CREATE_PARSER_F(symbol);

CREATE_PARSER_F(byte_data);

CREATE_PARSER_F(mutex);
CREATE_PARSER_F(event);

CREATE_PARSER_F(region);

CREATE_PARSER_F(field);

CREATE_PARSER_F(fatal);

CREATE_PARSER_F(extopcnt_0);
CREATE_PARSER_F(extopcnt_1);
CREATE_PARSER_F(extopcnt_2);
CREATE_PARSER_F(extopcnt_6);

acpi_aml_parse_f acpi_aml_parse_fs[] = {
	PARSER_F_NAME(const_data),
	PARSER_F_NAME(const_data),
	NULL, // empty
	NULL, // empty
	NULL, // empty
	NULL, // empty
	PARSER_F_NAME(alias),
	NULL, // empty
	PARSER_F_NAME(name), // 0x08
	NULL, // empty
	PARSER_F_NAME(const_data),
	PARSER_F_NAME(const_data),
	PARSER_F_NAME(const_data),
	PARSER_F_NAME(const_data), // 0x0D
	PARSER_F_NAME(const_data),
	NULL, // empty
	PARSER_F_NAME(scope), // 0x10
	PARSER_F_NAME(buffer),
	PARSER_F_NAME(package),
	PARSER_F_NAME(varpackage),
	PARSER_F_NAME(method),
	PARSER_F_NAME(external),
	NULL, NULL, NULL, /*0x18*/ NULL, NULL, NULL, NULL, NULL, NULL, NULL, // empty
	NULL,  // 0x20 // empty
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /*0x28*/ NULL, NULL, NULL, NULL, NULL, NULL, NULL, // empty
	NULL,  // 0x30 // empty
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /*0x38*/ NULL, NULL, NULL, NULL, NULL, NULL, NULL, // empty
	NULL,  // 0x40 // empty
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /*0x48*/ NULL, NULL, NULL, NULL, NULL, NULL, NULL, // empty
	NULL,  // 0x50 // empty
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /*0x58*/ NULL, NULL, // empty
	PARSER_F_NAME(op_extended),
	NULL, NULL, NULL, NULL, // empty
	PARSER_F_NAME(opcnt_0),  // 0x60
	PARSER_F_NAME(opcnt_0),
	PARSER_F_NAME(opcnt_0),
	PARSER_F_NAME(opcnt_0),
	PARSER_F_NAME(opcnt_0),
	PARSER_F_NAME(opcnt_0),
	PARSER_F_NAME(opcnt_0),
	PARSER_F_NAME(opcnt_0),
	PARSER_F_NAME(opcnt_0), /*0x68*/
	PARSER_F_NAME(opcnt_0),
	PARSER_F_NAME(opcnt_0),
	PARSER_F_NAME(opcnt_0),
	PARSER_F_NAME(opcnt_0),
	PARSER_F_NAME(opcnt_0),
	PARSER_F_NAME(opcnt_0),
	NULL, // empty
	PARSER_F_NAME(opcnt_2),  // 0x70
	PARSER_F_NAME(opcnt_1),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_1),
	PARSER_F_NAME(opcnt_1),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_4), /*0x78*/
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_2),  // 0x80
	PARSER_F_NAME(opcnt_2),
	PARSER_F_NAME(opcnt_2),
	PARSER_F_NAME(opcnt_1),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_2),
	PARSER_F_NAME(opcnt_1),
	PARSER_F_NAME(opcnt_3), /*0x88*/
	PARSER_F_NAME(op_match),
	PARSER_F_NAME(create_field),
	PARSER_F_NAME(create_field),
	PARSER_F_NAME(create_field),
	PARSER_F_NAME(create_field),
	PARSER_F_NAME(opcnt_1),
	PARSER_F_NAME(create_field),
	PARSER_F_NAME(opcnt_2),  // 0x90
	PARSER_F_NAME(opcnt_2),
	PARSER_F_NAME(logic_ext),
	PARSER_F_NAME(opcnt_2),
	PARSER_F_NAME(opcnt_2),
	PARSER_F_NAME(opcnt_2),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_2),
	PARSER_F_NAME(opcnt_2), /*0x98*/
	PARSER_F_NAME(opcnt_2),
	NULL, NULL, // empty opts
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_2),
	PARSER_F_NAME(opcnt_4),
	PARSER_F_NAME(opcnt_0),
	PARSER_F_NAME(op_if),  // 0xA0
	PARSER_F_NAME(op_else),
	PARSER_F_NAME(op_while),
	PARSER_F_NAME(opcnt_0),
	PARSER_F_NAME(opcnt_1),
	PARSER_F_NAME(opcnt_0),
	NULL, NULL, NULL, /*0xA8*/ NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL,  // 0xB0
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /*0xB8*/ NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL,  // 0xC0
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /*0xC8*/ NULL, NULL, NULL,
	PARSER_F_NAME(opcnt_0), // 0xCC
	NULL, NULL, NULL,
	NULL,  // 0xD0
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /*0xD8*/ NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL,  // 0xE0
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /*0xE8*/ NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL,  // 0xF0
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /*0xE8*/ NULL, NULL, NULL, NULL, NULL, NULL,
	PARSER_F_NAME(const_data)
};

int8_t acpi_aml_parse_all_items(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	while(ctx->remaining > 0 && ctx->flags.method_return != 1 && ctx->flags.fatal != 1) {
		if(acpi_aml_parse_one_item(ctx, data, consumed) != 0) {
			return -1;
		}
	}

	if(ctx->flags.fatal == 1) {
		return -1;
	}

	return 0;
}

int8_t acpi_aml_parse_one_item(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	if(ctx->remaining == 0) { // realy we need this?
		PRINTLOG(ACPIAML, LOG_ERROR, "premature end", 0);
		ctx->flags.fatal = 1;
		return -1;
	}

	int8_t res = -1;

	if (*ctx->data != NULL && acpi_aml_is_namestring_start(ctx->data) == 0) {
		res = acpi_aml_parse_symbol(ctx, data, consumed);
	} else {
		acpi_aml_parse_f parser = acpi_aml_parse_fs[*ctx->data];

		if( parser == NULL ) {
			PRINTLOG(ACPIAML, LOG_ERROR, "null parser %02x %02x %02x %02x", *ctx->data, *(ctx->data + 1), *(ctx->data + 2), *(ctx->data + 3));

			res = -1;
		}else {
			res = parser(ctx, data, consumed);
		}
	}

	if(res == -1 && ctx->flags.fatal == 1) {
		PRINTLOG(ACPIAML, LOG_ERROR, "scope: -%s- one_item data: 0x%02x length: %li remaining: %li", ctx->scope_prefix, *ctx->data, ctx->length, ctx->remaining);
		return -1;
	}

	return res;
}

int8_t acpi_aml_parse_symbol(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	uint64_t t_consumed = 0;
	uint64_t r_consumed = 0;

	uint64_t namelen = acpi_aml_len_namestring(ctx);
	char_t* name = memory_malloc_ext(ctx->heap, sizeof(char_t) * namelen + 1, 0x0);

	if(name == NULL) {
		return -1;
	}

	int64_t tmp_start = ctx->remaining;
	if(acpi_aml_parse_namestring(ctx, (void**)&name, NULL) != 0) {
		memory_free_ext(ctx->heap, name);
		return -1;
	}
	r_consumed += (tmp_start - ctx->remaining);


	acpi_aml_object_t* tmp_obj = acpi_aml_symbol_lookup(ctx, name);

	if(tmp_obj == NULL) {
		tmp_obj = memory_malloc_ext(ctx->heap, sizeof(acpi_aml_object_t), 0x0);

		if(tmp_obj == NULL) {
			return -1;
		}

		char_t* nomname = acpi_aml_normalize_name(ctx, ctx->scope_prefix, name);
		tmp_obj->name = nomname;
		tmp_obj->type = ACPI_AML_OT_RUNTIMEREF;
	}

	memory_free_ext(ctx->heap, name);

	if(tmp_obj->type == ACPI_AML_OT_METHOD && ctx->flags.dismiss_execute_method == 0) { // TODO: external if it is method
		t_consumed = 0;

		if(acpi_aml_parse_op_code_with_cnt(ACPI_AML_METHODCALL, tmp_obj->method.arg_count, ctx, data, &t_consumed, tmp_obj) != 0) {
			return -1;
		}

		r_consumed += t_consumed;
	} else {
		if(data != NULL) {
			if(*data != NULL) {
				memory_free_ext(ctx->heap, *data);
			}
			*data = (void*)tmp_obj;
		} else {
			return -1;
		}
	}

	if(consumed != NULL) {
		*consumed += r_consumed;
	}

	return 0;
}

acpi_aml_parse_f acpi_aml_parse_extfs[] = {
	PARSER_F_NAME(mutex), // 0x01 -> 0
	PARSER_F_NAME(event),
	PARSER_F_NAME(extopcnt_2), //0x12 -> 2
	PARSER_F_NAME(create_field),
	PARSER_F_NAME(extopcnt_6), //0x1F -> 4
	PARSER_F_NAME(extopcnt_2),
	PARSER_F_NAME(extopcnt_1),
	PARSER_F_NAME(extopcnt_1),
	PARSER_F_NAME(extopcnt_2),
	PARSER_F_NAME(extopcnt_1),
	PARSER_F_NAME(extopcnt_2),
	PARSER_F_NAME(extopcnt_1),
	PARSER_F_NAME(extopcnt_1),
	PARSER_F_NAME(extopcnt_2),
	PARSER_F_NAME(extopcnt_2),
	PARSER_F_NAME(extopcnt_0), // 0x30 -> 15
	PARSER_F_NAME(extopcnt_0),
	PARSER_F_NAME(fatal),
	PARSER_F_NAME(extopcnt_0),
	PARSER_F_NAME(region), // 0x80 -> 19
	PARSER_F_NAME(field),
	PARSER_F_NAME(scope),
	PARSER_F_NAME(scope),
	PARSER_F_NAME(scope),
	PARSER_F_NAME(scope),
	PARSER_F_NAME(field),
	PARSER_F_NAME(field),
	PARSER_F_NAME(region),
};


int8_t acpi_aml_parse_op_extended(acpi_aml_parser_context_t* ctx, void** data, uint64_t* consumed){
	uint64_t t_consumed = 0;
	uint64_t r_consumed = 1;

	ctx->data++;
	ctx->remaining--;

	uint8_t ext_opcode = *ctx->data;

	uint8_t idx = acpi_aml_get_index_of_extended_code(ext_opcode);

	acpi_aml_parse_f parser = acpi_aml_parse_extfs[idx];
	if(parser(ctx, data, &t_consumed) != 0) {
		return -1;
	}

	r_consumed += t_consumed;

	if(consumed != NULL) {
		*consumed = r_consumed;
	}

	return 0;
}

uint8_t acpi_aml_parser_defaults[] =
{
	0x10, 0x05, 0x5F, 0x47, 0x50, 0x45, // _GPE
	0x10, 0x05, 0x5F, 0x50, 0x52, 0x5F,  // _PR_
	0x10, 0x05, 0x5F, 0x53, 0x42, 0x5F,   // _SB_
	0x10, 0x05, 0x5F, 0x53, 0x49, 0x5F,   // _SI_
	0x10, 0x05, 0x5F, 0x54, 0x5A, 0x5F,   // _TZ_
	0x5B, 0x01, 0x5F, 0x47, 0x4C, 0x5F, 0x00,   // _GL_
	0x08, 0x5F, 0x4F, 0x53, 0x5F, 0x0D, 0x48, 0x6F, 0x62, 0x62, 0x79, 0x20, 0x4F, 0x53, 0x00,   // _OS_ -> Hobby OS
	0x14, 0x08, 0x5F, 0x4F, 0x53, 0x49, 0x01, 0xA4, 0x00,   // _OSI
	0x08, 0x5F, 0x52, 0x45, 0x56, 0x0A, 0x00   // _REV
};

int8_t acpi_aml_object_name_comparator(const void* data1, const void* data2) {
	acpi_aml_object_t* obj1 = (acpi_aml_object_t*)data1;
	acpi_aml_object_t* obj2 = (acpi_aml_object_t*)data2;

	return strcmp(obj1->name, obj2->name);
}

int8_t acpi_aml_device_name_comparator(const void* data1, const void* data2) {
	acpi_aml_device_t* obj1 = (acpi_aml_device_t*)data1;
	acpi_aml_device_t* obj2 = (acpi_aml_device_t*)data2;

	return strcmp(obj1->name, obj2->name);
}

acpi_aml_parser_context_t* acpi_aml_parser_context_create_with_heap(memory_heap_t* heap, uint8_t revision, uint8_t* aml, int64_t size) {
	acpi_aml_parser_context_t* ctx = memory_malloc_ext(heap, sizeof(acpi_aml_parser_context_t), 0x0);

	if(ctx == NULL) {
		return NULL;
	}

	acpi_aml_parser_defaults[sizeof(acpi_aml_parser_defaults) - 1] = revision;

	ctx->heap = heap;
	ctx->data = acpi_aml_parser_defaults;
	ctx->length = sizeof(acpi_aml_parser_defaults);
	ctx->remaining = sizeof(acpi_aml_parser_defaults);
	ctx->scope_prefix = "\\";
	ctx->symbols = linkedlist_create_sortedlist_with_heap(heap, acpi_aml_object_name_comparator);
	ctx->revision = revision;

	if(acpi_aml_parse_all_items(ctx, NULL, NULL) != 0) {
		memory_free_ext(heap, ctx);
		return NULL;
	}

	ctx->data = aml;
	ctx->length = size;
	ctx->remaining = size;

	return ctx;
}

void acpi_aml_parser_context_destroy(acpi_aml_parser_context_t* ctx) {
	iterator_t* iter = linkedlist_iterator_create(ctx->devices);

	while(iter->end_of_iterator(iter) != 0) {
		acpi_aml_device_t* d = iter->get_item(iter);
		memory_free_ext(ctx->heap, d);

		iter = iter->next(iter);
	}

	iter->destroy(iter);

	linkedlist_destroy(ctx->devices);

	acpi_aml_destroy_symbol_table(ctx, 0);
	memory_free_ext(ctx->heap, ctx);
}


int8_t acpi_aml_parse(acpi_aml_parser_context_t* ctx) {
	if(ctx == NULL) {
		return -1;
	}

	uint8_t res = -1;

	res = acpi_aml_parse_all_items(ctx, NULL, NULL);

	return res;
}
