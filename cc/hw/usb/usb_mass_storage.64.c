/**
 * @file usb_mass_storage.64.c
 * @brief USB Mass Storage Device Class (MSC) driver
 */

#include <driver/usb.h>
#include <video.h>
#include <driver/scsi.h>
#include <future.h>
#include <random.h>
#include <time/timer.h>
#include <disk.h>
#include <driver/usb_mass_storage_disk.h>
#include <hashmap.h>


MODULE("turnstone.kernel.hw.usb.mass_storage");

typedef struct usb_driver_t {
    uint64_t                      id;
    usb_device_t *                device;
    uint8_t                       interface_number;
    uint8_t                       in_endpoint;
    uint8_t                       out_endpoint;
    uint32_t                      max_lun;
    uint32_t                      cbw_tag;
    uint64_t                      lba_count;
    uint32_t                      block_size;
    lock_t                        lock;
    boolean_t                     command_size_16_supported;
    scsi_standard_inquiry_data_t* inquiry_data;
} usb_driver_t;

#define USB_MASS_STORAGE_REQUEST_GET_MAX_LUN 0xFE
#define USB_MASS_STORAGE_REQUEST_RESET       0xFF

#define USB_MASS_STORAGE_CBW_SIGNATURE 0x43425355
#define USB_MASS_STORAGE_CSW_SIGNATURE 0x53425355

#define USB_MASS_STORAGE_CBW_FLAG_DATA_OUT 0x00
#define USB_MASS_STORAGE_CBW_FLAG_DATA_IN  0x80
#define USB_MASS_STORAGE_CBW_FLAG_NO_DATA  0x00

typedef struct usb_mass_storage_cbw_t {
    uint32_t signature;
    uint32_t tag;
    uint32_t data_transfer_length;
    uint8_t  flags;
    uint8_t  lun;
    uint8_t  command_length;
    uint8_t  command[16];
} __attribute__((packed)) usb_mass_storage_cbw_t;

typedef struct usb_mass_storage_csw_t {
    uint32_t signature;
    uint32_t tag;
    uint32_t data_residue;
    uint8_t  status;
} __attribute__((packed)) usb_mass_storage_csw_t;

boolean_t usb_mass_storage_send_cbw(usb_driver_t* usb_driver, uint32_t dtl, uint8_t flags, uint8_t lun, uint8_t command_length, uint8_t* command);
boolean_t usb_mass_storage_get_csw(usb_driver_t* usb_driver);
boolean_t usb_mass_storage_read_write(usb_driver_t* usb_driver, boolean_t read, uint32_t dtl, uint8_t* data);

hashmap_t* usb_mass_storage_disks = NULL;

boolean_t usb_mass_storage_read_write(usb_driver_t* usb_driver, boolean_t read, uint32_t dtl, uint8_t* data) {
    usb_transfer_t ut = {0};

    ut.device = usb_driver->device;

    if(read) {
        ut.endpoint = usb_driver->device->configurations[usb_driver->device->selected_config]->endpoints[usb_driver->in_endpoint];
    } else {
        ut.endpoint = usb_driver->device->configurations[usb_driver->device->selected_config]->endpoints[usb_driver->out_endpoint];
    }


    ut.is_async = true;
    ut.need_future = true;
    ut.length = dtl;
    ut.data = data;

    int8_t res =  usb_driver->device->controller->bulk_transfer(usb_driver->device->controller, &ut);

    if(res != 0 || ut.transfer_future == NULL) {
        PRINTLOG(USB, LOG_ERROR, "cannot get inquiry data from mass storage device");
        lock_release(usb_driver->lock);

        return false;
    }

    future_get_data_and_destroy(ut.transfer_future);


    return ut.complete && ut.success;
}

boolean_t usb_mass_storage_send_cbw(usb_driver_t* usb_driver, uint32_t dtl, uint8_t flags, uint8_t lun, uint8_t command_length, uint8_t* command) {
    lock_acquire(usb_driver->lock);

    usb_driver->cbw_tag = rand();

    usb_mass_storage_cbw_t cbw = {0};
    cbw.signature = USB_MASS_STORAGE_CBW_SIGNATURE;
    cbw.tag = usb_driver->cbw_tag;
    cbw.data_transfer_length = dtl;
    cbw.flags = flags;
    cbw.lun = lun;
    cbw.command_length = command_length;
    memory_memcopy(command, &cbw.command, command_length);

    usb_transfer_t ut = {0};

    ut.device = usb_driver->device;
    ut.endpoint = usb_driver->device->configurations[usb_driver->device->selected_config]->endpoints[usb_driver->out_endpoint];
    ut.is_async = true;
    ut.need_future = true;
    ut.length = sizeof(usb_mass_storage_cbw_t);
    ut.data = (uint8_t*)&cbw;

    int8_t res =  usb_driver->device->controller->bulk_transfer(usb_driver->device->controller, &ut);

    if(res != 0 || ut.transfer_future == NULL) {
        PRINTLOG(USB, LOG_ERROR, "cannot send command to mass storage device");
        lock_release(usb_driver->lock);

        return false;
    }

    future_get_data_and_destroy(ut.transfer_future);

    return ut.complete && ut.success;
}

boolean_t usb_mass_storage_get_csw(usb_driver_t* usb_driver) {
    usb_transfer_t ut = {0};
    usb_mass_storage_csw_t csw = {0};

    ut.device = usb_driver->device;
    ut.endpoint = usb_driver->device->configurations[usb_driver->device->selected_config]->endpoints[usb_driver->in_endpoint];
    ut.is_async = true;
    ut.need_future = true;
    ut.length = sizeof(usb_mass_storage_csw_t);
    ut.data = (uint8_t*)&csw;

    int8_t res =  usb_driver->device->controller->bulk_transfer(usb_driver->device->controller, &ut);

    if(res != 0 || ut.transfer_future == NULL) {
        PRINTLOG(USB, LOG_ERROR, "cannot get csw from mass storage device");
        lock_release(usb_driver->lock);

        return false;
    }

    future_get_data_and_destroy(ut.transfer_future);

    if(!ut.complete || !ut.success) {
        PRINTLOG(USB, LOG_ERROR, "cannot get csw from mass storage device");
        lock_release(usb_driver->lock);

        return false;
    }

    if(csw.signature != USB_MASS_STORAGE_CSW_SIGNATURE) {
        PRINTLOG(USB, LOG_ERROR, "invalid csw signature: 0x%x != 0x%x", csw.signature, USB_MASS_STORAGE_CSW_SIGNATURE);
        lock_release(usb_driver->lock);

        return false;
    }

    if(csw.tag != usb_driver->cbw_tag) {
        PRINTLOG(USB, LOG_ERROR, "invalid csw tag: 0x%x != 0x%x", csw.tag, usb_driver->cbw_tag);
        lock_release(usb_driver->lock);

        return false;
    }

    if(csw.status != 0) {
        PRINTLOG(USB, LOG_ERROR, "invalid csw status: 0x%x", csw.status);
        lock_release(usb_driver->lock);

        return false;
    }

    PRINTLOG(USB, LOG_TRACE, "csw received. data residue: %d", csw.data_residue);
    lock_release(usb_driver->lock);

    return true;
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t usb_mass_storage_init(usb_device_t * usb_device)
{
    if(usb_mass_storage_disks == NULL) {
        usb_mass_storage_disks = hashmap_integer(64);

        if(usb_mass_storage_disks == NULL) {
            PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for mass storage disks");

            return -1;
        }
    }


    usb_driver_t* usb_ms = memory_malloc(sizeof(usb_driver_t));

    if (!usb_ms) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for mass storage device");

        return -1;
    }

    usb_ms->device = usb_device;
    usb_device->driver = usb_ms;

    usb_ms->interface_number = usb_device->configurations[usb_device->selected_config]->interface->interface_number;

    if(usb_device->configurations[usb_device->selected_config]->endpoints[0]->in) {
        usb_ms->in_endpoint = 0;
        usb_ms->out_endpoint = 1;
    } else {
        usb_ms->out_endpoint = 0;
        usb_ms->in_endpoint = 1;
    }

    PRINTLOG(USB, LOG_DEBUG, "in endpoint: 0x%x out endpoint 0x%x", usb_ms->in_endpoint, usb_ms->out_endpoint);

    if (!usb_device_request(usb_device,
                            USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                            USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_MASS_STORAGE_REQUEST_RESET,
                            0, usb_ms->interface_number, 0, NULL)) {
        PRINTLOG(USB, LOG_ERROR, "cannot reset mass storage device");
        memory_free(usb_ms);

        return -1;
    }

    boolean_t ready = false;
    int8_t tries = 5;

    do {
        time_timer_spinsleep(1000);

        if (!usb_device_request(usb_device,
                                USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                                USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_MASS_STORAGE_REQUEST_GET_MAX_LUN,
                                0, usb_ms->interface_number, 1, &usb_ms->max_lun)) {
            PRINTLOG(USB, LOG_ERROR, "cannot get max lun of mass storage device");
        } else {
            ready = true;
        }


    }while(tries-- > 0 && !ready);

    if(!ready) {
        PRINTLOG(USB, LOG_ERROR, "cannot get max lun of mass storage device");
        memory_free(usb_ms);

        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "max lun: %d", usb_ms->max_lun);

    usb_ms->inquiry_data = memory_malloc(sizeof(scsi_standard_inquiry_data_t));

    if (!usb_ms->inquiry_data) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for inquiry data");
        memory_free(usb_ms);

        return -1;
    }

    scsi_command_inquiry_t inquiry = {0};
    inquiry.opcode = SCSI_COMMAND_OPCODE_INQUIRY;
    inquiry.allocation_length[0] = sizeof(scsi_standard_inquiry_data_t) >> 8;
    inquiry.allocation_length[1] = sizeof(scsi_standard_inquiry_data_t) & 0xFF;

    if (!usb_mass_storage_send_cbw(usb_ms, sizeof(scsi_standard_inquiry_data_t), USB_MASS_STORAGE_CBW_FLAG_DATA_IN, 0, sizeof(scsi_command_inquiry_t), (uint8_t*)&inquiry)) {
        PRINTLOG(USB, LOG_ERROR, "cannot send inquiry command to mass storage device");
        memory_free(usb_ms);

        return -1;
    }


    if(!usb_mass_storage_read_write(usb_ms, true, sizeof(scsi_standard_inquiry_data_t), (uint8_t*)usb_ms->inquiry_data)) {
        PRINTLOG(USB, LOG_ERROR, "cannot read inquiry data from mass storage device");
        memory_free(usb_ms->inquiry_data);
        memory_free(usb_ms);

        return -1;
    }


    if(!usb_mass_storage_get_csw(usb_ms)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get csw from mass storage device, inquiry failed");
        memory_free(usb_ms->inquiry_data);
        memory_free(usb_ms);

        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "inquiry data: 0x%x 0x%x 0x%x 0x%x 0x%x",
             usb_ms->inquiry_data->peripheral_device_type,
             usb_ms->inquiry_data->peripheral_qualifier,
             usb_ms->inquiry_data->version,
             usb_ms->inquiry_data->additional_length,
             usb_ms->inquiry_data->protect);


    scsi_command_test_unit_ready_t test_unit_ready = {0};
    test_unit_ready.opcode = SCSI_COMMAND_OPCODE_TEST_UNIT_READY;

    if (!usb_mass_storage_send_cbw(usb_ms, 0, USB_MASS_STORAGE_CBW_FLAG_NO_DATA, 0, sizeof(scsi_command_test_unit_ready_t), (uint8_t*)&test_unit_ready)) {
        PRINTLOG(USB, LOG_ERROR, "cannot send test unit ready command to mass storage device");
        memory_free(usb_ms->inquiry_data);
        memory_free(usb_ms);

        return -1;
    }

    if(!usb_mass_storage_get_csw(usb_ms)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get csw from mass storage device, test unit ready failed");
        memory_free(usb_ms->inquiry_data);
        memory_free(usb_ms);

        return -1;
    }

    scsi_command_read_capacity_16_t read_capacity_16 = {0};
    read_capacity_16.opcode = SCSI_COMMAND_OPCODE_READ_CAPACITY_16;
    read_capacity_16.allocation_length[3] = sizeof(scsi_capacity_16_t);

    if (!usb_mass_storage_send_cbw(usb_ms, sizeof(scsi_capacity_16_t), USB_MASS_STORAGE_CBW_FLAG_DATA_IN, 0, sizeof(scsi_command_read_capacity_16_t), (uint8_t*)&read_capacity_16)) {
        PRINTLOG(USB, LOG_ERROR, "cannot send read capacity 16 command to mass storage device");
        memory_free(usb_ms->inquiry_data);
        memory_free(usb_ms);

        return -1;
    }

    scsi_capacity_16_t capacity_16 = {0};

    if(!usb_mass_storage_read_write(usb_ms, true, sizeof(scsi_capacity_16_t), (uint8_t*)&capacity_16)) {
        PRINTLOG(USB, LOG_ERROR, "cannot read capacity 16 from mass storage device");
        memory_free(usb_ms->inquiry_data);
        memory_free(usb_ms);

        return -1;
    }

    if(!usb_mass_storage_get_csw(usb_ms)) {
        PRINTLOG(USB, LOG_ERROR, "may be 16 bytes command not supported, try 10 bytes command");
    } else {
        usb_ms->command_size_16_supported = true;

        uint64_t last_lba = BYTE_SWAP64(capacity_16.last_logical_block_address);
        uint32_t block_size = BYTE_SWAP32(capacity_16.logical_block_length);

        PRINTLOG(USB, LOG_DEBUG, "16 bytes command supported")

        usb_ms->lba_count = last_lba + 1;
        usb_ms->block_size = block_size;

    }



    scsi_command_read_capacity_10_t read_capacity_10 = {0};
    read_capacity_10.opcode = SCSI_COMMAND_OPCODE_READ_CAPACITY_10;

    if (!usb_mass_storage_send_cbw(usb_ms, sizeof(scsi_capacity_10_t), USB_MASS_STORAGE_CBW_FLAG_DATA_IN, 0, sizeof(scsi_command_read_capacity_10_t), (uint8_t*)&read_capacity_10)) {
        PRINTLOG(USB, LOG_ERROR, "cannot send read capacity 10 command to mass storage device");
        memory_free(usb_ms->inquiry_data);
        memory_free(usb_ms);

        return -1;
    }

    scsi_capacity_10_t capacity_10 = {0};

    if(!usb_mass_storage_read_write(usb_ms, true, sizeof(scsi_capacity_10_t), (uint8_t*)&capacity_10)) {
        PRINTLOG(USB, LOG_ERROR, "cannot read capacity 10 from mass storage device");
        memory_free(usb_ms->inquiry_data);
        memory_free(usb_ms);

        return -1;
    }

    if(!usb_mass_storage_get_csw(usb_ms)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get csw from mass storage device, read capacity 10 failed");
        memory_free(usb_ms->inquiry_data);
        memory_free(usb_ms);

        return -1;
    }

    uint32_t last_lba = BYTE_SWAP32(capacity_10.last_logical_block_address);
    uint32_t block_size = BYTE_SWAP32(capacity_10.logical_block_length);

    usb_ms->lba_count = last_lba + 1;
    usb_ms->block_size = block_size;


    PRINTLOG(USB, LOG_DEBUG, "capacity: 0x%llx, block size: 0x%x", usb_ms->lba_count, usb_ms->block_size);

    usb_ms->id = hashmap_size(usb_mass_storage_disks);

    hashmap_put(usb_mass_storage_disks, (void*)usb_ms->id, usb_ms);

    return 0;
}
#pragma GCC diagnostic pop

uint64_t usb_mass_storage_get_disk_count(void) {
    return hashmap_size(usb_mass_storage_disks);
}

usb_driver_t* usb_mass_storage_get_disk_by_id(uint64_t id) {
    return (usb_driver_t*)hashmap_get(usb_mass_storage_disks, (void*)id);
}

typedef struct usb_mass_storage_disk_impl_context_t {
    usb_driver_t* usb_mass_storage;
    uint64_t      block_size;
    uint8_t       lun;
} usb_mass_storage_disk_impl_context_t;

uint64_t usb_mass_storage_disk_impl_get_size(const disk_or_partition_t* d);
uint64_t usb_mass_storage_disk_impl_get_block_size(const disk_or_partition_t* d);
int8_t   usb_mass_storage_disk_impl_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data);
int8_t   usb_mass_storage_disk_impl_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data);
int8_t   usb_mass_storage_disk_impl_flush(const disk_or_partition_t* d);
int8_t   usb_mass_storage_disk_impl_close(const disk_or_partition_t* d);


uint64_t usb_mass_storage_disk_impl_get_size(const disk_or_partition_t* d){
    usb_mass_storage_disk_impl_context_t* ctx = (usb_mass_storage_disk_impl_context_t*)d->context;
    return ctx->usb_mass_storage->lba_count * ctx->block_size;
}

uint64_t usb_mass_storage_disk_impl_get_block_size(const disk_or_partition_t* d){
    usb_mass_storage_disk_impl_context_t* ctx = (usb_mass_storage_disk_impl_context_t*)d->context;
    return ctx->block_size;
}

int8_t usb_mass_storage_disk_impl_write(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t* data) {
    usb_mass_storage_disk_impl_context_t* ctx = (usb_mass_storage_disk_impl_context_t*)d->context;

    if(data == NULL) {
        return -1;
    }

    uint64_t buffer_len = ctx->block_size;
    uint8_t* tmp_data = data;

    for(uint64_t i = 0; i < count; i++) {
        uint8_t cbw_buffer[16] = {0};
        memory_memclean(cbw_buffer, 16);
        uint8_t cbw_buffer_len = 16;

        if(ctx->usb_mass_storage->command_size_16_supported) {
            scsi_command_write_16_t* write_16 = (scsi_command_write_16_t*)cbw_buffer;
            write_16->opcode = SCSI_COMMAND_OPCODE_WRITE_16;
            write_16->lba = BYTE_SWAP64(lba);
            write_16->transfer_length = BYTE_SWAP32(1);

        } else {
            cbw_buffer_len = 10;
            scsi_command_write_10_t* write_10 = (scsi_command_write_10_t*)cbw_buffer;
            write_10->opcode = SCSI_COMMAND_OPCODE_WRITE_10;
            write_10->lba = BYTE_SWAP32(lba);
            write_10->transfer_length = BYTE_SWAP16(1);
        }

        if(!usb_mass_storage_send_cbw(ctx->usb_mass_storage, ctx->block_size, USB_MASS_STORAGE_CBW_FLAG_DATA_OUT, ctx->lun, cbw_buffer_len, cbw_buffer)) {
            PRINTLOG(USB, LOG_ERROR, "Failed to send CBW for write command");

            return -1;
        }

        if(!usb_mass_storage_read_write(ctx->usb_mass_storage, false, ctx->block_size, tmp_data)) {
            PRINTLOG(USB, LOG_ERROR, "Failed to write data for write command");

            return -1;
        }

        if(!usb_mass_storage_get_csw(ctx->usb_mass_storage)) {
            PRINTLOG(USB, LOG_ERROR, "Failed to receive CSW for write command");

            return -1;
        }

        lba++;
        tmp_data += buffer_len;
    }


    return 0;
}

int8_t usb_mass_storage_disk_impl_read(const disk_or_partition_t* d, uint64_t lba, uint64_t count, uint8_t** data){
    usb_mass_storage_disk_impl_context_t* ctx = (usb_mass_storage_disk_impl_context_t*)d->context;

    uint64_t buffer_len = count * ctx->block_size;

    *data = memory_malloc_ext(NULL, buffer_len, 0x1000);

    if(*data == NULL) {
        return -1;
    }

    uint8_t* tmp_data = *data;

    for(uint64_t i = 0; i < count; i++) {
        uint8_t cbw_buffer[16] = {0};
        memory_memclean(cbw_buffer, 16);
        uint8_t cbw_buffer_len = 16;

        if(ctx->usb_mass_storage->command_size_16_supported) {
            scsi_command_read_16_t* read_16 = (scsi_command_read_16_t*)cbw_buffer;
            read_16->opcode = SCSI_COMMAND_OPCODE_READ_16;
            read_16->lba = BYTE_SWAP64(lba);
            read_16->transfer_length = BYTE_SWAP32(1);

        } else {
            cbw_buffer_len = 10;
            scsi_command_read_10_t* read_10 = (scsi_command_read_10_t*)cbw_buffer;
            read_10->opcode = SCSI_COMMAND_OPCODE_READ_10;
            read_10->lba = BYTE_SWAP32(lba);
            read_10->transfer_length = BYTE_SWAP16(1);
        }

        if(!usb_mass_storage_send_cbw(ctx->usb_mass_storage, ctx->block_size, USB_MASS_STORAGE_CBW_FLAG_DATA_IN, ctx->lun, cbw_buffer_len, cbw_buffer)) {
            PRINTLOG(USB, LOG_ERROR, "Failed to send CBW for read command");

            return -1;
        }

        if(!usb_mass_storage_read_write(ctx->usb_mass_storage, true, ctx->block_size, tmp_data)) {
            PRINTLOG(USB, LOG_ERROR, "Failed to read data for read command");

            return -1;
        }

        if(!usb_mass_storage_get_csw(ctx->usb_mass_storage)) {
            PRINTLOG(USB, LOG_ERROR, "Failed to receive CSW for read command");

            return -1;
        }


        lba++;
        tmp_data += ctx->block_size;
    }



    return 0;
}

int8_t usb_mass_storage_disk_impl_flush(const disk_or_partition_t* d) {
    usb_mass_storage_disk_impl_context_t* ctx = (usb_mass_storage_disk_impl_context_t*)d->context;

    uint8_t buffer[16] = {0};
    memory_memclean(buffer, 16);
    uint8_t buffer_len = 16;

    if(ctx->usb_mass_storage->command_size_16_supported) {
        scsi_command_sync_cache_16_t* sync_cache_16 = (scsi_command_sync_cache_16_t*)buffer;
        sync_cache_16->opcode = SCSI_COMMAND_OPCODE_SYNCHRONIZE_CACHE_16;
        sync_cache_16->immed = 1;
        sync_cache_16->lba = 0;
        sync_cache_16->number_of_blocks = BYTE_SWAP32(ctx->usb_mass_storage->lba_count);
    } else {
        buffer_len = 10;
        scsi_command_sync_cache_10_t* sync_cache = (scsi_command_sync_cache_10_t*)buffer;
        sync_cache->opcode = SCSI_COMMAND_OPCODE_SYNCHRONIZE_CACHE_10;
        sync_cache->immed = 1;
        sync_cache->lba = 0;
        sync_cache->number_of_blocks = BYTE_SWAP16(ctx->usb_mass_storage->lba_count);
    }

    if(!usb_mass_storage_send_cbw(ctx->usb_mass_storage, 0, USB_MASS_STORAGE_CBW_FLAG_NO_DATA, ctx->lun, buffer_len, buffer)) {
        PRINTLOG(USB, LOG_ERROR, "Failed to send CBW for sync cache command");

        return -1;
    }

    if(!usb_mass_storage_get_csw(ctx->usb_mass_storage)) {
        PRINTLOG(USB, LOG_ERROR, "Failed to receive CSW for sync cache command");

        return -1;
    }


    return 0;
}

int8_t usb_mass_storage_disk_impl_close(const disk_or_partition_t* d) {
    usb_mass_storage_disk_impl_context_t* ctx = (usb_mass_storage_disk_impl_context_t*)d->context;

    d->flush(d);

    memory_free(ctx);

    memory_free((void*)d);

    return 0;
}

disk_t* usb_mass_storage_disk_impl_open(usb_driver_t* usb_mass_storage, uint8_t lun) {

    if(usb_mass_storage == NULL) {
        return NULL;
    }

    usb_mass_storage_disk_impl_context_t* ctx = memory_malloc(sizeof(usb_mass_storage_disk_impl_context_t));

    if(ctx == NULL) {
        return NULL;
    }

    ctx->usb_mass_storage = usb_mass_storage;
    ctx->block_size = usb_mass_storage->block_size;
    ctx->lun = lun;

    disk_t* d = memory_malloc(sizeof(disk_t));

    if(d == NULL) {
        memory_free(ctx);

        return NULL;
    }

    d->disk.context = ctx;
    d->disk.get_size = usb_mass_storage_disk_impl_get_size;
    d->disk.get_block_size = usb_mass_storage_disk_impl_get_block_size;
    d->disk.write = usb_mass_storage_disk_impl_write;
    d->disk.read = usb_mass_storage_disk_impl_read;
    d->disk.flush = usb_mass_storage_disk_impl_flush;
    d->disk.close = usb_mass_storage_disk_impl_close;

    return d;
}
