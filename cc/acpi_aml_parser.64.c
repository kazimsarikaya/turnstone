/**
 * @file acpi_aml_parser.64.c
 * @brief acpi parser methos
 */

 #include <acpi/aml.h>

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
	PARSER_F_NAME(opcnt_2),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_2),
	PARSER_F_NAME(opcnt_2),
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
	PARSER_F_NAME(opcnt_2),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_3),
	PARSER_F_NAME(opcnt_2),
	PARSER_F_NAME(opcnt_2),
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
	while(ctx->remaining > 0) {
		if(acpi_aml_parse_one_item(ctx, data, consumed) != 0) {
			return -1;
		}
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
	0x10, 0x05, 0x5F, 0x47, 0x50, 0x45,
	0x10, 0x05, 0x5F, 0x50, 0x52, 0x5F,
	0x10, 0x05, 0x5F, 0x53, 0x42, 0x5F,
	0x10, 0x05, 0x5F, 0x53, 0x49, 0x5F,
	0x10, 0x05, 0x5F, 0x54, 0x5A, 0x5F,
	0x5B, 0x01, 0x5F, 0x47, 0x4C, 0x5F, 0x00,
	0x08, 0x5F, 0x4F, 0x53, 0x5F, 0x0D, 0x48, 0x6F, 0x62, 0x62, 0x79, 0x20, 0x4F, 0x53, 0x00,
	0x14, 0x08, 0x5F, 0x4F, 0x53, 0x49, 0x01, 0xA4, 0x00,
	0x08, 0x5F, 0x52, 0x45, 0x56, 0x0A, 0x02
};

acpi_aml_parser_context_t* acp_aml_parser_context_create(uint8_t* aml, int64_t size) {
	char_t* root_prefix = "";

	acpi_aml_parser_context_t* ctx = memory_malloc(sizeof(acpi_aml_parser_context_t));
	ctx->data = acpi_aml_parser_defaults;
	ctx->length = sizeof(acpi_aml_parser_defaults);
	ctx->remaining = sizeof(acpi_aml_parser_defaults);
	ctx->scope_prefix = root_prefix;
	ctx->symbols = linkedlist_create_list_with_heap(NULL);

	if(acpi_aml_parse_all_items(ctx, NULL, NULL) != 0) {
		memory_free(ctx);
		return NULL;
	}

	ctx->data = aml;
	ctx->length = size;
	ctx->remaining = size;

	return ctx;
}
