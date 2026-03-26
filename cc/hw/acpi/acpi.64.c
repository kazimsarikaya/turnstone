/**
 * @file acpi.64.c
 * @brief acpi table parsers
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define ___ACPI_AML_IMPLEMENTATION 0
#include <acpi.h>
#include <acpi/aml.h>
#include <acpi/aml_internal.h>
#include <memory.h>
#include <logging.h>
#include <ports.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <systeminfo.h>
#include <list.h>
#include <cpu.h>
#include <cpu/smp.h>
#include <cpu/task.h>
#include <cpu/cpu_state.h>
#include <strings.h>
#include <pci.h>
#include <time.h>
#include <time/timer.h>

MODULE("turnstone.kernel.hw.acpi");

acpi_aml_object_t* ACPI_PM1A_CONTROL_REGISTER = NULL;
acpi_aml_object_t* ACPI_PM1B_CONTROL_REGISTER = NULL;
acpi_aml_object_t* ACPI_RESET_REGISTER        = NULL;
acpi_contex_t* ACPI_CONTEXT                   = NULL;

typedef enum acpi_sleep_type_t {
    ACPI_SLEEP_TYPE_S0 = 0,
    ACPI_SLEEP_TYPE_S1 = 1,
    ACPI_SLEEP_TYPE_S2 = 2,
    ACPI_SLEEP_TYPE_S3 = 3,
    ACPI_SLEEP_TYPE_S4 = 4,
    ACPI_SLEEP_TYPE_S5 = 5,
} acpi_sleep_type_t;

const char_t*const acpi_sleep_state_type_names[] = {
    "\\_S0_",
    "\\_S1_",
    "\\_S2_",
    "\\_S3_",
    "\\_S4_",
    "\\_S5_",
};

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

static int8_t acpi_sleep_task(int64_t argc, void** argv) {
    UNUSED(argc);

    task_t* current_task = task_get_current_task();

    task_set_attribute(current_task->task_id, TASK_ATTRIBUTE_ACPI_SLEEP_TASK | TASK_ATTRIBUTE_NO_PREEMPTION);

    task_broadcast_parked_but_not_myself();
    task_wait_for_cpus_in_parked_but_not_myself();

    const char_t* sleep_type_str = "unknown";

    if(strcontains(current_task->task_name, "suspend")) {
        sleep_type_str = "suspend";
    } else if(strcontains(current_task->task_name, "hibernate")) {
        sleep_type_str = "hibernate";
    } else if(strcontains(current_task->task_name, "poweroff")) {
        sleep_type_str = "poweroff";
    }

    PRINTLOG(ACPI, LOG_INFO, "all cpus parked, going to %s", sleep_type_str);

    uint32_t reg_val_32 = (uint32_t)(uintptr_t)argv;

    uint16_t reg_val_a = (uint16_t)reg_val_32;
    uint16_t reg_val_b = (uint16_t)(reg_val_32 >> 16);


    asm volatile ("wbinvd\n");

    if(acpi_aml_write_as_integer(ACPI_CONTEXT->acpi_parser_context, reg_val_a, ACPI_PM1A_CONTROL_REGISTER) != 0) {
        PRINTLOG(ACPI, LOG_ERROR, "Cannot write pm1a");

        smp_data_t* smp_data = (smp_data_t*)0x9000;
        smp_data->is_for_wakeup = false;
        smp_data->wakeup_count--;

        return -1;
    } else {
        if(ACPI_PM1B_CONTROL_REGISTER) {
            if(acpi_aml_write_as_integer(ACPI_CONTEXT->acpi_parser_context, reg_val_b, ACPI_PM1B_CONTROL_REGISTER) != 0) {
                PRINTLOG(ACPI, LOG_ERROR, "Cannot write pm1b");

                smp_data_t* smp_data = (smp_data_t*)0x9000;
                smp_data->is_for_wakeup = false;
                smp_data->wakeup_count--;

                return -1;
            }
        }
    }

    return -1;
}

static int8_t acpi_sleep_generic(acpi_sleep_type_t sleep_type) {
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

    const char_t* sleep_state = acpi_sleep_state_type_names[sleep_type];

    acpi_aml_object_t* sleep_state_obj = acpi_aml_symbol_lookup(ACPI_CONTEXT->acpi_parser_context, sleep_state);

    if(sleep_state_obj == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "No %s state", sleep_state);
        return -1;
    }

    if(sleep_state_obj->type != ACPI_AML_OT_PACKAGE) {
        PRINTLOG(ACPI, LOG_ERROR, "%s wrong object type: %i", sleep_state, sleep_state_obj->type);
        return -1;
    }

    const acpi_aml_object_t* slp_type_a = list_get_data_at_position(sleep_state_obj->package.elements, 0);

    if(slp_type_a == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "%s sleep type a is null", sleep_state);
        return -1;
    }

    int64_t slp_type_a_val = 0;

    if(acpi_aml_read_as_integer(ACPI_CONTEXT->acpi_parser_context, slp_type_a, &slp_type_a_val) != 0) {
        PRINTLOG(ACPI, LOG_ERROR, "Cannot obtain %s sleep type a", sleep_state);
        return -1;
    }

    const acpi_aml_object_t* slp_type_b = list_get_data_at_position(sleep_state_obj->package.elements, 1);

    if(slp_type_b == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "%s sleep type b is null", sleep_state);
        return -1;
    }

    int64_t slp_type_b_val = 0;

    if(acpi_aml_read_as_integer(ACPI_CONTEXT->acpi_parser_context, slp_type_b, &slp_type_b_val) != 0) {
        PRINTLOG(ACPI, LOG_ERROR, "Cannot obtain %s sleep type b", sleep_state);
        return -1;
    }

    acpi_pm1_control_register_t reg = {0};

    reg.sleep_enable = 1;
    reg.sleep_type   = slp_type_a_val;

    uint16_t reg_val_a = reg.value;

    reg.sleep_type = slp_type_b_val;

    uint16_t reg_val_b = reg.value;

    uint32_t reg_val = ((uint32_t)reg_val_b << 16) | reg_val_a;

    smp_data_t* smp_data = (smp_data_t*)0x9000;

    smp_data->is_for_wakeup = true;
    smp_data->wakeup_count++;
    smp_data->wakeup_task = task_get_current_task();

    task_set_attribute(task_get_id(), TASK_ATTRIBUTE_WAKEUP_FROM_ACPI_SLEEP);

    memory_heap_t* heap = memory_get_default_heap();

    char_t* task_name = NULL;

    switch(sleep_type) {
    case ACPI_SLEEP_TYPE_S3:
        task_name = strprintf("acpi_sleep_suspend_task-%lli", smp_data->wakeup_count);
        break;
    case ACPI_SLEEP_TYPE_S4:
        task_name = strprintf("acpi_sleep_hibernate_task-%lli", smp_data->wakeup_count);
        break;
    case ACPI_SLEEP_TYPE_S5:
        task_name = strprintf("acpi_sleep_poweroff_task-%lli", smp_data->wakeup_count);
        break;
    default:
        task_name = strprintf("acpi_sleep_%s_task-%lli", sleep_state, smp_data->wakeup_count);
        break;
    }

    if(task_create_task(heap, 1 << 20, 64 << 10, acpi_sleep_task, 1, (void**)(uintptr_t)reg_val, task_name) == -1ULL) {
        PRINTLOG(ACPI, LOG_ERROR, "cannot create acpi sleep task");
        smp_data->is_for_wakeup = false;
        smp_data->wakeup_count--;
        memory_free(task_name);

        return -1;
    }

    memory_free(task_name);

    while(smp_data->is_for_wakeup) {
        task_msleep(10);
    }

    PRINTLOG(ACPI, LOG_INFO, "system resumed from acpi sleep");

    return 0;
}

int8_t acpi_sleep(void) {
    return acpi_sleep_generic(ACPI_SLEEP_TYPE_S3);
}

int8_t acpi_hibernate(void) {
    return acpi_sleep_generic(ACPI_SLEEP_TYPE_S4);
}

int8_t acpi_poweroff(void){
    if(acpi_sleep_generic(ACPI_SLEEP_TYPE_S5) != 0) {
        PRINTLOG(ACPI, LOG_ERROR, "acpi sleep failed");
        return -1;
    }

    while(true) {
        cpu_idle();
    }

    return 0;
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

static int8_t acpi_pm_configure_timer(void) {
    acpi_table_fadt_t* fadt = ACPI_CONTEXT->fadt;

    if(fadt->pm_timer_block_address_64bit.address == 0) {
        PRINTLOG(ACPI, LOG_ERROR, "acpi timer address is 0");
        return -1;
    }

    if(fadt->pm_timer_block_address_64bit.address_space != ACPI_AML_OPREGT_SYSIO) {
        PRINTLOG(ACPI, LOG_ERROR, "acpi timer address space is not system io");
        return -1;
    }

    uint64_t timer_address = fadt->pm_timer_block_address_64bit.address;

    uint64_t timer_tick_hz = fadt->pm_timer_block_address_64bit.bit_width == 32?3579545:14318180;

    uint64_t total_tsc = 0;

    for(int i = 0; i < 10; i++) {
        volatile uint64_t start_tsc = rdtsc();
        // sleep 100ms using acpi timer
        uint64_t start_timer = inl(timer_address);
        uint64_t end_timer   = 0;

        do {
            end_timer = inl(timer_address);
        } while((end_timer - start_timer) * 1000 / timer_tick_hz < 100);

        volatile uint64_t end_tsc = rdtsc();
        total_tsc += (end_tsc - start_tsc);
    }

    uint64_t time_timer_rdtsc_delta    = total_tsc / 1000; // for ms
    uint64_t time_timer_rdtsc_delta_us = time_timer_rdtsc_delta / 1000; // for us

    time_timer_set_rdtsc_delta(time_timer_rdtsc_delta);
    time_timer_set_rdtsc_delta_us(time_timer_rdtsc_delta_us);

    PRINTLOG(ACPI, LOG_INFO, "acpi timer configured with rdtsc delta %llu for ms and %llu for us", time_timer_rdtsc_delta, time_timer_rdtsc_delta_us);

    return 0;

}

static int8_t acpi_configure_sleep(void) {
    if(ACPI_CONTEXT == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "acpi context null");
        return -1;
    }

    uintptr_t facs_fa = ACPI_CONTEXT->fadt->firmare_control_address_64bit?(uintptr_t)ACPI_CONTEXT->fadt->firmare_control_address_64bit:ACPI_CONTEXT->fadt->firmare_control_address_32bit;
    uintptr_t facs_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(facs_fa);

    PRINTLOG(ACPI, LOG_INFO, "facs fa 0x%llx va 0x%llx", facs_fa, facs_va);

    frame_t facs_frame = {.frame_address = facs_fa, .frame_count = 0x1};

    if(memory_paging_add_va_for_frame(facs_va, &facs_frame, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(ACPI, LOG_ERROR, "cannot map facs table");
        return -1;
    }

    acpi_table_facs_t* facs = (acpi_table_facs_t*)facs_va;

    if(!facs) {
        PRINTLOG(ACPI, LOG_ERROR, "facs table not found");
        return -1;
    }

    if(strncmp(facs->signature, "FACS", 4) != 0) {
        PRINTLOG(ACPI, LOG_ERROR, "facs signature invalid");
        return -1;
    }

    if(facs->version != 2) {
        PRINTLOG(ACPI, LOG_WARNING, "facs version is not 2.");
    }

    if(facs->flags & ACPI_FACS_FEATURE_FLAG_S4BIOS_SUPPORT) {
        PRINTLOG(ACPI, LOG_DEBUG, "s4bios flag is set.");
    }

    if(facs->flags & ACPI_FACS_FEATURE_FLAG_64BIT_WAKE_SUPPORTED) {
        PRINTLOG(ACPI, LOG_DEBUG, "64 bit wakeup supported.");
    }

    facs->firmware_waking_vector = (uint32_t)0x8000;
    // NOTE: don't trust this field. Always set it to 0.
    // and wakeup from real mode.
    facs->x_firmware_waking_vector = (uint64_t)0;

    PRINTLOG(ACPI, LOG_INFO, "32 bit wakeup vector is set to 0x%x", facs->firmware_waking_vector);
    PRINTLOG(ACPI, LOG_DEBUG, "64 bit wakeup vector is set to 0x%llx", facs->x_firmware_waking_vector);

    return 0;
}

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

    boolean_t acpi_enabled = false;

    if(fadt->smi_command_port == 0) {
        PRINTLOG(ACPI, LOG_DEBUG, "smi command port is 0.");
        acpi_enabled = true;
    }

    if(fadt->acpi_enable == 0 && fadt->acpi_disable == 0) {
        PRINTLOG(ACPI, LOG_DEBUG, "acpi enable/disable is 0.");
        acpi_enabled = true;
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
        acpi_enabled = true;
    }

    if(!acpi_enabled) {
        outb(fadt->smi_command_port, fadt->acpi_enable);

        while((inw(pm_1a_port) & 0x1) != 0x1) {;}

        acpi_enabled = true;
    }

    uint16_t pm_1b_port = fadt->pm_1b_control_block_address_64bit.address;

    if(pm_1b_port) {
        uint32_t pm_1b_value = inw(pm_1b_port);

        if((pm_1b_value & 0x1) == 0x1) {
            PRINTLOG(ACPI, LOG_DEBUG, "pm 1b control block acpi en is setted");
            acpi_enabled = true;
        } else {
            PRINTLOG(ACPI, LOG_DEBUG, "pm 1b control block acpi en is not setted");
            acpi_enabled = false;
        }

        if(!acpi_enabled) {
            outb(fadt->smi_command_port, fadt->acpi_enable);

            while((inw(pm_1b_port) & 0x1) != 0x1) {;}
        }

        acpi_enabled = true;
    }

    if(!acpi_enabled) {
        PRINTLOG(ACPI, LOG_ERROR, "cannot enable acpi");
        return -1;
    }

    if(acpi_configure_sleep() != 0) {
        PRINTLOG(ACPI, LOG_ERROR, "cannot configure sleep");
        return -1;
    }

    ACPI_CONTEXT->mcfg = (acpi_table_mcfg_t*)acpi_get_table(ACPI_CONTEXT->xrsdp_desc, "MCFG");

    if(!ACPI_CONTEXT->mcfg) {
        PRINTLOG(ACPI, LOG_ERROR, "mcfg table not found");

        return -1;
    }
    for(size_t i = 0; i < ACPI_MCFG_PCI_SEGMENT_GROUP_CONFIG_COUNT(ACPI_CONTEXT->mcfg); i++) {
        uint64_t pci_base_address_fa = ACPI_CONTEXT->mcfg->pci_segment_group_configs[i].base_address;
        uint64_t pci_base_address_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(pci_base_address_fa);
        uint64_t frm_count           = (ACPI_CONTEXT->mcfg->pci_segment_group_configs[i].bus_end - ACPI_CONTEXT->mcfg->pci_segment_group_configs[i].bus_start + 1) * PCI_DEVICE_MAX_COUNT * PCI_FUNCTION_MAX_COUNT * 4096 / FRAME_SIZE;

        PRINTLOG(ACPI, LOG_INFO, "mapping pci mmio space at 0x%llx with frame count %lli", pci_base_address_fa, frm_count);

        frame_t req_frame = {pci_base_address_fa, frm_count, FRAME_TYPE_RESERVED, 0};

        if(memory_paging_add_va_for_frame(pci_base_address_va, &req_frame, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
            PRINTLOG(ACPI, LOG_ERROR, "cannot map pci mmio space");
            return -1;
        }

    }

    acpi_pm_configure_timer();

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

    LOGBLOCK(ACPI, LOG_INFO){
        acpi_device_print_all(pctx);
        acpi_aml_print_symbol_table(pctx);
    }

    PRINTLOG(ACPI, LOG_INFO, "acpi init completed successfully");

    return 0;
}
#pragma GCC diagnostic pop
