/**
 * @file usb_ms.64.c
 * @brief USB Mass Storage Device Class (MSC) driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define ___USB_MASS_STORAGE_IMPLEMENTATION
#include <driver/usb_ms.h>
#include <logging.h>
#include <disk.h>
#include <hashmap.h>
#include <driver/scsi.h>


MODULE("turnstone.kernel.hw.usb.ms");

typedef struct usb_driver_t {
    usb_device_t *                device;
    usb_pipeline_callback_f       pipeline_callback;
    uint32_t                      expected_packet_size;
    uint64_t                      id;
    boolean_t                     is_uas;
    boolean_t                     command_size_16_supported;
    uint32_t                      max_lun;
    uint64_t                      lba_count;
    uint32_t                      block_size;
    scsi_standard_inquiry_data_t* inquiry_data;
} usb_driver_t;


hashmap_t* usb_ms_disks = NULL;


boolean_t usb_ms_read_write(usb_driver_t* usb_driver, boolean_t read, uint32_t dtl, uint8_t* data) {
    if(usb_driver->is_uas) {
        return usb_ms_uas_read_write(usb_driver, read, dtl, data);
    }

    return usb_ms_bulk_only_read_write(usb_driver, read, dtl, data);
}

boolean_t usb_ms_send_command(usb_driver_t* usb_driver, uint32_t dtl, uint8_t flags, uint8_t lun, uint8_t command_length, uint8_t* command) {
    if(usb_driver->is_uas) {
        return usb_ms_uas_send_command(usb_driver, dtl, flags, lun, command_length, command);
    }

    return usb_ms_bulk_only_send_command(usb_driver, dtl, flags, lun, command_length, command);
}

boolean_t usb_ms_get_status(usb_driver_t* usb_driver) {
    if(usb_driver->is_uas) {
        return usb_ms_uas_get_status(usb_driver);
    }

    return usb_ms_bulk_only_get_status(usb_driver);
}

int8_t usb_ms_inquiry(usb_driver_t* usb_ms) {
    usb_ms->inquiry_data = memory_malloc(sizeof(scsi_standard_inquiry_data_t));

    if (!usb_ms->inquiry_data) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for inquiry data");

        return -1;
    }

    scsi_command_inquiry_t inquiry = {0};
    inquiry.opcode = SCSI_COMMAND_OPCODE_INQUIRY;
    inquiry.allocation_length[0] = sizeof(scsi_standard_inquiry_data_t) >> 8;
    inquiry.allocation_length[1] = sizeof(scsi_standard_inquiry_data_t) & 0xFF;

    if (!usb_ms_send_command(usb_ms, sizeof(scsi_standard_inquiry_data_t), USB_MS_FLAG_DATA_IN, 0, sizeof(scsi_command_inquiry_t), (uint8_t*)&inquiry)) {
        PRINTLOG(USB, LOG_ERROR, "cannot send inquiry command to mass storage device");
        memory_free(usb_ms->inquiry_data);

        return -1;
    }


    if(!usb_ms_read_write(usb_ms, true, sizeof(scsi_standard_inquiry_data_t), (uint8_t*)usb_ms->inquiry_data)) {
        PRINTLOG(USB, LOG_ERROR, "cannot read inquiry data from mass storage device");
        memory_free(usb_ms->inquiry_data);

        return -1;
    }


    if(!usb_ms_get_status(usb_ms)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get succeed status from mass storage device, inquiry failed");
        memory_free(usb_ms->inquiry_data);

        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "inquiry data: 0x%x 0x%x 0x%x 0x%x 0x%x",
             usb_ms->inquiry_data->peripheral_device_type,
             usb_ms->inquiry_data->peripheral_qualifier,
             usb_ms->inquiry_data->version,
             usb_ms->inquiry_data->additional_length,
             usb_ms->inquiry_data->protect);

    return 0;
}

int8_t usb_ms_sense(usb_driver_t* usb_ms, scsi_sense_data_t* sense) {
    // send request sense
    scsi_command_request_sense_t request_sense = {0};
    request_sense.opcode = SCSI_COMMAND_OPCODE_REQUEST_SENSE;
    request_sense.allocation_length = sizeof(scsi_sense_data_t);
    request_sense.control = 0;

    if (!usb_ms_send_command(usb_ms, sizeof(scsi_sense_data_t), USB_MS_FLAG_DATA_IN, 0, sizeof(scsi_command_request_sense_t), (uint8_t*)&request_sense)) {
        PRINTLOG(USB, LOG_ERROR, "cannot send request sense command to mass storage device");

        return -1;
    }

    if(!usb_ms_read_write(usb_ms, true, sizeof(scsi_sense_data_t), (uint8_t*)sense)) {
        PRINTLOG(USB, LOG_ERROR, "cannot read request sense data from mass storage device");

        return -1;
    }

    if(!usb_ms_get_status(usb_ms)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get csw from mass storage device, request sense failed");

        return -1;
    }

    PRINTLOG(USB, LOG_TRACE, "sense response_code: 0x%x sense key: 0x%x information: 0x%04llx asc: 0x%x ascq: 0x%x additional_length: 0x%x",
             sense->response_code,
             sense->sense_key,
             BYTE_SWAP32(sense->information),
             sense->asc,
             sense->ascq,
             sense->additional_length);

    return 0;
}

int8_t usb_ms_test_unit_ready(usb_driver_t* usb_ms) {
    scsi_command_test_unit_ready_t test_unit_ready = {0};
    test_unit_ready.opcode = SCSI_COMMAND_OPCODE_TEST_UNIT_READY;

    if (!usb_ms_send_command(usb_ms, 0, USB_MS_FLAG_NO_DATA, 0, sizeof(scsi_command_test_unit_ready_t), (uint8_t*)&test_unit_ready)) {
        PRINTLOG(USB, LOG_ERROR, "cannot send test unit ready command to mass storage device");

        return -1;
    }

    if(!usb_ms_get_status(usb_ms)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get succeed status from mass storage device, test unit ready failed");

        return -1;
    }

    return 0;
}


int8_t usb_mass_storage_init(usb_device_t * usb_device)
{
    if(usb_ms_disks == NULL) {
        usb_ms_disks = hashmap_integer(64);

        if(usb_ms_disks == NULL) {
            PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for mass storage disks");

            return -1;
        }
    }

    usb_config_t* config = usb_device->configurations[usb_device->selected_config];

    usb_driver_t* usb_ms = NULL;

    if(config->interface->interface_protocol == USB_PROTOCOL_MASS_STORAGE_UAS) {
        PRINTLOG(USB, LOG_DEBUG, "mass storage device is uas");
        usb_ms = usb_ms_uas_init(usb_device);
    } else {
        PRINTLOG(USB, LOG_DEBUG, "mass storage device is bulk only");
        usb_ms = usb_ms_bulk_only_init(usb_device);
    }


    if(usb_ms == NULL) {
        PRINTLOG(USB, LOG_ERROR, "cannot initialize mass storage device");

        return -1;
    }

    usb_ms->id = hashmap_size(usb_ms_disks);

    hashmap_put(usb_ms_disks, (void*)usb_ms->id, usb_ms);

    return 0;
}

uint64_t usb_mass_storage_get_disk_count(void) {
    return hashmap_size(usb_ms_disks);
}

usb_driver_t* usb_mass_storage_get_disk_by_id(uint64_t id) {
    return (usb_driver_t*)hashmap_get(usb_ms_disks, (void*)id);
}

typedef struct disk_context_t {
    memory_heap_t* heap;
    usb_driver_t*  usb_ms;
    uint64_t       block_size;
    uint8_t        lun;
} disk_context_t;

memory_heap_t* usb_ms_disk_impl_get_heap(const disk_or_partition_t* d);
uint64_t       usb_ms_disk_impl_get_size(const disk_or_partition_t* d);
uint64_t       usb_ms_disk_impl_get_block_size(const disk_or_partition_t* d);
int8_t         usb_ms_disk_impl_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data);
int8_t         usb_ms_disk_impl_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data);
int8_t         usb_ms_disk_impl_flush(const disk_or_partition_t* d);
int8_t         usb_ms_disk_impl_close(const disk_or_partition_t* d);

memory_heap_t* usb_ms_disk_impl_get_heap(const disk_or_partition_t* d){
    if(d == NULL) {
        return NULL;
    }

    disk_context_t* ctx = (disk_context_t*)d->context;

    if(ctx == NULL) {
        return NULL;
    }

    return ctx->heap;
}

uint64_t usb_ms_disk_impl_get_size(const disk_or_partition_t* d){
    disk_context_t* ctx = (disk_context_t*)d->context;
    return ctx->usb_ms->lba_count * ctx->block_size;
}

uint64_t usb_ms_disk_impl_get_block_size(const disk_or_partition_t* d){
    disk_context_t* ctx = (disk_context_t*)d->context;
    return ctx->block_size;
}

int8_t usb_ms_disk_impl_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data) {
    disk_context_t* ctx = (disk_context_t*)d->context;

    if(data == NULL) {
        return -1;
    }

    // uint64_t buffer_len = ctx->block_size;
    uint8_t* tmp_data = data;

    // for(uint64_t i = 0; i < count; i++) {
    uint8_t cbw_buffer[16] = {0};
    memory_memclean(cbw_buffer, 16);
    uint8_t cbw_buffer_len = 16;

    if(ctx->usb_ms->command_size_16_supported) {
        scsi_command_write_16_t* write_16 = (scsi_command_write_16_t*)cbw_buffer;
        write_16->opcode = SCSI_COMMAND_OPCODE_WRITE_16;
        write_16->lba = BYTE_SWAP64(lba);
        write_16->transfer_length = BYTE_SWAP32(count);

    } else {
        cbw_buffer_len = 10;
        scsi_command_write_10_t* write_10 = (scsi_command_write_10_t*)cbw_buffer;
        write_10->opcode = SCSI_COMMAND_OPCODE_WRITE_10;
        write_10->lba = BYTE_SWAP32(lba);
        write_10->transfer_length = BYTE_SWAP16(count);
    }

    if(!usb_ms_send_command(ctx->usb_ms, ctx->block_size * count, USB_MS_FLAG_DATA_OUT, ctx->lun, cbw_buffer_len, cbw_buffer)) {
        PRINTLOG(USB, LOG_ERROR, "Failed to send CBW for write command");

        return -1;
    }

    if(!usb_ms_read_write(ctx->usb_ms, false, ctx->block_size * count, tmp_data)) {
        PRINTLOG(USB, LOG_ERROR, "Failed to write data for write command");

        return -1;
    }

    if(!usb_ms_get_status(ctx->usb_ms)) {
        PRINTLOG(USB, LOG_ERROR, "Failed to receive CSW for write command");

        return -1;
    }

    // lba++;
    // tmp_data += buffer_len;
    // }


    return 0;
}

int8_t usb_ms_disk_impl_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data){
    disk_context_t* ctx = (disk_context_t*)d->context;

    uint64_t buffer_len = count * ctx->block_size;

    *data = memory_malloc_ext(NULL, buffer_len, 0x1000);

    if(*data == NULL) {
        return -1;
    }

    uint8_t* tmp_data = *data;

    // for(uint64_t i = 0; i < count; i++) {
    uint8_t cbw_buffer[16] = {0};
    memory_memclean(cbw_buffer, 16);
    uint8_t cbw_buffer_len = 16;

    if(ctx->usb_ms->command_size_16_supported) {
        scsi_command_read_16_t* read_16 = (scsi_command_read_16_t*)cbw_buffer;
        read_16->opcode = SCSI_COMMAND_OPCODE_READ_16;
        read_16->lba = BYTE_SWAP64(lba);
        read_16->transfer_length = BYTE_SWAP32(count);

    } else {
        cbw_buffer_len = 10;
        scsi_command_read_10_t* read_10 = (scsi_command_read_10_t*)cbw_buffer;
        read_10->opcode = SCSI_COMMAND_OPCODE_READ_10;
        read_10->lba = BYTE_SWAP32(lba);
        read_10->transfer_length = BYTE_SWAP16(count);
    }

    if(!usb_ms_send_command(ctx->usb_ms, ctx->block_size * count, USB_MS_FLAG_DATA_IN, ctx->lun, cbw_buffer_len, cbw_buffer)) {
        PRINTLOG(USB, LOG_ERROR, "Failed to send CBW for read command");

        return -1;
    }

    if(!usb_ms_read_write(ctx->usb_ms, true, ctx->block_size * count, tmp_data)) {
        PRINTLOG(USB, LOG_ERROR, "Failed to read data for read command");

        return -1;
    }

    if(!usb_ms_get_status(ctx->usb_ms)) {
        PRINTLOG(USB, LOG_ERROR, "Failed to receive CSW for read command");

        return -1;
    }


    // lba++;
    // tmp_data += ctx->block_size;
    // }



    return 0;
}

int8_t usb_ms_disk_impl_flush(const disk_or_partition_t* d) {
    disk_context_t* ctx = (disk_context_t*)d->context;

    uint8_t buffer[16] = {0};
    memory_memclean(buffer, 16);
    uint8_t buffer_len = 16;

    if(ctx->usb_ms->command_size_16_supported) {
        scsi_command_sync_cache_16_t* sync_cache_16 = (scsi_command_sync_cache_16_t*)buffer;
        sync_cache_16->opcode = SCSI_COMMAND_OPCODE_SYNCHRONIZE_CACHE_16;
        sync_cache_16->immed = 1;
        sync_cache_16->lba = 0;
        sync_cache_16->number_of_blocks = BYTE_SWAP32(ctx->usb_ms->lba_count);
    } else {
        buffer_len = 10;
        scsi_command_sync_cache_10_t* sync_cache = (scsi_command_sync_cache_10_t*)buffer;
        sync_cache->opcode = SCSI_COMMAND_OPCODE_SYNCHRONIZE_CACHE_10;
        sync_cache->immed = 1;
        sync_cache->lba = 0;
        sync_cache->number_of_blocks = BYTE_SWAP16(ctx->usb_ms->lba_count);
    }

    if(!usb_ms_send_command(ctx->usb_ms, 0, USB_MS_FLAG_NO_DATA, ctx->lun, buffer_len, buffer)) {
        PRINTLOG(USB, LOG_ERROR, "Failed to send CBW for sync cache command");

        return -1;
    }

    if(!usb_ms_get_status(ctx->usb_ms)) {
        PRINTLOG(USB, LOG_ERROR, "Failed to receive CSW for sync cache command");

        return -1;
    }


    return 0;
}

int8_t usb_ms_disk_impl_close(const disk_or_partition_t* d) {
    disk_context_t* ctx = (disk_context_t*)d->context;

    d->flush(d);

    memory_free(ctx);

    memory_free((void*)d);

    return 0;
}

disk_t* usb_mass_storage_disk_impl_open(usb_driver_t* usb_ms, uint8_t lun) {

    if(usb_ms == NULL) {
        return NULL;
    }

    disk_context_t* ctx = memory_malloc(sizeof(disk_context_t));

    if(ctx == NULL) {
        return NULL;
    }

    ctx->usb_ms = usb_ms;
    ctx->block_size = usb_ms->block_size;
    ctx->lun = lun;

    disk_t* d = memory_malloc(sizeof(disk_t));

    if(d == NULL) {
        memory_free(ctx);

        return NULL;
    }

    d->disk.context = ctx;
    d->disk.get_heap = usb_ms_disk_impl_get_heap;
    d->disk.get_size = usb_ms_disk_impl_get_size;
    d->disk.get_block_size = usb_ms_disk_impl_get_block_size;
    d->disk.write = usb_ms_disk_impl_write;
    d->disk.read = usb_ms_disk_impl_read;
    d->disk.flush = usb_ms_disk_impl_flush;
    d->disk.close = usb_ms_disk_impl_close;

    return d;
}
