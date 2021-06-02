/**
 * @file acpi_aml_executor.64.c
 * @brief acpi aml executor methods
 */

#include <acpi/aml.h>
#include <video.h>


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

CREATE_EXEC_F(op2_logic);
CREATE_EXEC_F(op1_logic);


CREATE_EXEC_F(copy);
CREATE_EXEC_F(mid);
CREATE_EXEC_F(while_cont);
CREATE_EXEC_F(noop);
CREATE_EXEC_F(mth_return);
CREATE_EXEC_F(while_break);

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
CREATE_EXEC_F(frombcd);
CREATE_EXEC_F(tobcd);
CREATE_EXEC_F(revision);
CREATE_EXEC_F(debug);
CREATE_EXEC_F(timer);
CREATE_EXEC_F(breakpoint);
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
	EXEC_F_NAME(op2_tgt1_maths),
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
	EXEC_F_NAME(op1_logic),
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
	EXEC_F_NAME(mid),
	EXEC_F_NAME(while_cont),
	NULL, // 0xA0
	NULL,
	NULL,
	EXEC_F_NAME(noop),
	EXEC_F_NAME(mth_return),
	EXEC_F_NAME(while_break), // 0xA5 -> index 53

	EXEC_F_NAME(condrefof), // 0x5b12 -> index 54
	EXEC_F_NAME(load_table), // 0x5b1f -> index 55
	EXEC_F_NAME(load), // 0x5b20
	EXEC_F_NAME(stall), // 0x5b21
	EXEC_F_NAME(sleep), // 0x5b22
	EXEC_F_NAME(acquire), // 0x5b23
	EXEC_F_NAME(signal), // 0x5b24
	EXEC_F_NAME(wait), // 0x5b25
	EXEC_F_NAME(reset), // 0x5b26
	EXEC_F_NAME(release), // 0x5b27
	EXEC_F_NAME(frombcd), // 0x5b28
	EXEC_F_NAME(tobcd), // 0x5b29
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	EXEC_F_NAME(revision), // 0x5b30
	EXEC_F_NAME(debug), // 0x5b31
	NULL,
	EXEC_F_NAME(timer), // 0x5b33 -> index 75


	EXEC_F_NAME(breakpoint), // 0xCC -> index 76

	EXEC_F_NAME(method) // 0xFE -> index 77
};


int8_t acpi_aml_executor_opcode(acpi_aml_parser_context_t* ctx, apci_aml_opcode_t* opcode){
	int16_t idx = -1;

	if(opcode->opcode & 0x5b00) { // if extended ?
		int16_t tmp = opcode->opcode & 0xFF;

		if(tmp == 0x12) {
			idx = 54;
		} else if(tmp >= 0x1f && tmp <= 0x33) {
			idx = tmp - 55;
		}
	} else { // or
		if(opcode->opcode == 0xCC) {
			idx = 76;
		} else if(opcode->opcode == 0xFE) {
			idx = 77;
		} else if(opcode->opcode >= 0x70 && opcode->opcode <= 0xA5) {
			idx = opcode->opcode - 0x70;
		}
	}

	if(idx == -1) {
		printf("ACPIAML: FATAL unknown op code for execution\n");
		return -1;
	}

	acpi_aml_exec_f exec_f = acpi_aml_exec_fs[idx];

	if(exec_f == NULL) {
		printf("ACPIAML: FATAL unwanted op code for execution\n");
		return -1;
	}

	return exec_f(ctx, opcode);
}


#define UNIMPLEXEC(name) \
	int8_t acpi_aml_exec_ ## name(acpi_aml_parser_context_t * ctx, apci_aml_opcode_t * opcode){ \
		UNUSED(ctx); \
		printf("ACPIAML: FATAL method %s for opcode 0x%04x not implemented\n", #name, opcode->opcode); \
		return -1; \
	}

UNIMPLEXEC(store);
UNIMPLEXEC(refof);
UNIMPLEXEC(concat);
UNIMPLEXEC(findsetbit);
UNIMPLEXEC(derefof);
UNIMPLEXEC(concatres);
UNIMPLEXEC(notify);
UNIMPLEXEC(op_sizeof);
UNIMPLEXEC(index);
UNIMPLEXEC(match);
UNIMPLEXEC(object_type);
UNIMPLEXEC(to_buffer);
UNIMPLEXEC(to_decimalstring);
UNIMPLEXEC(to_hexstring);
UNIMPLEXEC(to_integer);
UNIMPLEXEC(to_string);
UNIMPLEXEC(op1_tgt0_maths);
UNIMPLEXEC(op1_tgt1_maths);
UNIMPLEXEC(op2_tgt1_maths);
UNIMPLEXEC(op2_logic);
UNIMPLEXEC(op1_logic);
UNIMPLEXEC(copy);
UNIMPLEXEC(mid);
UNIMPLEXEC(while_cont);
UNIMPLEXEC(noop);
UNIMPLEXEC(mth_return);
UNIMPLEXEC(while_break);
UNIMPLEXEC(condrefof);
UNIMPLEXEC(load_table);
UNIMPLEXEC(load);
UNIMPLEXEC(stall);
UNIMPLEXEC(sleep);
UNIMPLEXEC(acquire);
UNIMPLEXEC(signal);
UNIMPLEXEC(wait);
UNIMPLEXEC(reset);
UNIMPLEXEC(release);
UNIMPLEXEC(frombcd);
UNIMPLEXEC(tobcd);
UNIMPLEXEC(revision);
UNIMPLEXEC(debug);
UNIMPLEXEC(timer);
UNIMPLEXEC(breakpoint);
UNIMPLEXEC(method);
