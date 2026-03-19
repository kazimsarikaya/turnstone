/**
 * @file acpi.h
 * @brief acpi interface
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#ifndef ___ACPI_H
/*! prevent duplicate header error macro */
#define ___ACPI_H 0

#include <types.h>
#include <acpi/acpi_tables.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct acpi_contex_t {
    acpi_xrsdp_descriptor_t* xrsdp_desc;
    acpi_table_fadt_t*       fadt;
    acpi_table_mcfg_t*       mcfg;
    void*                    acpi_parser_context;
} acpi_contex_t;

extern acpi_contex_t* ACPI_CONTEXT;

int8_t acpi_setup(acpi_xrsdp_descriptor_t* desc);

int8_t acpi_reset(void);
int8_t acpi_poweroff(void);
int8_t acpi_setup_events(void);

#ifdef __cplusplus
}
#endif

#endif
