/**
 * @file acpi_aml_executor.64.c
 * @brief acpi aml executor methods
 */

 #include <acpi/aml.h>

#ifndef ___TESTMODE
int8_t acpi_aml_executor_opcode(acpi_aml_parser_context_t* ctx, apci_aml_opcode_t* opcode){
	UNUSED(ctx);
	UNUSED(opcode);
	return -1;
}
#endif
