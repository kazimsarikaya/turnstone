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
	PARSER_F_NAME(op_local_arg),  // 0x60
	PARSER_F_NAME(op_local_arg),
	PARSER_F_NAME(op_local_arg),
	PARSER_F_NAME(op_local_arg),
	PARSER_F_NAME(op_local_arg),
	PARSER_F_NAME(op_local_arg),
	PARSER_F_NAME(op_local_arg),
	PARSER_F_NAME(op_local_arg),
	PARSER_F_NAME(op_local_arg), /*0x68*/
	PARSER_F_NAME(op_local_arg),
	PARSER_F_NAME(op_local_arg),
	PARSER_F_NAME(op_local_arg),
	PARSER_F_NAME(op_local_arg),
	PARSER_F_NAME(op_local_arg),
	PARSER_F_NAME(op_local_arg),
	NULL, // empty
	PARSER_F_NAME(op1_tgt1),  // 0x70
	PARSER_F_NAME(op2_tgt0),
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op2_tgt0),
	PARSER_F_NAME(op2_tgt0),
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op2_tgt2), /*0x78*/
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op1_tgt1),  // 0x80
	PARSER_F_NAME(op1_tgt1),
	PARSER_F_NAME(op1_tgt1),
	PARSER_F_NAME(op2_tgt0),
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op1_tgt1),
	PARSER_F_NAME(op2_tgt0),
	PARSER_F_NAME(op2_tgt1), /*0x88*/
	PARSER_F_NAME(op_match),
	PARSER_F_NAME(create_field),
	PARSER_F_NAME(create_field),
	PARSER_F_NAME(create_field),
	PARSER_F_NAME(create_field),
	PARSER_F_NAME(op1_tgt0),
	PARSER_F_NAME(create_field),
	PARSER_F_NAME(op2_tgt0),  // 0x90
	PARSER_F_NAME(op2_tgt0),
	PARSER_F_NAME(logic_ext),
	PARSER_F_NAME(op2_tgt0),
	PARSER_F_NAME(op2_tgt0),
	PARSER_F_NAME(op2_tgt0),
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op1_tgt1),
	PARSER_F_NAME(op1_tgt1), /*0x98*/
	PARSER_F_NAME(op1_tgt1),
	NULL, NULL, // empty opts
	PARSER_F_NAME(op2_tgt1),
	PARSER_F_NAME(op1_tgt1),
	PARSER_F_NAME(op3_tgt1),
	PARSER_F_NAME(op0_tgt0),
	PARSER_F_NAME(op_if),  // 0xA0
	PARSER_F_NAME(op_else),
	PARSER_F_NAME(op_while),
	PARSER_F_NAME(op0_tgt0),
	PARSER_F_NAME(op1_tgt0),
	PARSER_F_NAME(op0_tgt0),
	NULL, NULL, NULL, /*0xA8*/ NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL,  // 0xB0
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /*0xB8*/ NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL,  // 0xC0
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, /*0xC8*/ NULL, NULL, NULL,
	PARSER_F_NAME(op0_tgt0), // 0xCC
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
