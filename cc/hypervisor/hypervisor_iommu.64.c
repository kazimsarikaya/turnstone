/**
 * @file hypervisor_iommu.64.c
 * @brief Hypervisor IOMMU (AMD-Vi) related functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <hypervisor/hypervisor_iommu.h>
#include <hypervisor/hypervisor_vmx_utils.h> // FIXME: move allocate_region to hypervisor_utils.h
#include <logging.h>
#include <acpi.h>
#include <pci.h>
#include <memory/frame.h>
#include <memory/paging.h>

MODULE("turnstone.hypervisor.iommu");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
static void hypervisor_iommu_enable_cap(volatile amdvi_pci_capability_t * amdvi_cap) {
    volatile uint32_t* base_address_low_ptr = (volatile uint32_t*)&amdvi_cap->base_address_low.bits;

    *base_address_low_ptr |= 1;
}
#pragma GCC diagnostic pop

static uint64_t hypervisor_iommu_mmio_read_safe(uint64_t base, uint64_t offset) {
    volatile uint64_t* addr = (volatile uint64_t*)(base + offset);

    return *addr;
}

static void hypervisor_iommu_mmio_write_safe(uint64_t base, uint64_t offset, uint64_t value) {
    volatile uint64_t* addr = (volatile uint64_t*)(base + offset);

    *addr = value;
}


int8_t hypervisor_iommu_init(void) {
    logging_set_level(HYPERVISOR_IOMMU, LOG_TRACE);

    int8_t ret = 0;

    acpi_sdt_header_t* ivrs_hdr = acpi_get_table(ACPI_CONTEXT->xrsdp_desc, "IVRS");

    if(!ivrs_hdr) {
        PRINTLOG(HYPERVISOR_IOMMU, LOG_ERROR, "IVRS table not found");

        ret = -1;
        goto out;
    }

    uint8_t* ivrs = (uint8_t*)ivrs_hdr;
    int64_t ivrs_size = ivrs_hdr->length;

    for(int64_t i = 0; i < ivrs_size; i += 8) {
        for(int64_t j = 0; j < 8 && i + j < ivrs_size; j++) {
            printf("%02x ", ivrs[i + j]);
        }
        printf("\n");
    }
    printf("\n");

    ivrs += sizeof(acpi_sdt_header_t);

    ivrs_size -= sizeof(acpi_sdt_header_t);

    ivrs_ivinfo_t* ivinfo = (ivrs_ivinfo_t*)ivrs;

    PRINTLOG(HYPERVISOR_IOMMU, LOG_TRACE, "IVRS IVINFO: EFR Support: %d, DMA Remap Support: %d, GVA Size: %d, PA Size: %d, VA Size: %d, HT ATS Reserved: %d",
             ivinfo->fields.efr_support, ivinfo->fields.dma_remap_support, ivinfo->fields.gva_size, ivinfo->fields.pa_size, ivinfo->fields.va_size, ivinfo->fields.ht_ats_reserved);

    // Skip IVINFO and 8 bytes reserved area
    ivrs += sizeof(uint32_t) + sizeof(uint64_t);
    ivrs_size -= sizeof(uint32_t) + sizeof(uint64_t);

    ivrs_ivhd_type_11_t* ivhd_type_11 = NULL;

    while(ivrs_size > 0) {
        ivrs_ivhd_t* ivhd = (ivrs_ivhd_t*)ivrs;

        if(ivhd->type_length.type == 0x11) {
            PRINTLOG(HYPERVISOR_IOMMU, LOG_TRACE, "Device ID: %x:%02x:%02x.%x, IOMMU Base Address: 0x%llx",
                     ivhd->type_11.pci_segment_group, ivhd->type_11.device_id.fields.bus,
                     ivhd->type_11.device_id.fields.device, ivhd->type_11.device_id.fields.function,
                     ivhd->type_11.iommu_base_addr);
            PRINTLOG(HYPERVISOR_IOMMU, LOG_TRACE, "Features: 0x%08x, EFR Image: 0x%llx",
                     ivhd->type_11.iommu_feature_info.bits, ivhd->type_11.iommu_efr_image);

            ivhd_type_11 = &ivhd->type_11;


        } else {
            PRINTLOG(HYPERVISOR_IOMMU, LOG_TRACE, "Unimplemented IVHD type: 0x%02x", ivhd->type_length.type);
        }



        ivrs += ivhd->type_length.length;
        ivrs_size -= ivhd->type_length.length;
    }

    if(!ivhd_type_11) {
        PRINTLOG(HYPERVISOR_IOMMU, LOG_ERROR, "IVHD type 11 not found");

        ret = -1;
        goto out;
    }

    int64_t device_info_length = ivhd_type_11->length - sizeof(ivrs_ivhd_type_11_t);

    uint8_t* device_info = (uint8_t*)ivhd_type_11 + sizeof(ivrs_ivhd_type_11_t);

    uint16_t max_did = 0;

    while(device_info_length > 0) {
        uint8_t device_info_type = *device_info;

        if(device_info_type < 64) {
            // 4 bytes

            ivrs_4byte_device_info_t* device_info_4byte = (ivrs_4byte_device_info_t*)device_info;

            PRINTLOG(HYPERVISOR_IOMMU, LOG_TRACE, "Device Info: Type: 0x%02x, BDF: %x:%02x:%02x, DTE: 0x%02x",
                     device_info_4byte->type,
                     device_info_4byte->bdf.fields.bus, device_info_4byte->bdf.fields.device, device_info_4byte->bdf.fields.function,
                     device_info_4byte->dte);

            if(device_info_4byte->bdf.bits > max_did) {
                max_did = device_info_4byte->bdf.bits;
            }

            device_info += 4;
            device_info_length -= 4;
        } else if(device_info_type < 128) {
            // 8 bytes

            if(device_info_type == 72) {
                ivrs_8byte_device_info_72_t* device_info_8byte = (ivrs_8byte_device_info_72_t*)device_info;

                PRINTLOG(HYPERVISOR_IOMMU, LOG_TRACE, "Device Info: Type: 0x%02x, BDF: %x:%02x:%02x, DTE: 0x%02x, Handle: 0x%02x, Variety: %d",
                         device_info_8byte->type,
                         device_info_8byte->bdf.fields.bus, device_info_8byte->bdf.fields.device, device_info_8byte->bdf.fields.function,
                         device_info_8byte->dte, device_info_8byte->handle, device_info_8byte->variety);

                if(device_info_8byte->bdf.bits > max_did) {
                    max_did = device_info_8byte->bdf.bits;
                }

            } else {
                PRINTLOG(HYPERVISOR_IOMMU, LOG_WARNING, "Unsupported device info type: %d", device_info_type);
            }


            device_info += 8;
            device_info_length -= 8;
        } else {
            // variable length
            if(device_info_type == 0xF0) {
                ivrs_vbyte_device_info_f0_t* device_info_f0 = (ivrs_vbyte_device_info_f0_t*)device_info;

                PRINTLOG(HYPERVISOR_IOMMU, LOG_TRACE, "Device Info: Type: 0x%02x, BDF: %x:%02x:%02x, DTE: %d, HID: 0x%llx, CID: 0x%llx, UID Format: %d, UID Length: %d",
                         device_info_f0->type,
                         device_info_f0->bdf.fields.bus, device_info_f0->bdf.fields.device, device_info_f0->bdf.fields.function,
                         device_info_f0->dte, device_info_f0->hid, device_info_f0->cid, device_info_f0->uid_format, device_info_f0->uid_length);

                if(device_info_f0->bdf.bits > max_did) {
                    max_did = device_info_f0->bdf.bits;
                }

                device_info += sizeof(ivrs_vbyte_device_info_f0_t) + device_info_f0->uid_length;
                device_info_length -= sizeof(ivrs_vbyte_device_info_f0_t) + device_info_f0->uid_length;
            } else {
                PRINTLOG(HYPERVISOR_IOMMU, LOG_WARNING, "Unsupported device info type: 0x%02x", device_info_type);
                ret = -1;
                goto out;
            }
        }
    }

    // max_did should be the 4KB aligned value, used as device table size
    max_did = (max_did + 0xFFF) & ~0xFFF;
    uint64_t device_table_size = max_did * 4 * sizeof(uint64_t); // FIXME: use dte
    PRINTLOG(HYPERVISOR_IOMMU, LOG_TRACE, "Max did: 0x%x Device Table Size 0x%llx", max_did, device_table_size);


    pci_context_t* pci_ctx = pci_get_context();
    const pci_dev_t* pci_dev_found = NULL;

    for(size_t i = 0; i < list_size(pci_ctx->other_devices); i++) {
        const pci_dev_t* pci_dev = list_get_data_at_position(pci_ctx->other_devices, i);

        if(pci_dev->group_number == ivhd_type_11->pci_segment_group &&
           pci_dev->bus_number == ivhd_type_11->device_id.fields.bus &&
           pci_dev->device_number == ivhd_type_11->device_id.fields.device &&
           pci_dev->function_number == ivhd_type_11->device_id.fields.function  &&
           pci_dev->pci_header->class_code == 0x08 && pci_dev->pci_header->subclass_code == 0x06
           ) {
            pci_dev_found = pci_dev;
            PRINTLOG(HYPERVISOR_IOMMU, LOG_TRACE, "Device found: %x:%02x:%02x.%x",
                     pci_dev->group_number, pci_dev->bus_number, pci_dev->device_number, pci_dev->function_number);
            break;
        }
    }

    if(!pci_dev_found) {
        PRINTLOG(HYPERVISOR_IOMMU, LOG_ERROR, "PCI device not found");

        ret = -1;
        goto out;
    }

    volatile amdvi_pci_capability_t* amdvi_cap = (volatile amdvi_pci_capability_t*)(((uint8_t*)pci_dev_found->pci_header) + ivhd_type_11->capability_offset);

    if(amdvi_cap->header.fields.cap.capability_id != 0x0f || amdvi_cap->header.fields.cap_type != 3) {
        PRINTLOG(HYPERVISOR_IOMMU, LOG_ERROR, "AMD-Vi capability not found");

        ret = -1;
        goto out;
    }

    PRINTLOG(HYPERVISOR_IOMMU, LOG_TRACE, "AMD-Vi capability found at 0x%p", amdvi_cap);

    hypervisor_iommu_enable_cap(amdvi_cap);

    uint64_t amdvi_base_fa = ivhd_type_11->iommu_base_addr;

    frame_t amdvi_base_frame = {
        .frame_address = amdvi_base_fa,
        .frame_count = AMDVI_REG_TOTAL_SIZE / FRAME_SIZE,
    };

    uint64_t amdvi_base_va = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(amdvi_base_fa);

    if(memory_paging_add_va_for_frame(amdvi_base_va, &amdvi_base_frame, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        PRINTLOG(HYPERVISOR_IOMMU, LOG_ERROR, "cannot map AMD-Vi base va 0x%llx for frame at 0x%llx with count 0x%llx", amdvi_base_va, amdvi_base_fa, amdvi_base_frame.frame_count);

        ret = -1;
        goto out;
    }

    frame_t* frm_device_table_base_address = NULL;

    uint64_t dev_tlb_va = hypervisor_allocate_region(&frm_device_table_base_address, AMDVI_REG_TOTAL_SIZE);

    if(!dev_tlb_va) {
        PRINTLOG(HYPERVISOR_IOMMU, LOG_ERROR, "cannot allocate device table base address");

        ret = -1;
        goto out;
    }

    amdvi_device_table_base_t dev_tlb_base = {0};
    dev_tlb_base.fields.size = (device_table_size / FRAME_SIZE) - 1;
    dev_tlb_base.fields.base_address = frm_device_table_base_address->frame_address >> 12;

    hypervisor_iommu_mmio_write_safe(amdvi_base_va, AMDVI_REG_DEVICE_TABLE_BASE_BASE_ADDRESS, dev_tlb_base.bits);

    amdvi_extended_feature_t ext_feat = {.bits = hypervisor_iommu_mmio_read_safe(amdvi_base_va, AMDVI_REG_EXTENDED_FEATURES)};

    amdvi_control_t control = {0};
    control.fields.iommu_en = 1;
    control.fields.ht_tun_en = 1;

    if(ext_feat.fields.gt_sup) {
        control.fields.gt_en = 1;
    }

    if(ext_feat.fields.ga_sup) {
        control.fields.ga_en = 1;
        control.fields.ga_mode_en = 1;
        control.fields.ga_log_en = 1; // configure ga log base address 0x00e0, 0x2040, 0x2048

        frame_t* frm_ga_log_base_address = NULL;
        uint64_t ga_log_base_va = hypervisor_allocate_region(&frm_ga_log_base_address, 0x1000);

        if(!ga_log_base_va) {
            PRINTLOG(HYPERVISOR_IOMMU, LOG_ERROR, "cannot allocate GA log base address");

            ret = -1;
            goto out;
        }

        amdvi_ga_log_base_t ga_log_base = {0};
        ga_log_base.fields.base_address = frm_ga_log_base_address->frame_address >> 12;
        ga_log_base.fields.size = 8;

        hypervisor_iommu_mmio_write_safe(amdvi_base_va, AMDVI_REG_GUEST_VIRTUAL_APIC_LOG_BASE_ADDRESS, ga_log_base.bits);

        PRINTLOG(HYPERVISOR_IOMMU, LOG_TRACE, "GA Log Base FA: 0x%llx VA: 0x%llx", frm_ga_log_base_address->frame_address, ga_log_base_va);
    }

    control.fields.event_log_en = 1; // configure event log base address 0x0010, 0x2010, 0x2018

    frame_t* frm_event_log_base_address = NULL;

    uint64_t event_log_base_va = hypervisor_allocate_region(&frm_event_log_base_address, 0x1000);

    if(!event_log_base_va) {
        PRINTLOG(HYPERVISOR_IOMMU, LOG_ERROR, "cannot allocate event log base address");

        ret = -1;
        goto out;
    }

    amdvi_event_log_base_t event_log_base = {0};
    event_log_base.fields.base_address = frm_event_log_base_address->frame_address >> 12;
    event_log_base.fields.size = 8;

    hypervisor_iommu_mmio_write_safe(amdvi_base_va, AMDVI_REG_EVENT_LOG_BASE_ADDRESS, event_log_base.bits);

    PRINTLOG(HYPERVISOR_IOMMU, LOG_TRACE, "Event Log Base FA: 0x%llx VA: 0x%llx", frm_event_log_base_address->frame_address, event_log_base_va);

    control.fields.cmd_buf_en = 1; // configure command buffer base address 0x0008, 0x2000, 0x2008

    frame_t* frm_cmd_buf_base_address = NULL;

    uint64_t cmd_buf_base_va = hypervisor_allocate_region(&frm_cmd_buf_base_address, 0x1000);

    if(!cmd_buf_base_va) {
        PRINTLOG(HYPERVISOR_IOMMU, LOG_ERROR, "cannot allocate command buffer base address");

        ret = -1;
        goto out;
    }

    amdvi_command_buffer_base_t cmd_buf_base = {0};
    cmd_buf_base.fields.base_address = frm_cmd_buf_base_address->frame_address >> 12;
    cmd_buf_base.fields.size = 8;

    hypervisor_iommu_mmio_write_safe(amdvi_base_va, AMDVI_REG_COMMAND_BUFFER_BASE_ADDRESS, cmd_buf_base.bits);

    PRINTLOG(HYPERVISOR_IOMMU, LOG_TRACE, "Command Buffer Base FA: 0x%llx VA: 0x%llx", frm_cmd_buf_base_address->frame_address, cmd_buf_base_va);

    control.fields.ppr_log_en = 1; // configure ppr log base address 0x0038, 0x2030, 0x2038
    control.fields.ppr_en = 1; // configure ppr base address

    frame_t* frm_ppr_log_base_address = NULL;

    uint64_t ppr_log_base_va = hypervisor_allocate_region(&frm_ppr_log_base_address, 0x1000);

    if(!ppr_log_base_va) {
        PRINTLOG(HYPERVISOR_IOMMU, LOG_ERROR, "cannot allocate ppr log base address");

        ret = -1;
        goto out;
    }

    amdvi_ppr_log_base_t ppr_log_base = {0};
    ppr_log_base.fields.base_address = frm_ppr_log_base_address->frame_address >> 12;
    ppr_log_base.fields.size = 8;

    hypervisor_iommu_mmio_write_safe(amdvi_base_va, AMDVI_REG_PPR_LOG_BASE_ADDRESS, ppr_log_base.bits);

    PRINTLOG(HYPERVISOR_IOMMU, LOG_TRACE, "PPR Log Base FA: 0x%llx VA: 0x%llx", frm_ppr_log_base_address->frame_address, ppr_log_base_va);

    // TODO: set other control fields: 3,4,14,29 -> this fields need msi int configuration, they are interrupt related

    hypervisor_iommu_mmio_write_safe(amdvi_base_va, AMDVI_REG_CONTROL, control.bits);


out:
    logging_set_level(HYPERVISOR_IOMMU, LOG_INFO);
    return ret;
}
