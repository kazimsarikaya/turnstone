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

static int8_t acpi_page_map_table_addresses(acpi_xrsdp_descriptor_t* xrsdp_desc){
    if(xrsdp_desc->rsdp.revision == 0) {
        uint32_t addr           = xrsdp_desc->rsdp.rsdt_address;
        acpi_sdt_header_t* rsdt = (acpi_sdt_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, (uint64_t)(addr));
        uint8_t* table_addrs    = (uint8_t*)(rsdt + 1);
        size_t table_count      = (rsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
        acpi_sdt_header_t* res;

        for(size_t i = 0; i < table_count; i++) {
            uint32_t table_addr = *((uint32_t*)(void*)(table_addrs + (i * sizeof(uint32_t))));
            res = (acpi_sdt_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, (uint64_t)(table_addr));

            PRINTLOG(ACPI, LOG_TRACE, "table %llx of %llx at fa 0x%x va 0x%p", i, table_count, table_addr, res);

            frame_t* acpi_frames = frame_get_allocator()->get_reserved_frames_of_address(frame_get_allocator(), (void*)(uint64_t)table_addr);

            if(acpi_frames == NULL) {
                PRINTLOG(ACPI, LOG_ERROR, "cannot find frames of table 0x%016x", table_addr);
            } else if((acpi_frames->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) != FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) {
                PRINTLOG(ACPI, LOG_TRACE, "frames of table 0x%016x is 0x%llx 0x%llx", table_addr, acpi_frames->frame_address, acpi_frames->frame_count);
                if(memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, acpi_frames->frame_address), acpi_frames, MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
                    PRINTLOG(ACPI, LOG_ERROR, "cannot add page mapping for table 0x%016x", table_addr);
                    return -1;
                }

                acpi_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
                char_t* sign = strndup(res->signature, 4);
                PRINTLOG(ACPI, LOG_TRACE, "table name %s", sign);
                memory_free(sign);
            }

        }
    } else if (xrsdp_desc->rsdp.revision >= 2) {
        acpi_xrsdt_t* xrsdt = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, xrsdp_desc->xrsdt);

        size_t table_count = (xrsdt->header.length - sizeof(acpi_sdt_header_t)) / sizeof(void*);
        acpi_sdt_header_t* res;

        for(size_t i = 0; i < table_count; i++) {
            res = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, xrsdt->acpi_sdt_header_ptrs[i]);

            PRINTLOG(ACPI, LOG_TRACE, "table %lli of %lli at fa 0x%p va 0x%p", i, table_count, xrsdt->acpi_sdt_header_ptrs[i], res);


            frame_t* acpi_frames = frame_get_allocator()->get_reserved_frames_of_address(frame_get_allocator(), xrsdt->acpi_sdt_header_ptrs[i]);

            if(acpi_frames == NULL) {
                PRINTLOG(ACPI, LOG_ERROR, "cannot find frames of table 0x%p", xrsdt->acpi_sdt_header_ptrs[i]);
            } else if((acpi_frames->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) != FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) {
                PRINTLOG(ACPI, LOG_TRACE, "frames of table 0x%p is 0x%llx 0x%llx", xrsdt->acpi_sdt_header_ptrs[i], acpi_frames->frame_address, acpi_frames->frame_count);
                if(memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, acpi_frames->frame_address), acpi_frames, MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
                    PRINTLOG(ACPI, LOG_ERROR, "cannot add page mapping for table 0x%p", xrsdt->acpi_sdt_header_ptrs[i]);
                    return -1;
                }

                acpi_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
                char_t* sign = strndup(res->signature, 4);
                PRINTLOG(ACPI, LOG_TRACE, "table name %s", sign);
                memory_free(sign);
            }
        }
    }

    acpi_table_fadt_t* fadt = (acpi_table_fadt_t*)acpi_get_table(xrsdp_desc, "FACP");

    if(fadt == NULL) {
        return -1;
    }

    uint64_t dsdt_fa = 0;

    if(SYSTEM_INFO->acpi_version == 2) {
        dsdt_fa = fadt->dsdt_address_64bit;
    } else {
        dsdt_fa = fadt->dsdt_address_32bit;
    }

    frame_t* acpi_frames = frame_get_allocator()->get_reserved_frames_of_address(frame_get_allocator(), (void*)dsdt_fa);

    if(acpi_frames == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "cannot find frames of  dsdt table");
    } else if((acpi_frames->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) != FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) {
        PRINTLOG(ACPI, LOG_TRACE, "frames of dsdt table is 0x%llx 0x%llx", acpi_frames->frame_address, acpi_frames->frame_count);
        if(memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, acpi_frames->frame_address), acpi_frames, MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
            PRINTLOG(ACPI, LOG_ERROR, "cannot add page mapping for dsdt table");
            return -1;
        }

        acpi_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
    }


    return 0;
}


acpi_xrsdp_descriptor_t* acpi_find_xrsdp(void){
    PRINTLOG(ACPI, LOG_DEBUG, "searching for rsdp");

    frame_t* acpi_frames = frame_get_allocator()->get_reserved_frames_of_address(frame_get_allocator(), SYSTEM_INFO->acpi_xrsdp);

    if(acpi_frames == NULL) {
        PRINTLOG(ACPI, LOG_ERROR, "cannot find acpi frames of table 0x%p", SYSTEM_INFO->acpi_xrsdp);
        return NULL;
    }

    uint64_t acpi_frames_vas = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, acpi_frames->frame_address);

    PRINTLOG(ACPI, LOG_DEBUG, "acpi area frames 0x%016llx->0x%016llx 0x%08llx", acpi_frames_vas, acpi_frames->frame_address, acpi_frames->frame_count);

    if(((acpi_frames->frame_attributes & FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) != FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED) &&
       memory_paging_add_va_for_frame(MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, acpi_frames->frame_address), acpi_frames, MEMORY_PAGING_PAGE_TYPE_READONLY | MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
        acpi_frames->frame_attributes |= FRAME_ATTRIBUTE_RESERVED_PAGE_MAPPED;
        PRINTLOG(ACPI, LOG_ERROR, "cannot add page mapping for acpi area 0x%016llx->0x%016llx 0x%08llx", acpi_frames_vas, acpi_frames->frame_address, acpi_frames->frame_count);
        return NULL;
    }

    acpi_xrsdp_descriptor_t* desc = (acpi_xrsdp_descriptor_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, SYSTEM_INFO->acpi_xrsdp);
    PRINTLOG(ACPI, LOG_DEBUG, "acpi descriptor address 0x%p", desc);


    if(desc != NULL) {
        size_t len = 0;

        if(desc->rsdp.revision == 0) {
            len = sizeof(acpi_rsdp_descriptor_t);
        } else if(desc->rsdp.revision == 2) {
            len = desc->length;
        } else {
            return NULL;
        }

        uint8_t* data2csum = (uint8_t*)desc;
        uint8_t checksum   = 0;

        for(size_t i = 0; i < len; i++) {
            checksum += data2csum[i];
        }

        if(checksum != 0x00) {
            return NULL;
        }

        acpi_page_map_table_addresses(desc);

        return desc;
    }

    return NULL;
}


uint8_t acpi_validate_checksum(acpi_sdt_header_t* sdt_header){
    uint8_t* data    = (uint8_t*)sdt_header;
    uint8_t checksum = 0;

    for(size_t i = 0; i < sdt_header->length; i++) {
        checksum += data[i];
    }

    return checksum;
}

acpi_sdt_header_t* acpi_get_next_table(acpi_xrsdp_descriptor_t* xrsdp_desc, const char_t* signature, list_t* old_tables) {
    if(xrsdp_desc->rsdp.revision == 0) {
        uint32_t addr = xrsdp_desc->rsdp.rsdt_address;

        PRINTLOG(ACPI, LOG_TRACE, "rsdt address 0x%016x", addr);

        acpi_sdt_header_t* rsdt = (acpi_sdt_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, (uint64_t)(addr));
        uint8_t* table_addrs    = (uint8_t*)(rsdt + 1);
        size_t table_count      = (rsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
        acpi_sdt_header_t* res;

        PRINTLOG(ACPI, LOG_TRACE, "looking for table %s", signature);

        for(size_t i = 0; i < table_count; i++) {
            uint32_t table_addr = *((uint32_t*)(void*)(table_addrs + (i * sizeof(uint32_t))));
            res = (acpi_sdt_header_t*)MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, (uint64_t)(table_addr));

            PRINTLOG(ACPI, LOG_TRACE, "looking for table %lli of %lli at fa 0x%x va 0x%p", i, table_count, table_addr, res);

            if(memory_memcompare(res->signature, signature, 4) == 0) {

                if(old_tables && list_contains(old_tables, res)) {
                    continue;
                }

                if(acpi_validate_checksum(res) == 0) {
                    return res;
                }

                return NULL;
            }

        }
    } else if (xrsdp_desc->rsdp.revision == 2) {
        acpi_xrsdt_t* xrsdt = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, xrsdp_desc->xrsdt);
        PRINTLOG(ACPI, LOG_TRACE, "xrsdp address 0x%p", xrsdp_desc);
        PRINTLOG(ACPI, LOG_TRACE, "xrsdt address 0x%p", xrsdp_desc->xrsdt);

        size_t table_count = (xrsdt->header.length - sizeof(acpi_sdt_header_t)) / sizeof(void*);
        acpi_sdt_header_t* res;

        for(size_t i = 0; i < table_count; i++) {
            res = MEMORY_PAGING_GET_VA_FOR_RESERVED_FA(HARDWARE, xrsdt->acpi_sdt_header_ptrs[i]);

            PRINTLOG(ACPI, LOG_TRACE, "looking for table %lli of %lli at fa 0x%p va 0x%p", i, table_count, xrsdt->acpi_sdt_header_ptrs[i], res);

            if(memory_memcompare(res->signature, signature, 4) == 0) {

                if(old_tables && list_contains(old_tables, res)) {
                    continue;
                }

                if(acpi_validate_checksum(res) == 0) {
                    return res;
                }

                return NULL;
            }
        }
    }

    return NULL;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
list_t* acpi_get_apic_table_entries_with_heap(memory_heap_t* heap, acpi_sdt_header_t* sdt_header){
    if(memory_memcompare(sdt_header->signature, "APIC", 4) != 0) {
        return NULL;
    }

    list_t* entries = list_create_list_with_heap(heap);
    acpi_table_madt_entry_t* e;
    uint8_t* data     = (uint8_t*)sdt_header;
    uint8_t* data_end = data + sdt_header->length;

    data                         += sizeof(acpi_sdt_header_t);
    e                             = memory_malloc_ext(heap, sizeof(acpi_table_madt_entry_t), 0x0);
    e->local_apic_address.type    = ACPI_MADT_ENTRY_TYPE_LOCAL_APIC_ADDRESS;
    e->local_apic_address.length  = 10;
    e->local_apic_address.address = (uint32_t)(*((uint32_t*)(void*)data));
    data                         += sizeof(uint32_t);
    e->local_apic_address.flags   = (uint32_t)(*((uint32_t*)(void*)data));
    data                         += sizeof(uint32_t);
    list_list_insert(entries, e);

    while(data < data_end) {
        e = (acpi_table_madt_entry_t*)data;
        list_list_insert(entries, e);
        data += e->info.length;

        if(e->info.type == ACPI_MADT_ENTRY_TYPE_LOCAL_APIC_ADDRESS_OVERRIDE) {
            const acpi_table_madt_entry_t* t_e = list_delete_at_position(entries, 0);
            memory_free_ext(heap, (void*)t_e);
        }

    }

    return entries;
}
#pragma GCC diagnostic pop
