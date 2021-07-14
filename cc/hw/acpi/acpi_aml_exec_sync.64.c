/**
 * @file acpi_aml_exec_sync.64.c
 * @brief acpi aml exec method over sync objects methods
 */

#include <acpi/aml_internal.h>
#include <video.h>

#define UNIMPLEXEC(name) \
	int8_t acpi_aml_exec_ ## name(acpi_aml_parser_context_t * ctx, acpi_aml_opcode_t * opcode){ \
		UNUSED(ctx); \
		printf("ACPIAML: FATAL method %s for opcode 0x%04x not implemented\n", #name, opcode->opcode); \
		return -1; \
	}

UNIMPLEXEC(acquire);
UNIMPLEXEC(release);
UNIMPLEXEC(signal);
UNIMPLEXEC(wait);
UNIMPLEXEC(reset);
