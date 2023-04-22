/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include "setup.h"
#include <strings.h>
#include <linker.h>
#include <systeminfo.h>
#include <cpu.h>
#include <cpu/sync.h>
#include <buffer.h>
#include <data.h>
#include <zpack.h>
#include <xxhash.h>

efi_system_table_t* ST;
efi_boot_services_t* BS;
efi_runtime_services_t* RS;

typedef int8_t (*kernel_start_t)(system_info_t* sysinfo) __attribute__((sysv_abi));

typedef struct efi_kernel_data_t {
    uint8_t* data;
    uint64_t size;
} efi_kernel_data_t;

efi_status_t efi_setup_heap(void);
efi_status_t efi_setup_graphics(video_frame_buffer_t** vfb_res);
efi_status_t efi_lookup_kernel_partition(efi_block_io_t* bio, efi_kernel_data_t** kernel_data);
efi_status_t efi_load_local_kernel(efi_kernel_data_t** kernel_data);
efi_status_t efi_print_variable_names(void);
efi_status_t efi_is_pxe_boot(boolean_t* result);
efi_status_t efi_load_pxe_kernel(efi_kernel_data_t** kernel_data);
efi_status_t efi_main(efi_handle_t image, efi_system_table_t* system_table);

efi_status_t efi_setup_heap(void){
    efi_status_t res;

    void* heap_area = NULL;
    int64_t heap_size = 1024 * 1024 * 4; // 4 MiB

    res = BS->allocate_pool(EFI_LOADER_DATA, heap_size, &heap_area);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "memory pool creation failed. err code 0x%llx", res);

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "memory pool created");

    size_t start = (size_t)heap_area;

    memory_heap_t* heap = memory_create_heap_simple(start, start + heap_size);

    if(heap) {
        PRINTLOG(EFI, LOG_DEBUG, "heap created at 0x%p with size 0x%llx", heap_area, heap_size);
    } else {
        PRINTLOG(EFI, LOG_DEBUG, "heap creation failed");
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    memory_set_default_heap(heap);

    res = EFI_SUCCESS;

catch_efi_error:
    return res;
}

efi_status_t efi_setup_graphics(video_frame_buffer_t** vfb_res) {
    efi_status_t res;

    video_frame_buffer_t* vfb = NULL;

    efi_guid_t gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    efi_gop_t* gop;
    res = BS->locate_protocol(&gop_guid, NULL, (efi_handle_t*)&gop);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_FATAL, "gop handle failed %llx. Halting...", res);

        goto catch_efi_error;
    }

    uint64_t next_mode = gop->mode->mode;
    for(int64_t i = 0; i < gop->mode->max_mode; i++) {
        uint64_t gop_mode_size = 0;
        efi_gop_mode_info_t* gop_mi = NULL;
        if(gop->query_mode(gop, i, &gop_mode_size, &gop_mi) == EFI_SUCCESS && gop_mi->horizontal_resolution == 1280 && gop_mi->vertical_resolution == 1024) {
            next_mode = i;
            break;
        }
    }

    res = gop->set_mode(gop, next_mode);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_FATAL, "cannot set gop mode 0x%llx", res);

        goto catch_efi_error;
    }

    vfb = memory_malloc(sizeof(video_frame_buffer_t));

    if(vfb == NULL) {
        PRINTLOG(EFI, LOG_FATAL, "cannot allocate vfb");
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    vfb->physical_base_address = gop->mode->frame_buffer_base;
    vfb->virtual_base_address = gop->mode->frame_buffer_base;
    vfb->buffer_size = gop->mode->frame_buffer_size;
    vfb->width = gop->mode->information->horizontal_resolution;
    vfb->height = gop->mode->information->vertical_resolution;
    vfb->pixels_per_scanline = gop->mode->information->pixels_per_scanline;

    PRINTLOG(EFI, LOG_DEBUG, "frame buffer info %ix%i pps %i at 0x%llx size 0x%llx", vfb->width, vfb->height, vfb->pixels_per_scanline, vfb->physical_base_address, vfb->buffer_size);
    PRINTLOG(EFI, LOG_DEBUG, "vfb address 0x%p", vfb);

    *vfb_res = vfb;

    res = EFI_SUCCESS;

catch_efi_error:
    return res;
}

efi_status_t efi_lookup_kernel_partition(efi_block_io_t* bio, efi_kernel_data_t** kernel_data) {
    efi_status_t res;

    disk_t* sys_disk = efi_disk_impl_open(bio);

    if(sys_disk == NULL) {
        PRINTLOG(EFI, LOG_ERROR, "sys disk open failed");
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "openning as gpt disk");

    sys_disk = gpt_get_or_create_gpt_disk(sys_disk);

    PRINTLOG(EFI, LOG_DEBUG, "gpt disk getted");

    efi_guid_t kernel_guid = EFI_PART_TYPE_TURNSTONE_KERNEL_PART_GUID;
    const disk_partition_context_t* part_ctx = NULL;

    iterator_t* iter = sys_disk->get_partition_contexts(sys_disk);

    while(iter->end_of_iterator(iter) != 0) {
        const disk_partition_context_t* tmp_part_ctx = iter->get_item(iter);

        if(tmp_part_ctx == NULL) {
            res = EFI_OUT_OF_RESOURCES;

            break;
        }

        efi_partition_entry_t* efi_pe = (efi_partition_entry_t*)tmp_part_ctx->internal_context;

        if(efi_guid_equal(kernel_guid, efi_pe->partition_type_guid) == 0) {
            part_ctx = tmp_part_ctx;

            break;
        }

        memory_free((void*)tmp_part_ctx);

        iter = iter->next(iter);
    }

    iter->destroy(iter);


    if(part_ctx == NULL) {
        PRINTLOG(EFI, LOG_ERROR, "cannot find turnstone kernel partition");
        res = EFI_NOT_FOUND;

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "kernel start lba %llx end lba %llx", part_ctx->start_lba, part_ctx->end_lba);

    *kernel_data = memory_malloc(sizeof(efi_kernel_data_t));

    if(*kernel_data == NULL) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate kernel data");
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    (*kernel_data)->size = (part_ctx->end_lba - part_ctx->start_lba  + 1) * bio->media->block_size;

    PRINTLOG(EFI, LOG_DEBUG, "kernel size %lli", (*kernel_data)->size);

    res = sys_disk->disk.read((disk_or_partition_t*)sys_disk, part_ctx->start_lba, part_ctx->end_lba - part_ctx->start_lba  + 1, &(*kernel_data)->data);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "kernel load failed");

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "kernel loaded at 0x%p", (*kernel_data)->data);


catch_efi_error:
    return res;
}

efi_status_t efi_load_local_kernel(efi_kernel_data_t** kernel_data) {
    efi_status_t res = EFI_NOT_FOUND;

    efi_guid_t bio_guid = EFI_BLOCK_IO_PROTOCOL_GUID;
    efi_handle_t handles[128];
    uint64_t handle_size = sizeof(handles);
    res = BS->locate_handle(EFI_BY_PROTOCOL, &bio_guid, NULL, &handle_size, (efi_handle_t*)&handles);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "blocks devs retrivation failed. code: 0x%llx", res);

        goto catch_efi_error;
    }

    handle_size /= (uint64_t)sizeof(efi_handle_t);

    PRINTLOG(EFI, LOG_DEBUG, "block devs retrived. count %lli", handle_size);



    for(uint64_t i = 0; i < handle_size; i++) {
        res = EFI_NOT_FOUND;

        block_file_t* blk_dev = memory_malloc(sizeof(block_file_t));

        if(blk_dev == NULL) {
            PRINTLOG(EFI, LOG_ERROR, "cannot alloc block dev array");

            goto catch_efi_error;
        }

        if(handles[i] && !EFI_ERROR(BS->handle_protocol(handles[i], &bio_guid, (void**)&blk_dev->bio)) &&
           blk_dev->bio && blk_dev->bio->media && blk_dev->bio->media->block_size > 0) {

            PRINTLOG(EFI, LOG_DEBUG, "disk %lli mid %i block size: %i removable %i present %i readonly %i size %lli",
                     i,
                     blk_dev->bio->media->media_id,
                     blk_dev->bio->media->block_size,
                     blk_dev->bio->media->removable_media,
                     blk_dev->bio->media->media_present,
                     blk_dev->bio->media->readonly,
                     blk_dev->bio->media->last_block);

            if(blk_dev->bio->media->media_present != 1 || blk_dev->bio->media->last_block == 0) {
                memory_free(blk_dev);

                continue;
            }

            if(blk_dev->bio->media->block_size == 512) {
                uint8_t* buffer = memory_malloc(512);

                res = blk_dev->bio->read(blk_dev->bio, blk_dev->bio->media->media_id, 0, 512, buffer);

                if(res == EFI_SUCCESS) {
                    efi_pmbr_partition_t* pmbr = (efi_pmbr_partition_t*)&buffer[0x1be];

                    if(pmbr->part_type == EFI_PMBR_PART_TYPE) {
                        PRINTLOG(EFI, LOG_DEBUG, "gpt disk id %lli", i);

                        PRINTLOG(EFI, LOG_DEBUG, "trying sys disk %lli", i);

                        res = efi_lookup_kernel_partition(blk_dev->bio, kernel_data);

                        if(res == EFI_SUCCESS) {
                            memory_free(buffer);
                            break;
                        }

                    }
                }

                memory_free(buffer);

            }


        }   // end of checking disk existence

        memory_free(blk_dev);

    }     // end of iter over all disks

catch_efi_error:
    return res;
}

efi_status_t efi_print_variable_names(void) {
    efi_status_t res;

    wchar_t buffer[256];
    memory_memclean(buffer, 256);
    efi_guid_t var_ven_guid;
    uint64_t var_size = 0;

    while(1) {
        var_size = sizeof(buffer);
        res = RS->get_next_variable_name(&var_size, buffer, &var_ven_guid);

        if(res == EFI_NOT_FOUND) {
            break;
        }

        if(res != EFI_SUCCESS) {
            PRINTLOG(EFI, LOG_ERROR, "cannot get next variable name: 0x%llx", res);

            goto catch_efi_error;
        }

        char_t* var_name = wchar_to_char(buffer);
        PRINTLOG(EFI, LOG_DEBUG, "variable size %lli name: %s", var_size, var_name);
        memory_free(var_name);
    }

catch_efi_error:
    return res;
}

efi_status_t efi_is_pxe_boot(boolean_t* result){
    efi_status_t res;

    if(result == NULL) {
        res = EFI_INVALID_PARAMETER;

        goto catch_efi_error;
    }

    wchar_t* var_name_boot_current = char_to_wchar("BootCurrent");
    efi_guid_t var_global = EFI_GLOBAL_VARIABLE;
    uint32_t var_attrs = 0;
    uint64_t buffer_size = sizeof(uint16_t);
    uint16_t boot_order_idx;

    res = RS->get_variable(var_name_boot_current, &var_global, &var_attrs, &buffer_size, &boot_order_idx);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot get current boot variable 0x%llx", res);

        goto catch_efi_error;
    }

    memory_free(var_name_boot_current);


    char_t* boot_order_idx_str = itoh(boot_order_idx);

    char_t* boot_value_rev = strdup("0000tooB");

    for(int32_t i = strlen(boot_order_idx_str) - 1, j = 0; i >= 0; i--) {
        boot_value_rev[j++] = boot_order_idx_str[i];
    }

    char_t* boot_value = strrev(boot_value_rev);

    memory_free(boot_value_rev);

    PRINTLOG(EFI, LOG_DEBUG, "current boot order: %i %s", boot_order_idx, boot_value);

    wchar_t* var_val_boot_current = char_to_wchar(boot_value);

    memory_free(boot_value);

    res = RS->get_variable(var_val_boot_current, &var_global, &var_attrs, &buffer_size, NULL);

    if(res != EFI_BUFFER_TOO_SMALL) {
        PRINTLOG(EFI, LOG_ERROR, "cannot get current boot size: 0x%llx", res);

        goto catch_efi_error;
    }

    uint8_t* var_val_boot = memory_malloc(buffer_size);

    res = RS->get_variable(var_val_boot_current, &var_global, &var_attrs, &buffer_size, (void*)var_val_boot);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot get current boot variable: 0x%llx", res);

        goto catch_efi_error;
    }

    memory_free(var_val_boot_current);

    //uint32_t lo_attr = *((uint32_t*)var_val_boot);
    uint16_t lo_len = *((uint16_t*)(var_val_boot + sizeof(uint32_t)));
    wchar_t* lo_desc = (wchar_t*)(var_val_boot +  sizeof(uint32_t) + sizeof(uint16_t));
    char_t* boot_desc = wchar_to_char(lo_desc);

    PRINTLOG(EFI, LOG_DEBUG, "boot len %i desc: %s dl %lli", lo_len, boot_desc, wchar_size(lo_desc));

    efi_device_path_t* lo_fp = (efi_device_path_t*)(var_val_boot +  sizeof(uint32_t) + sizeof(uint16_t) + wchar_size(lo_desc) * sizeof(wchar_t) + sizeof(wchar_t));

    while(lo_len > 0) {
        PRINTLOG(EFI, LOG_DEBUG, "boot fp type %i subtype %i len %i", lo_fp->type, lo_fp->sub_type, lo_fp->length);

        if(lo_fp->type == EFI_DEVICE_PATH_TYPE_EOF && lo_fp->sub_type == EFI_DEVICE_PATH_SUBTYPE_EOF_EOF) {
            break;
        }

        if(lo_fp->type == EFI_DEVICE_PATH_TYPE_MESSAGING && lo_fp->sub_type == EFI_DEVICE_PATH_SUBTYPE_MESSAGING_MAC) {
            *result = 1;

            break;
        }


        lo_len -= lo_fp->length;
        lo_fp = (efi_device_path_t*)(((uint8_t*)lo_fp) + lo_fp->length);
    }

    res = EFI_SUCCESS;

catch_efi_error:
    return res;
}


efi_status_t efi_load_pxe_kernel(efi_kernel_data_t** kernel_data) {
    efi_status_t res = EFI_NOT_FOUND;

    efi_pxe_base_code_protocol_t* pxe_prot;
    efi_guid_t pxe_prot_guid = EFI_PXE_BASE_CODE_PROTOCOL_GUID;

    res = BS->locate_protocol(&pxe_prot_guid, NULL, (void**)&pxe_prot);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot find pxe protocol: 0x%llx", res);

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "pxe started: %i", pxe_prot->mode->started);
    PRINTLOG(EFI, LOG_DEBUG, "pxe discover: %i.%i.%i.%i",
             pxe_prot->mode->dhcp_ack.dhcpv4.bootp_si_addr[0],
             pxe_prot->mode->dhcp_ack.dhcpv4.bootp_si_addr[1],
             pxe_prot->mode->dhcp_ack.dhcpv4.bootp_si_addr[2],
             pxe_prot->mode->dhcp_ack.dhcpv4.bootp_si_addr[3]);


    efi_ip_address_t eipa;
    eipa.v4.addr[0] = pxe_prot->mode->dhcp_ack.dhcpv4.bootp_si_addr[0];
    eipa.v4.addr[1] = pxe_prot->mode->dhcp_ack.dhcpv4.bootp_si_addr[1];
    eipa.v4.addr[2] = pxe_prot->mode->dhcp_ack.dhcpv4.bootp_si_addr[2];
    eipa.v4.addr[3] = pxe_prot->mode->dhcp_ack.dhcpv4.bootp_si_addr[3];

    uint64_t buffer_size;
    char_t* pxeconfig = (char_t*)"pxeconf.json";
    boolean_t pxeconfig_is_json = true;

    res = pxe_prot->mtftp(pxe_prot, EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE, NULL, 0, &buffer_size, NULL, &eipa, pxeconfig, NULL, 1);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot get config size as json: 0x%llx", res);

        pxeconfig = (char_t*)"pxeconf.bson";
        pxeconfig_is_json = false;

        res = pxe_prot->mtftp(pxe_prot, EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE, NULL, 0, &buffer_size, NULL, &eipa, pxeconfig, NULL, 1);

        if(res != EFI_SUCCESS) {
            PRINTLOG(EFI, LOG_ERROR, "cannot get config size as bson: 0x%llx", res);

            goto catch_efi_error;
        }

    }

    PRINTLOG(EFI, LOG_DEBUG, "config size 0x%llx", buffer_size);

    uint8_t* buffer = memory_malloc(buffer_size);

    if(buffer == NULL) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate config buffer");
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    res = pxe_prot->mtftp(pxe_prot, EFI_PXE_BASE_CODE_TFTP_READ_FILE, buffer, 0, &buffer_size, NULL, &eipa, pxeconfig, NULL, 0);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot get config %s: 0x%llx", pxeconfig, res);

        goto catch_efi_error;
    }

    data_t pxeconf_deser_data = {DATA_TYPE_INT8_ARRAY, buffer_size, NULL, buffer};

    data_t* pxeconfig_data = NULL;

    if(pxeconfig_is_json) {
        pxeconfig_data = data_json_deserialize(&pxeconf_deser_data);
    } else {
        pxeconfig_data = data_bson_deserialize(&pxeconf_deser_data, DATA_SERIALIZE_WITH_FLAGS);
    }

    memory_free(buffer);

    if(pxeconfig_data == NULL) {
        PRINTLOG(EFI, LOG_ERROR, "cannot deserialize pxe config");
        res = EFI_INVALID_PARAMETER;

        goto catch_efi_error;
    }

    if(pxeconfig_data->name == NULL || strcmp(pxeconfig_data->name->value, "pxe-config") != 0 || pxeconfig_data->length != 2) {
        PRINTLOG(EFI, LOG_ERROR, "malformed pxe config");
        res = EFI_INVALID_PARAMETER;

        goto catch_efi_error;
    }

    data_t* kerd = &((data_t*)pxeconfig_data->value)[0];
    data_t* kerd_s = &((data_t*)pxeconfig_data->value)[1];

    if(kerd->name == NULL || strcmp(kerd->name->value, "kernel") != 0 || kerd_s->name == NULL || strcmp(kerd_s->name->value, "kernel-size") != 0) {
        PRINTLOG(EFI, LOG_ERROR, "malformed pxe config");
        res = EFI_INVALID_PARAMETER;

        goto catch_efi_error;
    }

    buffer_size = (uint64_t)kerd_s->value;
    buffer = memory_malloc(buffer_size);

    res = pxe_prot->mtftp(pxe_prot, EFI_PXE_BASE_CODE_TFTP_READ_FILE, buffer, 0, &buffer_size, NULL, &eipa, kerd->value, NULL, 0);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot get kernel: 0x%llx", res);

        goto catch_efi_error;
    }

    *kernel_data = memory_malloc(sizeof(efi_kernel_data_t));

    if(*kernel_data == NULL) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate kernel data");
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    (*kernel_data)->size = buffer_size;
    (*kernel_data)->data = buffer;

    res = EFI_SUCCESS;

catch_efi_error:
    return res;
}

efi_status_t efi_main(efi_handle_t image, efi_system_table_t* system_table) {
    ST = system_table;
    BS = system_table->boot_services;
    RS = system_table->runtime_services;

    efi_status_t res;

    video_clear_screen();

    PRINTLOG(EFI, LOG_INFO, "%s", "TURNSTONE EFI Loader Starting...");

    res = efi_setup_heap();

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_FATAL, "cannot setup heap %llx", res);

        goto catch_efi_error;
    }


    video_frame_buffer_t* vfb = NULL;

    res = efi_setup_graphics(&vfb);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_FATAL, "cannot setup graphics %llx", res);

        goto catch_efi_error;
    }

    efi_guid_t lip_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    efi_loaded_image_t* loaded_image = NULL;

    res = BS->handle_protocol(image, &lip_guid, (void**)&loaded_image);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot get details of loaded image 0x%llx", res);

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "devhandle 0x%p fp 0x%p los %i", loaded_image->device_handle, loaded_image->file_path, loaded_image->load_options_size);

    boolean_t is_pxe = 0;

    res = efi_is_pxe_boot(&is_pxe);

    if(res != EFI_SUCCESS) {
        goto catch_efi_error;
    }

    efi_kernel_data_t* kernel_data = NULL;

    if(is_pxe) {
        res = efi_load_pxe_kernel(&kernel_data);
    } else {
        res = efi_load_local_kernel(&kernel_data);
    }

    if(res != EFI_SUCCESS || kernel_data == NULL) {
        PRINTLOG(EFI, LOG_FATAL, "cannot load kernel 0x%llx", res);

        goto catch_efi_error;
    }

    zpack_format_t* zf = (zpack_format_t*)kernel_data->data;

    if(zf == NULL) {
        PRINTLOG(EFI, LOG_FATAL, "no packed kernel data");

        goto catch_efi_error;
    }

    if(zf->magic != ZPACK_FORMAT_MAGIC) {
        PRINTLOG(EFI, LOG_FATAL, "zpack magic mismatch");

        goto catch_efi_error;
    }

    if(zf->packed_size + sizeof(zpack_format_t) > kernel_data->size) {
        PRINTLOG(EFI, LOG_FATAL, "loaded kernel data size deficit");

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "packed kernel size %lli", zf->packed_size);

    uint64_t packed_hash = xxhash64_hash(kernel_data->data + sizeof(zpack_format_t), zf->packed_size);

    if(packed_hash != zf->packed_hash) {
        PRINTLOG(EFI, LOG_FATAL, "kernel packed hash mismatch 0x%llx 0x%llx", zf->packed_hash, packed_hash);

        goto catch_efi_error;
    }

    buffer_t packed_kernel_buf = buffer_encapsulate(kernel_data->data + sizeof(zpack_format_t), zf->packed_size);
    buffer_t unpacked_kernel_buf = buffer_new_with_capacity(NULL, kernel_data->size * 2 + 4096);

    kernel_data->size = zpack_unpack(packed_kernel_buf, unpacked_kernel_buf);

    PRINTLOG(EFI, LOG_DEBUG, "unpacked kernel size %lli", kernel_data->size);

    if(kernel_data->size != zf->unpacked_size) {
        PRINTLOG(EFI, LOG_FATAL, "kernel unpacked size mismatch");

        goto catch_efi_error;
    }

    uint64_t zf_unpacked_hash = zf->unpacked_hash;

    buffer_destroy(packed_kernel_buf);

    memory_free(kernel_data->data);


    kernel_data->data = buffer_get_all_bytes(unpacked_kernel_buf, NULL);

    buffer_destroy(unpacked_kernel_buf);

    uint64_t unpacked_hash = xxhash64_hash(kernel_data->data, kernel_data->size);

    if(unpacked_hash != zf_unpacked_hash) {
        PRINTLOG(EFI, LOG_FATAL, "kernel unpacked hash mismatch 0x%llx 0x%llx", zf_unpacked_hash, unpacked_hash);

        goto catch_efi_error;
    }

    uint64_t frm_start_1mib = 1 << 20;

    res = BS->allocate_pages(EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA, 0x100, &frm_start_1mib);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate frame for kernel usage at 1mib to 2mib");

        goto catch_efi_error;
    }

    memory_memclean((void*)frm_start_1mib, 0x100 * 4096);

    int64_t kernel_page_count = (kernel_data->size + 4096 - 1) / 4096 + 0x150;

    int64_t new_kernel_2m_factor = 0;
    new_kernel_2m_factor = (kernel_page_count + 512 - 1) / 512;
    kernel_page_count = new_kernel_2m_factor * 512;

    PRINTLOG(EFI, LOG_DEBUG, "new kernel page count 0x%llx", kernel_page_count);

    uint64_t new_kernel_address = 2 << 20;

    res = BS->allocate_pages(EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA, kernel_page_count, &new_kernel_address);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot alloc pages for new kernel");

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "alloc pages for new kernel succed at 0x%llx", new_kernel_address);

    memory_memclean((void*)new_kernel_address, kernel_page_count * 4096);

    if(linker_memcopy_program_and_relink((size_t)kernel_data->data, new_kernel_address)) {
        PRINTLOG(EFI, LOG_ERROR, "cannot move and relink kernel");

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "moving kernel at 0x%llx succed", new_kernel_address);
    memory_free(kernel_data->data);
    memory_free(kernel_data);


    PRINTLOG(EFI, LOG_DEBUG, "conf table count %lli", system_table->configuration_table_entry_count);
    efi_guid_t acpi_table_v2_guid = EFI_ACPI_20_TABLE_GUID;
    efi_guid_t acpi_table_v1_guid = EFI_ACPI_TABLE_GUID;

    void* acpi_rsdp = NULL;
    void* acpi_xrsdp = NULL;

    for (uint64_t i = 0; i <  system_table->configuration_table_entry_count; i++ ) {
        if(efi_guid_equal(acpi_table_v2_guid, system_table->configuration_table[i].vendor_guid) == 0) {
            acpi_xrsdp = system_table->configuration_table[i].vendor_table;
        } else if(efi_guid_equal(acpi_table_v1_guid, system_table->configuration_table[i].vendor_guid) == 0) {
            acpi_rsdp = system_table->configuration_table[i].vendor_table;
        }
    }

    uint8_t* mmap = NULL;
    uint64_t map_size, map_key, descriptor_size;
    uint32_t descriptor_version;

    BS->get_memory_map(&map_size, (efi_memory_descriptor_t*)mmap, &map_key, &descriptor_size, &descriptor_version);
    PRINTLOG(EFI, LOG_DEBUG, "mmap size %lli desc size %lli ver %i", map_size, descriptor_size, descriptor_version);

    mmap = memory_malloc(map_size);

    res = BS->get_memory_map(&map_size, (efi_memory_descriptor_t*)mmap, &map_key, &descriptor_size, &descriptor_version);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot fill memory map.");

        goto catch_efi_error;
    }

    system_info_t* sysinfo = memory_malloc(sizeof(system_info_t));
    sysinfo->boot_type = SYSTEM_INFO_BOOT_TYPE_DISK;
    sysinfo->mmap_data = mmap;
    sysinfo->mmap_size = map_size;
    sysinfo->mmap_descriptor_size = descriptor_size;
    sysinfo->mmap_descriptor_version = descriptor_version;
    sysinfo->frame_buffer = vfb;
    sysinfo->acpi_version = acpi_xrsdp != NULL?2:1;
    sysinfo->acpi_table = acpi_xrsdp != NULL?acpi_xrsdp:acpi_rsdp;
    sysinfo->kernel_start = new_kernel_address;
    sysinfo->kernel_4k_frame_count = kernel_page_count;
    sysinfo->efi_system_table = system_table;

    PRINTLOG(EFI, LOG_INFO, "calling kernel @ 0x%llx with sysinfo @ 0x%p", new_kernel_address, sysinfo);

    BS->exit_boot_services(image, map_key);

    kernel_start_t ks = (kernel_start_t)new_kernel_address;

    ks(sysinfo);

catch_efi_error:
    PRINTLOG(EFI, LOG_FATAL, "efi app could not have finished correctly, infinite loop started. Halting...");

    cpu_hlt();

    return EFI_LOAD_ERROR;
}
