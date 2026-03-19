/**
 * @file acpi.64.c
 * @brief acpi table parsers
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */
#include <acpi.h>
#include <memory.h>
#include <logging.h>
#include <ports.h>
#include <acpi/aml.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <systeminfo.h>
#include <list.h>
#include <cpu.h>
#include <strings.h>

MODULE("turnstone.kernel.hw.acpi");

acpi_aml_object_t* ACPI_PM1A_CONTROL_REGISTER = NULL;
acpi_aml_object_t* ACPI_PM1B_CONTROL_REGISTER = NULL;
acpi_aml_object_t* ACPI_RESET_REGISTER        = NULL;
acpi_contex_t* ACPI_CONTEXT                   = NULL;


int8_t acpi_reset(void){
    if(ACPI_CONTEXT == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "acpi context null");
        return -1;
    }

    if(ACPI_CONTEXT->acpi_parser_context == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "acpi parser context null");
        return -1;
    }

    return acpi_aml_write_as_integer(ACPI_CONTEXT->acpi_parser_context, ACPI_CONTEXT->fadt->reset_value, ACPI_RESET_REGISTER);
}

int8_t acpi_poweroff(void){
    if(ACPI_CONTEXT == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "acpi context null");
        return -1;
    }

    if(ACPI_CONTEXT->acpi_parser_context == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "acpi parser context null");
        return -1;
    }

    if(ACPI_PM1A_CONTROL_REGISTER == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "pm1a control register is null");
        return -1;
    }

    acpi_aml_object_t* s5 = acpi_aml_symbol_lookup(ACPI_CONTEXT->acpi_parser_context, "\\_S5_");

    if(s5 == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "No s5 state");
    } else if(s5->type != ACPI_AML_OT_PACKAGE) {
        PRINTLOG(ACPI, LOG_ERROR, "s5 wrong object type: %i", s5->type);
    } else {
        const acpi_aml_object_t* slp_type_a = list_get_data_at_position(s5->package.elements, 0);
        const acpi_aml_object_t* slp_type_b = list_get_data_at_position(s5->package.elements, 1);

        if(slp_type_a == NULL || slp_type_b == NULL) {
            PRINTLOG(ACPI, LOG_ERROR, "s5 sleep type a 0x%p or b 0x%p is null", slp_type_a, slp_type_b);
        } else {
            int64_t slp_type_a_val = 0, slp_type_b_val = 0;

            if(acpi_aml_read_as_integer(ACPI_CONTEXT->acpi_parser_context, slp_type_a, &slp_type_a_val) == 0 &&
               acpi_aml_read_as_integer(ACPI_CONTEXT->acpi_parser_context, slp_type_b, &slp_type_b_val) == 0) {


                PRINTLOG(ACPI, LOG_DEBUG, "acpi poweroff started");

                acpi_pm1_control_register_t* reg = memory_malloc(sizeof(acpi_pm1_control_register_t));

                if(reg == NULL) {
                    return -1;
                }

                reg->sleep_enable = 1;
                reg->sleep_type   = slp_type_a_val;

                uint16_t reg_val = *((uint16_t*)(void*)reg);

                PRINTLOG(ACPI, LOG_TRACE, "writing pm1a 0x%x", reg_val);

                if(acpi_aml_write_as_integer(ACPI_CONTEXT->acpi_parser_context, reg_val, ACPI_PM1A_CONTROL_REGISTER) != 0) {
                    PRINTLOG(ACPI, LOG_ERROR, "Cannot write pm1a");
                    memory_free(reg);
                } else {
                    if(ACPI_PM1B_CONTROL_REGISTER) {
                        reg->sleep_type = slp_type_b_val;
                        reg_val         = *((uint16_t*)(void*)reg);

                        PRINTLOG(ACPI, LOG_TRACE, "writing pm1b 0x%x", reg_val);

                        if(acpi_aml_write_as_integer(ACPI_CONTEXT->acpi_parser_context, reg_val, ACPI_PM1B_CONTROL_REGISTER) != 0) {
                            PRINTLOG(ACPI, LOG_ERROR, "Cannot write pm1b");
                            memory_free(reg);
                        } else {
                            cpu_hlt();
                        }
                    } else {
                        cpu_hlt();
                    }
                }

            } else {
                PRINTLOG(ACPI, LOG_ERROR, "Cannot obtain s5 sleep type");
            }

        }
    }

    PRINTLOG(ACPI, LOG_FATAL, "acpi poweroff failed");

    return -1;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
static int8_t acpi_build_register(acpi_aml_object_t** reg, uint64_t address, uint8_t address_space, uint8_t bit_width, uint8_t bit_offset){
    if(reg == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "register address null");
        return -1;
    }

    if(bit_width == 0) {
        PRINTLOG(ACPI, LOG_ERROR, "bit width zero");
        return -1;
    }

    if(address == 0) {
        PRINTLOG(ACPI, LOG_ERROR, "register address zero");
        return -1;
    }

    *reg = memory_malloc(sizeof(acpi_aml_object_t));

    if((*reg) == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "cannot allocate register memory");
        return -1;
    }

    acpi_aml_object_t* opreg = memory_malloc(sizeof(acpi_aml_object_t));

    if(opreg == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "cannot allocate opregion memory");
        memory_free(*reg);
        return -1;
    }

    opreg->type                   = ACPI_AML_OT_OPREGION;
    opreg->opregion.region_space  = address_space;
    opreg->opregion.region_offset = address;
    opreg->opregion.region_len    = bit_width / 8;

    PRINTLOG(ACPI, LOG_TRACE, "register address 0x%llx type %i len 0x%llx", opreg->opregion.region_offset, opreg->opregion.region_space, opreg->opregion.region_len);

    acpi_aml_object_t* tmp = *reg;

    tmp->type              = ACPI_AML_OT_FIELD;
    tmp->field.offset      = bit_offset;
    tmp->field.sizeasbit   = bit_width;
    tmp->field.access_type = bit_width / 8;

    if(tmp->field.access_type == 4) {
        tmp->field.access_type = 3;
    } else if(tmp->field.access_type == 8) {
        tmp->field.access_type = 4;
    }


    PRINTLOG(ACPI, LOG_TRACE, "field offset 0x%llx size %lli at %i", tmp->field.offset, tmp->field.sizeasbit, tmp->field.access_type);

    tmp->field.related_object = opreg;

    return 0;
}
#pragma GCC diagnostic pop

#define acpi_build_register_with_gas(reg, gas) acpi_build_register(reg, gas.address, gas.address_space, gas.bit_width, gas.bit_offset)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t acpi_setup(acpi_xrsdp_descriptor_t* desc) {
    ACPI_CONTEXT = memory_malloc(sizeof(acpi_contex_t));

    if(ACPI_CONTEXT == NULL) {
        return -1;
    }

    ACPI_CONTEXT->xrsdp_desc = desc;

    acpi_table_fadt_t* fadt = (acpi_table_fadt_t*)acpi_get_table(desc, "FACP");

    if(fadt == NULL) {
        PRINTLOG(ACPI, LOG_DEBUG, "fadt not found");

        return -1;
    }

    ACPI_CONTEXT->fadt = fadt;

    PRINTLOG(ACPI, LOG_DEBUG, "fadt found");

    int8_t acpi_enabled = -1;

    if(fadt->smi_command_port == 0) {
        PRINTLOG(ACPI, LOG_DEBUG, "smi command port is 0.");
        acpi_enabled = 0;
    }

    if(fadt->acpi_enable == 0 && fadt->acpi_disable == 0) {
        PRINTLOG(ACPI, LOG_DEBUG, "acpi enable/disable is 0.");
        acpi_enabled = 0;
    }

    acpi_build_register_with_gas(&ACPI_RESET_REGISTER, fadt->reset_reg);

    if(SYSTEM_INFO->acpi_version == 2) {
        acpi_build_register_with_gas(&ACPI_PM1A_CONTROL_REGISTER, fadt->pm_1a_control_block_address_64bit);

        if(fadt->pm_1b_control_block_address_64bit.address) {
            acpi_build_register_with_gas(&ACPI_PM1B_CONTROL_REGISTER, fadt->pm_1b_control_block_address_64bit);
        }
    } else {
        acpi_build_register(&ACPI_PM1A_CONTROL_REGISTER, fadt->pm_1a_control_block_address_32bit, 1, 16, 0);
        acpi_build_register(&ACPI_PM1B_CONTROL_REGISTER, fadt->pm_1b_control_block_address_32bit, 1, 16, 0);
    }

    uint16_t pm_1a_port  = fadt->pm_1a_control_block_address_64bit.address;
    uint32_t pm_1a_value = 0;

    if((pm_1a_value & 0x1) == 0x1) {
        PRINTLOG(ACPI, LOG_DEBUG, "pm 1a control block acpi en is setted");
        acpi_enabled = 0;
    }

    if(acpi_enabled != 0) {
        outb(fadt->smi_command_port, fadt->acpi_enable);

        while((inw(pm_1a_port) & 0x1) != 0x1) {;}
    }

    uint16_t pm_1b_port = fadt->pm_1b_control_block_address_64bit.address;

    if(pm_1b_port) {
        uint32_t pm_1b_value = inw(pm_1b_port);

        if((pm_1b_value & 0x1) == 0x1) {
            PRINTLOG(ACPI, LOG_DEBUG, "pm 1b control block acpi en is setted");
            acpi_enabled = 0;
        }
    }

    PRINTLOG(ACPI, LOG_INFO, "ACPI Enabled");

    uint64_t dsdt_fa = 0;

    if(SYSTEM_INFO->acpi_version == 2) {
        dsdt_fa = fadt->dsdt_address_64bit;
    } else {
        dsdt_fa = fadt->dsdt_address_32bit;
    }

    acpi_sdt_header_t* dsdt = (acpi_sdt_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(dsdt_fa);
    PRINTLOG(ACPI, LOG_DEBUG, "DSDT address 0x%p", dsdt);

    acpi_aml_parser_context_t* pctx = acpi_aml_parser_context_create_with_heap(NULL, dsdt->revision);

    if(pctx == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "aml parser creation failed");

        return -1;
    }

    ACPI_CONTEXT->acpi_parser_context = pctx;

    if(acpi_aml_parser_parse_table(pctx, dsdt) == 0) {
        PRINTLOG(ACPI, LOG_INFO, "dsdt table parsed as aml");
    } else {
        PRINTLOG(ACPI, LOG_ERROR, "dsdt table cannot parsed as aml");
        return -1;
    }

    acpi_sdt_header_t* ssdt = acpi_get_table(desc, "SSDT");

    uint32_t ssdt_cnt = 0;

    list_t* old_ssdts = list_create_list();

    while(ssdt) {
        if(acpi_aml_parser_parse_table(pctx, ssdt) == 0) {
            PRINTLOG(ACPI, LOG_INFO, "ssdt%i table parsed as aml", ssdt_cnt);
        } else {
            PRINTLOG(ACPI, LOG_ERROR, "ssdt%i table cannot parsed as aml", ssdt_cnt);
            return -1;
        }

        ssdt_cnt++;

        list_list_insert(old_ssdts, ssdt);

        ssdt = acpi_get_next_table(desc, "SSDT", old_ssdts);
    }

    list_destroy(old_ssdts);

    if(acpi_device_build(pctx) != 0) {
        PRINTLOG(ACPI, LOG_ERROR, "devices cannot be builded");
        return -1;
    }

    LOGBLOCK(ACPI, LOG_TRACE){
        acpi_device_print_all(pctx);
        acpi_aml_print_symbol_table(pctx);
    }

    if(acpi_device_init(pctx) != 0) {
        PRINTLOG(ACPI, LOG_ERROR, "devices cannot be initialized");
        return -1;
    }

    PRINTLOG(ACPI, LOG_INFO, "Devices initialized");

    if(acpi_device_reserve_memory_ranges(pctx) != 0) {
        PRINTLOG(ACPI, LOG_ERROR, "devices memory reservation failed");
        return -1;
    }

    if(acpi_build_interrupt_map(pctx) != 0) {
        PRINTLOG(ACPI, LOG_ERROR, "cannot build interrupt map");
        return -1;
    }

    PRINTLOG(ACPI, LOG_INFO, "Interrupt map builded");

    ACPI_CONTEXT->mcfg = (acpi_table_mcfg_t*)acpi_get_table(ACPI_CONTEXT->xrsdp_desc, "MCFG");

    PRINTLOG(ACPI, LOG_INFO, "acpi init completed successfully");

    return 0;
}
#pragma GCC diagnostic pop
