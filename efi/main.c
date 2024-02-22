/**
 * @file main.c
 * @brief EFI entry point
 * @details This file contains EFI entry point and other EFI related methods.
 *
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
#include <tosdb/tosdb.h>
#include <memory/paging.h>
#include <memory/frame.h>
#include <stdbufs.h>
#include <random.h>

/*! module name */
MODULE("turnstone.efi");

/*! EFI system table global variable */
efi_system_table_t* ST;

/*! EFI boot services global variable */
efi_boot_services_t* BS;

/*! EFI runtime services global variable */
efi_runtime_services_t* RS;

/**
 * @typedef kernel_start_t
 * @brief kernel start function type
 * @details this function is called by EFI entry point to start kernel.
 * @param[in] sysinfo system information
 * @return 0 on success, -1 on failure (return means kernel is not started)
 */
typedef int8_t (*kernel_start_t)(system_info_t* sysinfo);

/**
 * @struct efi_tosdb_context_t
 * @brief EFI tosdb context
 */
typedef struct efi_tosdb_context_t {
    boolean_t        is_pxe; ///< true if tosdb is loaded from PXE, false if tosdb is loaded from local disk
    efi_block_io_t*  bio; ///< block io interface if tosdb is loaded from local disk
    tosdb_backend_t* backend; ///< tosdb backend
    tosdb_t*         tosdb; ///< tosdb instance
    uint64_t         pxe_data_size; ///< PXE data size
    uint64_t         pxe_data_base; ///< PXE data base
} efi_tosdb_context_t; ///< typedef for efi_tosdb_context_t

/**
 * @brief setup EFI heap
 * @return EFI_SUCCESS if all goes well
 */
efi_status_t efi_setup_heap(void);

/**
 * @brief setup EFI graphics
 * @details this method sets up graphics mode and returns video frame buffer. Min resolution is 1092x1080.
 * @param[out] vfb_res video frame buffer
 * @return EFI_SUCCESS if all goes well
 */
efi_status_t efi_setup_graphics(video_frame_buffer_t** vfb_res);

/**
 * @brief prints efi variable names.
 * @return EFI_SUCCESS if all goes well.
 */
efi_status_t efi_print_variable_names(void);

/**
 * @brief check is current boot is PXE boot.
 * @details for checking PXE boot, this method checks if EFI variable "BootCurrent" contains mac address.
 * @param[out] result true if current boot is PXE boot, false otherwise.
 * @return EFI_SUCCESS if all goes well.
 */
efi_status_t efi_is_pxe_boot(boolean_t* result);

/**
 * @brief EFI main function
 * @details this function is called by EFI entry point. Also this function should be EFIAPI because of EFI calling convention.
 * @param[in] image image handle
 * @param[in] system_table system table
 * @return EFI_SUCCESS if all goes well
 */
EFIAPI efi_status_t efi_main(efi_handle_t image, efi_system_table_t* system_table);

/**
 * @brief helper function for local tosdb loading
 * @param[in] bio block io interface
 * @param[out] tdb_ctx tosdb context
 * @return EFI_SUCCESS if all goes well
 */
efi_status_t efi_open_local_tosdb(efi_block_io_t* bio, efi_tosdb_context_t** tdb_ctx);

/**
 * @brief loads tosdb from disk if current boot is not PXE boot.
 * @param[out] tdb_ctx tosdb context
 * @return EFI_SUCCESS if all goes well
 */
efi_status_t efi_load_local_tosdb(efi_tosdb_context_t** tdb_ctx);

/**
 * @brief loads tosdb from PXE if current boot is PXE boot.
 * @param[out] tdb_ctx tosdb context
 * @return EFI_SUCCESS if all goes well
 */
efi_status_t efi_load_pxe_tosdb(efi_tosdb_context_t** tdb_ctx);

/**
 * @brief reads config value for system from tosdb.
 * @param[in] tdb_ctx tosdb context
 * @param[in] config_key config key
 * @param[out] config_value config value
 * @return EFI_SUCCESS if all goes well
 */
efi_status_t efi_tosdb_read_config(efi_tosdb_context_t* tdb_ctx, const char_t* config_key, void** config_value);


efi_status_t efi_tosdb_read_config(efi_tosdb_context_t* tdb_ctx, const char_t* config_key, void** config_value) {
    efi_status_t status = EFI_OUT_OF_RESOURCES;

    tosdb_database_t* db_system = tosdb_database_create_or_open(tdb_ctx->tosdb, "system");
    tosdb_table_t* tbl_config = tosdb_table_create_or_open(db_system, "config", 1 << 10, 512 << 10, 8);

    tosdb_record_t* rec_config = tosdb_table_create_record(tbl_config);

    if(rec_config == NULL) {
        PRINTLOG(EFI, LOG_FATAL, "cannot create entry point record");

        goto catch_efi_error;
    }

    if(!rec_config->set_string(rec_config, "name", config_key)) {
        PRINTLOG(EFI, LOG_FATAL, "cannot set program base");
        rec_config->destroy(rec_config);

        goto catch_efi_error;
    }

    if(!rec_config->get_record(rec_config)) {
        PRINTLOG(EFI, LOG_FATAL, "cannot get program base record");
        rec_config->destroy(rec_config);

        goto catch_efi_error;
    }

    if(!rec_config->get_data(rec_config, "value", DATA_TYPE_INT8_ARRAY, NULL, config_value)) {
        PRINTLOG(EFI, LOG_FATAL, "cannot get program base");
        rec_config->destroy(rec_config);

        goto catch_efi_error;
    }

    rec_config->destroy(rec_config);

    status = EFI_SUCCESS;

catch_efi_error:
    return status;
}

efi_status_t efi_setup_heap(void){
    efi_status_t res;

    efi_physical_address_t heap_area = NULL;
    int64_t heap_size = 1024 * 1024 * 64; // 64 MiB

    res = BS->allocate_pages(EFI_ALLOCATE_ANY_PAGES, EFI_LOADER_DATA, heap_size / FRAME_SIZE, &heap_area);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "memory pool creation failed. err code 0x%llx", res);

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "memory pool created");

    size_t start = (size_t)heap_area;

    memory_heap_t* heap = memory_create_heap_hash(start, start + heap_size);

    if(heap) {
        PRINTLOG(EFI, LOG_DEBUG, "heap created at 0x%p with size 0x%llx", (void*)heap_area, heap_size);
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

        if(gop->query_mode(gop, i, &gop_mode_size, &gop_mi) == EFI_SUCCESS) {
            PRINTLOG(EFI, LOG_DEBUG, "gop mode %lli %ix%i", i, gop_mi->horizontal_resolution, gop_mi->vertical_resolution);

            if(gop_mi->vertical_resolution == 1080 && gop_mi->horizontal_resolution == 1920) {
                next_mode = i;
            }
        }
    }

    res = gop->set_mode(gop, next_mode);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_FATAL, "cannot set gop mode 0x%llx", res);

        goto catch_efi_error;
    }

    uint64_t vfb_struct_size = sizeof(video_frame_buffer_t);

    if(vfb_struct_size % FRAME_SIZE) {
        vfb_struct_size += FRAME_SIZE - (vfb_struct_size % FRAME_SIZE);
    }

    vfb = memory_malloc_ext(NULL, vfb_struct_size, FRAME_SIZE);

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

    PRINTLOG(EFI, LOG_INFO, "frame buffer info %ix%i pps %i at 0x%llx size 0x%llx", vfb->width, vfb->height, vfb->pixels_per_scanline, vfb->physical_base_address, vfb->buffer_size);
    PRINTLOG(EFI, LOG_DEBUG, "vfb address 0x%p", vfb);

    *vfb_res = vfb;

    res = EFI_SUCCESS;

catch_efi_error:
    return res;
}

efi_status_t efi_load_pxe_tosdb(efi_tosdb_context_t** tdb_ctx) {
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
        PRINTLOG(EFI, LOG_WARNING, "cannot get config size as json: 0x%llx, trying bson", res);

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
        pxeconfig_data = data_bson_deserialize(&pxeconf_deser_data);
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

    data_t* tdbd = &((data_t*)pxeconfig_data->value)[0];
    data_t* tdbd_s = &((data_t*)pxeconfig_data->value)[1];

    if(tdbd->name == NULL || strcmp(tdbd->name->value, "tosdb") != 0 || tdbd_s->name == NULL || strcmp(tdbd_s->name->value, "tosdb-size") != 0) {
        PRINTLOG(EFI, LOG_ERROR, "malformed pxe config");
        res = EFI_INVALID_PARAMETER;

        goto catch_efi_error;
    }

    buffer_size = (uint64_t)tdbd_s->value;
    uint64_t requested_buffer_size = (2 << 20) + buffer_size;
    uint64_t requested_buffer_pages = requested_buffer_size / FRAME_SIZE;
    uint64_t requested_buffer_base = 0;

    res = BS->allocate_pages(EFI_ALLOCATE_ANY_PAGES, EFI_LOADER_DATA, requested_buffer_pages, &requested_buffer_base);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate buffer memory");

        goto catch_efi_error;
    }

    if(requested_buffer_base % (2 << 20)) {
        requested_buffer_base += (2 << 20) - (requested_buffer_base % (2 << 20));
    }

    memory_memclean((void*)requested_buffer_base, buffer_size);

    buffer = (uint8_t*)requested_buffer_base;

    res = pxe_prot->mtftp(pxe_prot, EFI_PXE_BASE_CODE_TFTP_READ_FILE, buffer, 0, &buffer_size, NULL, &eipa, tdbd->value, NULL, 0);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot get tosdb: 0x%llx", res);

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "turnstone tosdb loaded from pxe");

    buffer_t* tosdb_buffer = buffer_encapsulate(buffer, buffer_size);

    if(tosdb_buffer == NULL) {
        PRINTLOG(EFI, LOG_ERROR, "cannot encapsulate tosdb buffer");
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    buffer_set_readonly(tosdb_buffer, false);

    tosdb_backend_t* tosdb_backend = tosdb_backend_memory_from_buffer(tosdb_buffer);

    if(tosdb_backend == NULL) {
        PRINTLOG(EFI, LOG_ERROR, "cannot create tosdb backend");
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "tosdb backend created");

    tosdb_t* tdb = tosdb_new(tosdb_backend, COMPRESSION_TYPE_NONE); // tosdb already has compression type. passing none here is just a placeholder

    if(tdb == NULL) {
        PRINTLOG(EFI, LOG_ERROR, "cannot create tosdb");
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "tosdb created");

    tosdb_cache_config_t cc = {0};
    cc.bloomfilter_size = 2 << 20;
    cc.index_data_size = 4 << 20;
    cc.secondary_index_data_size = 4 << 20;
    cc.valuelog_size = 16 << 20;

    if(!tosdb_cache_config_set(tdb, &cc)) {
        PRINTLOG(EFI, LOG_ERROR, "cannot set tosdb cache config");
        tosdb_close(tdb);
        tosdb_backend_close(tosdb_backend);
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "tosdb cache config setted");

    *tdb_ctx = memory_malloc(sizeof(efi_tosdb_context_t));

    if(*tdb_ctx == NULL) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate tosdb context");
        tosdb_close(tdb);
        tosdb_backend_close(tosdb_backend);
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    (*tdb_ctx)->is_pxe = true;
    (*tdb_ctx)->pxe_data_size = buffer_size;
    (*tdb_ctx)->pxe_data_base = requested_buffer_base;
    (*tdb_ctx)->tosdb = tdb;
    (*tdb_ctx)->backend = tosdb_backend;

    res = EFI_SUCCESS;


catch_efi_error:
    return res;
}

efi_status_t efi_open_local_tosdb(efi_block_io_t* bio, efi_tosdb_context_t** tdb_ctx) {
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

    efi_guid_t kernel_guid = EFI_PART_TYPE_TURNSTONE_TOSDB_PART_GUID;

    disk_or_partition_t* tosdb_part = (disk_or_partition_t*)sys_disk->get_partition_by_type_data(sys_disk, &kernel_guid);

    if(tosdb_part == NULL) {
        PRINTLOG(EFI, LOG_ERROR, "cannot find turnstone tosdb partition");
        res = EFI_NOT_FOUND;

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "turnstone tosdb partition found");

    tosdb_backend_t* tosdb_backend = tosdb_backend_disk_new(tosdb_part);

    if(tosdb_backend == NULL) {
        PRINTLOG(EFI, LOG_ERROR, "cannot create tosdb backend");
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "tosdb backend created");

    tosdb_t* tdb = tosdb_new(tosdb_backend, COMPRESSION_TYPE_NONE); // tosdb already has compression type. passing none here is just a placeholder

    if(tdb == NULL) {
        PRINTLOG(EFI, LOG_ERROR, "cannot create tosdb");
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "tosdb created");

    tosdb_cache_config_t cc = {0};
    cc.bloomfilter_size = 2 << 20;
    cc.index_data_size = 4 << 20;
    cc.secondary_index_data_size = 4 << 20;
    cc.valuelog_size = 16 << 20;

    if(!tosdb_cache_config_set(tdb, &cc)) {
        PRINTLOG(EFI, LOG_ERROR, "cannot set tosdb cache config");
        tosdb_close(tdb);
        tosdb_backend_close(tosdb_backend);
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "tosdb cache config setted");

    *tdb_ctx = memory_malloc(sizeof(efi_tosdb_context_t));

    if(*tdb_ctx == NULL) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate tosdb context");
        tosdb_close(tdb);
        tosdb_backend_close(tosdb_backend);
        res = EFI_OUT_OF_RESOURCES;

        goto catch_efi_error;
    }

    (*tdb_ctx)->bio = bio;
    (*tdb_ctx)->tosdb = tdb;
    (*tdb_ctx)->backend = tosdb_backend;

    res = EFI_SUCCESS;

catch_efi_error:
    return res;
}

efi_status_t efi_load_local_tosdb(efi_tosdb_context_t** tdb_ctx) {
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

                        res = efi_open_local_tosdb(blk_dev->bio, tdb_ctx);

                        if(res == EFI_SUCCESS) {
                            memory_free(buffer);
                            break;
                        }

                    }
                }

                memory_free(buffer);

            }


        } // end of checking disk existence

        memory_free(blk_dev);

    } // end of iter over all disks

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

EFIAPI efi_status_t efi_main(efi_handle_t image, efi_system_table_t* system_table) {
    ST = system_table;
    BS = system_table->boot_services;
    RS = system_table->runtime_services;

    time_t boot_time = time_ns(NULL);

    efi_status_t res;

    video_clear_screen();

    PRINTLOG(EFI, LOG_INFO, "TURNSTONE EFI Loader Starting...");
    PRINTLOG(EFI, LOG_INFO, "boot time: %llu", boot_time);

    res = efi_setup_heap();

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_FATAL, "cannot setup heap %llx", res);

        goto catch_efi_error;
    }

    srand(time_ns(NULL));

    if(stdbufs_init_buffers() != 0) {
        PRINTLOG(EFI, LOG_FATAL, "cannot setup stdbufs");

        goto catch_efi_error;
    }

    res = efi_frame_allocator_init();

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_FATAL, "cannot setup frame allocator %llx", res);

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

    efi_tosdb_context_t* tdb_ctx = NULL;

    if(is_pxe) {
        res = efi_load_pxe_tosdb(&tdb_ctx);
    } else {
        res = efi_load_local_tosdb(&tdb_ctx);
    }

    if(res != EFI_SUCCESS || tdb_ctx == NULL) {
        PRINTLOG(EFI, LOG_FATAL, "cannot load tosdb 0x%llx", res);

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_INFO, "tosdb loaded");


    tosdb_database_t* db = tosdb_database_create_or_open(tdb_ctx->tosdb, "system");

    if(db == NULL) {
        PRINTLOG(EFI, LOG_FATAL, "cannot create or open database");

        goto catch_efi_error;
    }

    char_t* entry_point = NULL;

    res = efi_tosdb_read_config(tdb_ctx, "entry_point", (void**)&entry_point);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_FATAL, "cannot read entry point");

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_INFO, "entry point %s", entry_point);

    uint64_t* stack_size_ptr = 0;

    res = efi_tosdb_read_config(tdb_ctx, "stack_size", (void**)&stack_size_ptr);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_FATAL, "cannot read stack size");

        goto catch_efi_error;
    }

    uint64_t stack_size = *stack_size_ptr;

    memory_free(stack_size_ptr);

    PRINTLOG(EFI, LOG_INFO, "stack size 0x%llx", stack_size);

    uint64_t* program_base_ptr = 0;

    res = efi_tosdb_read_config(tdb_ctx, "program_base", (void**)&program_base_ptr);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_FATAL, "cannot read program base");

        goto catch_efi_error;
    }

    uint64_t program_base = *program_base_ptr;

    memory_free(program_base_ptr);

    PRINTLOG(EFI, LOG_INFO, "program base 0x%llx", program_base);

    tosdb_table_t* tbl_symbols = tosdb_table_create_or_open(db, "symbols", 1 << 10, 512 << 10, 8);
    tosdb_table_t* tbl_sections = tosdb_table_create_or_open(db, "sections", 1 << 10, 512 << 10, 8);

    tosdb_record_t* s_sym_rec = tosdb_table_create_record(tbl_symbols);

    if(!s_sym_rec) {
        PRINTLOG(EFI, LOG_ERROR, "cannot create record for search");
        tosdb_table_close(tbl_symbols);
        tosdb_database_close(db);

        goto catch_efi_error;
    }

    s_sym_rec->set_string(s_sym_rec, "name", entry_point);

    list_t* found_symbols = s_sym_rec->search_record(s_sym_rec);

    if(!found_symbols) {
        PRINTLOG(EFI, LOG_ERROR, "cannot search for entrypoint symbol");
        tosdb_table_close(tbl_symbols);
        tosdb_database_close(db);

        goto catch_efi_error;
    }

    s_sym_rec->destroy(s_sym_rec);

    if(list_size(found_symbols) == 0) {
        PRINTLOG(EFI, LOG_ERROR, "entrypoint symbol not found");
        list_destroy(found_symbols);
        tosdb_table_close(tbl_symbols);
        tosdb_database_close(db);

        goto catch_efi_error;
    }

    s_sym_rec = (tosdb_record_t*)list_queue_pop(found_symbols);
    list_destroy(found_symbols);

    if(!s_sym_rec) {
        PRINTLOG(EFI, LOG_ERROR, "cannot get record for entrypoint symbol");
        tosdb_table_close(tbl_symbols);
        tosdb_database_close(db);

        goto catch_efi_error;
    }

    uint64_t sec_id = 0;

    if(!s_sym_rec->get_int64(s_sym_rec, "section_id", (int64_t*)&sec_id)) {
        PRINTLOG(EFI, LOG_ERROR, "cannot get section id for entrypoint symbol");

        s_sym_rec->destroy(s_sym_rec);
        tosdb_table_close(tbl_symbols);
        tosdb_database_close(db);

        goto catch_efi_error;
    }

    uint64_t sym_id = 0;

    if(!s_sym_rec->get_int64(s_sym_rec, "id", (int64_t*)&sym_id)) {
        PRINTLOG(EFI, LOG_ERROR, "cannot get symbol id for entrypoint symbol");

        s_sym_rec->destroy(s_sym_rec);
        tosdb_table_close(tbl_symbols);
        tosdb_database_close(db);

        goto catch_efi_error;
    }

    s_sym_rec->destroy(s_sym_rec);


    PRINTLOG(EFI, LOG_INFO, "entry point symbol %s id 0x%llx section id: 0x%llx", entry_point, sym_id, sec_id);


    tosdb_record_t* s_sec_rec = tosdb_table_create_record(tbl_sections);

    if(!s_sec_rec) {
        PRINTLOG(EFI, LOG_ERROR, "cannot create record for search");
        tosdb_table_close(tbl_sections);
        tosdb_database_close(db);

        goto catch_efi_error;
    }

    s_sec_rec->set_int64(s_sec_rec, "id", sec_id);

    if(!s_sec_rec->get_record(s_sec_rec)) {
        PRINTLOG(EFI, LOG_ERROR, "cannot get record for search");
        tosdb_table_close(tbl_sections);
        tosdb_database_close(db);

        goto catch_efi_error;
    }

    uint64_t mod_id = 0;

    if(!s_sec_rec->get_int64(s_sec_rec, "module_id", (int64_t*)&mod_id)) {
        PRINTLOG(EFI, LOG_ERROR, "cannot get module id for entrypoint symbol");

        s_sec_rec->destroy(s_sec_rec);
        tosdb_table_close(tbl_sections);
        tosdb_database_close(db);

        goto catch_efi_error;
    }

    s_sec_rec->destroy(s_sec_rec);

    PRINTLOG(EFI, LOG_INFO, "entry point module id: 0x%llx", mod_id);

    linker_context_t* ctx = memory_malloc(sizeof(linker_context_t));

    if(!ctx) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate linker context");

        goto catch_efi_error;
    }

    ctx->entrypoint_symbol_id = sym_id;
    ctx->program_start_virtual = program_base;
    ctx->program_start_physical = program_base;
    ctx->tdb = tdb_ctx->tosdb;
    ctx->modules = hashmap_integer(16);
    ctx->got_table_buffer = buffer_new();
    ctx->symbol_table_buffer = buffer_new();
    ctx->got_symbol_index_map = hashmap_integer(1024);

    linker_global_offset_table_entry_t empty_got_entry = {0};

    buffer_append_bytes(ctx->got_table_buffer, (uint8_t*)&empty_got_entry, sizeof(linker_global_offset_table_entry_t)); // null entry
    buffer_append_bytes(ctx->got_table_buffer, (uint8_t*)&empty_got_entry, sizeof(linker_global_offset_table_entry_t)); // got itself

    time_t start_time = time_ns(NULL);
    PRINTLOG(EFI, LOG_INFO, "build module start time %llu", start_time);

    if(linker_build_module(ctx, mod_id, true) != 0) {
        PRINTLOG(EFI, LOG_ERROR, "cannot build module");
        linker_destroy_context(ctx);

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_INFO, "modules built");

    time_t end_time = time_ns(NULL);
    PRINTLOG(EFI, LOG_INFO, "build module end time %llu", end_time);

    if(!linker_is_all_symbols_resolved(ctx)) {
        PRINTLOG(EFI, LOG_ERROR, "not all symbols resolved");
        linker_destroy_context(ctx);

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_INFO, "all symbols resolved");

    time_t start_link_time = time_ns(NULL);
    PRINTLOG(EFI, LOG_INFO, "link start time %llu", start_link_time);

    if(linker_calculate_program_size(ctx) != 0) {
        PRINTLOG(EFI, LOG_ERROR, "cannot calculate program size");

        goto catch_efi_error;
    }

    uint64_t program_total_size = FRAME_SIZE + ctx->program_size + ctx->global_offset_table_size + ctx->relocation_table_size + ctx->metadata_size + ctx->symbol_table_size;
    uint64_t requested_program_size = (2 << 20) + program_total_size;
    uint64_t requested_program_pages = requested_program_size / FRAME_SIZE;

    if(requested_program_size % FRAME_SIZE != 0) {
        requested_program_pages++;
    }

    uint64_t requested_program_base = 0;

    res = BS->allocate_pages(EFI_ALLOCATE_ANY_PAGES, EFI_LOADER_DATA, requested_program_pages, &requested_program_base);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate program memory");

        goto catch_efi_error;
    }

    if(requested_program_base % (2 << 20)) {
        requested_program_base += (2 << 20) - (requested_program_base % (2 << 20));
    }

    memory_memclean((void*)requested_program_base, program_total_size);

    uint64_t heap_size = 64 << 20;
    uint64_t requested_heap_size = (2 << 20) + heap_size;
    uint64_t requested_heap_pages = requested_heap_size / FRAME_SIZE;
    uint64_t requested_heap_base = 0;

    res = BS->allocate_pages(EFI_ALLOCATE_ANY_PAGES, EFI_LOADER_DATA, requested_heap_pages, &requested_heap_base);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate heap memory");

        goto catch_efi_error;
    }

    if(requested_heap_base % (2 << 20)) {
        requested_heap_base += (2 << 20) - (requested_heap_base % (2 << 20));
    }

    memory_memclean((void*)requested_heap_base, heap_size);

    uint64_t stack_address = 0;

    res = BS->allocate_pages(EFI_ALLOCATE_ANY_PAGES, EFI_LOADER_DATA, stack_size / FRAME_SIZE, &stack_address);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate stack memory");

        goto catch_efi_error;
    }

    memory_memclean((void*)stack_address, stack_size);

    uint64_t page_table_helper_frames = 0;
    uint64_t page_frame_helper_size = 4 * FRAME_SIZE;

    res = BS->allocate_pages(EFI_ALLOCATE_ANY_PAGES, EFI_LOADER_DATA, page_frame_helper_size / FRAME_SIZE, &page_table_helper_frames);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate page_frame_helper memory");

        goto catch_efi_error;
    }

    memory_memclean((void*)page_table_helper_frames, page_frame_helper_size);

    ctx->program_start_physical = requested_program_base + FRAME_SIZE;
    ctx->program_start_virtual = (2 << 20) + FRAME_SIZE;

    if(linker_bind_linear_addresses(ctx) != 0) {
        PRINTLOG(EFI, LOG_ERROR, "cannot bind addresses");

        goto catch_efi_error;
    }

    if(linker_bind_got_entry_values(ctx) != 0) {
        PRINTLOG(EFI, LOG_ERROR, "cannot bind got entry values");

        goto catch_efi_error;
    }

    if(linker_link_program(ctx) != 0) {
        PRINTLOG(EFI, LOG_ERROR, "cannot link program");

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_INFO, "program linked");

    time_t end_link_time = time_ns(NULL);
    PRINTLOG(EFI, LOG_INFO, "link end time %llu", end_link_time);

    uint8_t* program_data = (uint8_t*)requested_program_base;

    program_header_t* program_header = (program_header_t*)requested_program_base;

    program_header->program_heap_physical_address = requested_heap_base;
    program_header->program_heap_virtual_address = (1 << 30);
    program_header->program_heap_size = heap_size;

    program_header->program_stack_physical_address = stack_address;
    program_header->program_stack_virtual_address = (1 << 30) - stack_size;
    program_header->program_stack_size = stack_size;

    ctx->page_table_helper_frames = page_table_helper_frames;

    PRINTLOG(EFI, LOG_INFO, "program will be dumped into memory at %p", program_data);

    if(linker_dump_program_to_array(ctx, LINKER_PROGRAM_DUMP_TYPE_ALL, program_data) != 0) {
        PRINTLOG(EFI, LOG_ERROR, "cannot dump program");

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_INFO, "program dumped into memory, building system info");


    PRINTLOG(EFI, LOG_DEBUG, "conf table count %lli", system_table->configuration_table_entry_count);
    efi_guid_t acpi_table_v2_guid = EFI_ACPI_20_TABLE_GUID;
    efi_guid_t acpi_table_v1_guid = EFI_ACPI_TABLE_GUID;
    efi_guid_t smbios_table_v2_guild = EFI_SMBIOS_2_TABLE_GUID;
    efi_guid_t smbios_table_v3_guild = EFI_SMBIOS_3_TABLE_GUID;
    uint64_t smbios_version = 0;
    void* smbios_table = NULL;

    void* acpi_rsdp = NULL;
    void* acpi_xrsdp = NULL;

    for (uint64_t i = 0; i <  system_table->configuration_table_entry_count; i++ ) {
        if(efi_guid_equal(acpi_table_v2_guid, system_table->configuration_table[i].vendor_guid) == 0) {
            acpi_xrsdp = system_table->configuration_table[i].vendor_table;
        } else if(efi_guid_equal(acpi_table_v1_guid, system_table->configuration_table[i].vendor_guid) == 0) {
            acpi_rsdp = system_table->configuration_table[i].vendor_table;
        } else if(efi_guid_equal(smbios_table_v2_guild, system_table->configuration_table[i].vendor_guid) == 0) {
            smbios_version = 2;
            smbios_table = system_table->configuration_table[i].vendor_table;
            PRINTLOG(EFI, LOG_INFO, "smbios v2 table 0x%p", smbios_table);
        } else if(efi_guid_equal(smbios_table_v3_guild, system_table->configuration_table[i].vendor_guid) == 0) {
            smbios_version = 3;
            smbios_table = system_table->configuration_table[i].vendor_table;
            PRINTLOG(EFI, LOG_INFO, "smbios v3 table 0x%p", smbios_table);
        }
    }

    uint8_t* mmap = NULL;
    uint64_t map_size, map_key, descriptor_size;
    uint32_t descriptor_version;

    BS->get_memory_map(&map_size, (efi_memory_descriptor_t*)mmap, &map_key, &descriptor_size, &descriptor_version);
    PRINTLOG(EFI, LOG_DEBUG, "mmap size %lli desc size %lli ver %i", map_size, descriptor_size, descriptor_version);

    uint64_t old_map_size = map_size;

    if(map_size % FRAME_SIZE != 0) {
        map_size += FRAME_SIZE - (map_size % FRAME_SIZE);
    }

    mmap = memory_malloc_ext(NULL, map_size, FRAME_SIZE);

    PRINTLOG(EFI, LOG_DEBUG, "mmap %p size 0x%llx", mmap, old_map_size);

    res = BS->get_memory_map(&old_map_size, (efi_memory_descriptor_t*)mmap, &map_key, &descriptor_size, &descriptor_version);

    if(res != EFI_SUCCESS) {
        PRINTLOG(EFI, LOG_ERROR, "cannot fill memory map.");

        goto catch_efi_error;
    }

    uint64_t si_struct_size = sizeof(system_info_t);

    if(si_struct_size % FRAME_SIZE != 0) {
        si_struct_size += FRAME_SIZE - (si_struct_size % FRAME_SIZE);
    }


    system_info_t* sysinfo = memory_malloc_ext(NULL, si_struct_size, FRAME_SIZE);

    if(!sysinfo) {
        PRINTLOG(EFI, LOG_ERROR, "cannot allocate system info");

        goto catch_efi_error;
    }

    sysinfo->boot_type = is_pxe?SYSTEM_INFO_BOOT_TYPE_PXE:SYSTEM_INFO_BOOT_TYPE_DISK;
    sysinfo->mmap_data = mmap;
    sysinfo->mmap_size = old_map_size;
    sysinfo->mmap_descriptor_size = descriptor_size;
    sysinfo->mmap_descriptor_version = descriptor_version;
    sysinfo->frame_buffer = vfb;
    sysinfo->acpi_version = acpi_xrsdp != NULL?2:1;
    sysinfo->acpi_table = acpi_xrsdp != NULL?acpi_xrsdp:acpi_rsdp;
    sysinfo->smbios_version = smbios_version;
    sysinfo->smbios_table = smbios_table;
    sysinfo->efi_system_table = system_table;
    sysinfo->program_header_physical_start = requested_program_base;
    sysinfo->program_header_virtual_start = (2 << 20);
    sysinfo->pxe_tosdb_size = tdb_ctx->pxe_data_size;
    sysinfo->pxe_tosdb_address = tdb_ctx->pxe_data_base;
    sysinfo->random_seed = rand64();

    memory_page_table_context_t* page_table_ctx = (memory_page_table_context_t*)program_header->page_table_context_address;

    frame_t frm = {0};

    frm.frame_address = (uint64_t)sysinfo;
    frm.frame_count = si_struct_size / FRAME_SIZE;

    if(memory_paging_add_va_for_frame_ext(page_table_ctx, frm.frame_address, &frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC | MEMORY_PAGING_PAGE_TYPE_READONLY) != 0) {
        PRINTLOG(EFI, LOG_ERROR, "cannot add system info to page table");

        goto catch_efi_error;
    }

    uint64_t vfb_struct_size = sizeof(video_frame_buffer_t);

    if(vfb_struct_size % FRAME_SIZE != 0) {
        vfb_struct_size += FRAME_SIZE - (vfb_struct_size % FRAME_SIZE);
    }

    frm.frame_address = (uint64_t)vfb;
    frm.frame_count = vfb_struct_size / FRAME_SIZE;

    if(memory_paging_add_va_for_frame_ext(page_table_ctx, frm.frame_address, &frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC | MEMORY_PAGING_PAGE_TYPE_READONLY) != 0) {
        PRINTLOG(EFI, LOG_ERROR, "cannot add video frame buffer to page table");

        goto catch_efi_error;
    }

    if(vfb->virtual_base_address) {
        frm.frame_address = (uint64_t)vfb->physical_base_address;
        frm.frame_count = (vfb->buffer_size + FRAME_SIZE - 1) / FRAME_SIZE;

        vfb->virtual_base_address = (64ULL << 40) | (uint64_t)vfb->virtual_base_address;

        if(memory_paging_add_va_for_frame_ext(page_table_ctx, vfb->virtual_base_address, &frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC) != 0) {
            PRINTLOG(EFI, LOG_ERROR, "cannot add video frame buffer to page table");

            goto catch_efi_error;
        }
    }

    PRINTLOG(EFI, LOG_DEBUG, "vfb virtual base address 0x%llx physical base address 0x%llx size 0x%llx", vfb->virtual_base_address, vfb->physical_base_address, vfb->buffer_size);

    frm.frame_address = (uint64_t)mmap;
    frm.frame_count = map_size / FRAME_SIZE;

    if(memory_paging_add_va_for_frame_ext(page_table_ctx, frm.frame_address, &frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC | MEMORY_PAGING_PAGE_TYPE_READONLY) != 0) {
        PRINTLOG(EFI, LOG_ERROR, "cannot add memory map to page table");

        goto catch_efi_error;
    }

    uint64_t tc_size = sizeof(memory_page_table_context_t);

    if(tc_size % FRAME_SIZE) {
        tc_size += FRAME_SIZE - (tc_size % FRAME_SIZE);
    }

    frm.frame_address = (uint64_t)program_header->page_table_context_address;
    frm.frame_count = tc_size / FRAME_SIZE;

    if(memory_paging_add_va_for_frame_ext(page_table_ctx, frm.frame_address, &frm, MEMORY_PAGING_PAGE_TYPE_NOEXEC | MEMORY_PAGING_PAGE_TYPE_READONLY) != 0) {
        PRINTLOG(EFI, LOG_ERROR, "cannot add page table context to page table");

        goto catch_efi_error;
    }

    PRINTLOG(EFI, LOG_DEBUG, "page table context virtual address 0x%llx", frm.frame_address);

    kernel_start_t kernel_start = (kernel_start_t)requested_program_base;

    tosdb_close(tdb_ctx->tosdb);
    PRINTLOG(EFI, LOG_DEBUG, "tosdb closed");

    tosdb_backend_close(tdb_ctx->backend);
    PRINTLOG(EFI, LOG_DEBUG, "tosdb backend closed");

    PRINTLOG(EFI, LOG_INFO, "calling kernel @ 0x%llx with sysinfo @ 0x%p, and will switch to 0x%llx", requested_program_base, sysinfo, program_base);
    time_t efi_end_time = time_ns(NULL);
    PRINTLOG(EFI, LOG_INFO, "efi end time %llu took: %llu ms", efi_end_time, (efi_end_time - boot_time) / 1000000ULL);

    BS->exit_boot_services(image, map_key);

    cpu_cli();

    kernel_start(sysinfo);

catch_efi_error:

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-use-of-uninitialized-value"
    if(tdb_ctx != NULL) {
        tosdb_close(tdb_ctx->tosdb);
        tosdb_backend_close(tdb_ctx->backend);
    }
#pragma GCC diagnostic pop

    PRINTLOG(EFI, LOG_FATAL, "efi app could not have finished correctly, infinite loop started. Halting...");

    buffer_t* err_buffer = buffer_get_io_buffer(BUFFER_IO_ERROR);
    char_t* err_msgs = (char_t*)buffer_get_all_bytes_and_destroy(err_buffer, NULL);

    video_print(err_msgs);

    cpu_hlt();

    return EFI_LOAD_ERROR;
}
