/**
 * @file usb_mass_storage.64.c
 * @brief USB Mass Storage Device Class (MSC) driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#define ___USB_MASS_STORAGE_IMPLEMENTATION
#include <driver/usb_ms.h>
#include <logging.h>
#include <random.h>
#include <time/timer.h>
#include <cpu/sync.h>

MODULE("turnstone.kernel.hw.usb.mass_storage");

#define USB_MASS_STORAGE_REQUEST_GET_MAX_LUN 0xFE
#define USB_MASS_STORAGE_REQUEST_RESET       0xFF

#define USB_MASS_STORAGE_CBW_SIGNATURE 0x43425355
#define USB_MASS_STORAGE_CSW_SIGNATURE 0x53425355

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

typedef struct usb_driver_t {
    USB_DRIVER_COMMON_FIELDS
    uint64_t                      id;
    boolean_t                     is_uas;
    boolean_t                     command_size_16_supported;
    uint32_t                      max_lun;
    uint64_t                      lba_count;
    uint32_t                      block_size;
    scsi_standard_inquiry_data_t* inquiry_data;
    usb_config_t*                 config;
    uint8_t                       interface_number;
    uint8_t                       in_endpoint;
    uint8_t                       out_endpoint;
    uint32_t                      command_tag;
    lock_t*                       lock;
} usb_driver_t;


boolean_t usb_ms_bulk_only_read_write(usb_driver_t* usb_driver, boolean_t read, uint32_t dtl, uint8_t* data) {
    usb_transfer_t ut = {0};

    uint32_t stream_id = 0;

    if(usb_driver->is_uas) {
        stream_id = 1;
    }

    ut.driver = usb_driver;

    if(read) {
        ut.endpoint = usb_driver->interface->endpoints[usb_driver->in_endpoint];
    } else {
        ut.endpoint = usb_driver->interface->endpoints[usb_driver->out_endpoint];
    }


    ut.length = dtl;
    ut.data = data;
    ut.stream_id = stream_id;

    int8_t res =  usb_driver->usb_device->controller->data_transfer(usb_driver->usb_device->controller, &ut);

    if(res != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot %s from mass storage device", read ? "read" : "write");
        lock_release(usb_driver->lock);

        return false;
    }

    return ut.complete && ut.success;
}

boolean_t usb_ms_bulk_only_send_command(usb_driver_t* usb_driver, uint32_t dtl, uint8_t flags, uint8_t lun, uint8_t command_length, uint8_t* command) {
    lock_acquire(usb_driver->lock);

    usb_driver->command_tag = rand();


    usb_mass_storage_cbw_t cbw = {0};
    cbw.signature = USB_MASS_STORAGE_CBW_SIGNATURE;
    cbw.tag = usb_driver->command_tag;
    cbw.data_transfer_length = dtl;
    cbw.flags = flags;
    cbw.lun = lun;
    cbw.command_length = command_length;
    memory_memcopy(command, &cbw.command, command_length);

    usb_transfer_t ut = {0};

    ut.driver = usb_driver;
    ut.endpoint = usb_driver->interface->endpoints[usb_driver->out_endpoint];
    ut.length = sizeof(usb_mass_storage_cbw_t);
    ut.data = (uint8_t*)&cbw;

    int8_t res =  usb_driver->usb_device->controller->data_transfer(usb_driver->usb_device->controller, &ut);

    if(res != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot send command to mass storage device");
        lock_release(usb_driver->lock);

        return false;
    }

    return ut.complete && ut.success;
}

boolean_t usb_ms_bulk_only_get_status(usb_driver_t* usb_driver) {
    usb_transfer_t ut = {0};

    usb_mass_storage_csw_t csw = {0};

    ut.driver = usb_driver;
    ut.endpoint = usb_driver->interface->endpoints[usb_driver->in_endpoint];
    ut.length = sizeof(usb_mass_storage_csw_t);
    ut.data = (uint8_t*)&csw;

    int8_t res =  usb_driver->usb_device->controller->data_transfer(usb_driver->usb_device->controller, &ut);

    if(res != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot get status from mass storage device");
        lock_release(usb_driver->lock);

        return false;
    }

    if(!ut.complete || !ut.success) {
        PRINTLOG(USB, LOG_ERROR, "cannot get status from mass storage device");
        lock_release(usb_driver->lock);

        return false;
    }


    if(csw.signature != USB_MASS_STORAGE_CSW_SIGNATURE) {
        PRINTLOG(USB, LOG_ERROR, "invalid csw signature: 0x%x != 0x%x", csw.signature, USB_MASS_STORAGE_CSW_SIGNATURE);
        lock_release(usb_driver->lock);

        return false;
    }

    if(csw.tag != usb_driver->command_tag) {
        PRINTLOG(USB, LOG_ERROR, "invalid csw tag: 0x%x != 0x%x", csw.tag, usb_driver->command_tag);
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

usb_driver_t* usb_ms_bulk_only_init(usb_device_t * usb_device, usb_interface_t* interface)
{
    usb_driver_t* usb_ms = memory_malloc(sizeof(usb_driver_t));

    if (!usb_ms) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for mass storage device");

        return NULL;
    }

    usb_ms->usb_device = usb_device;
    usb_ms->interface = interface;
    interface->driver = usb_ms;

    usb_ms->lock = lock_create();

    if(!usb_ms->lock) {
        PRINTLOG(USB, LOG_ERROR, "cannot create lock for mass storage device");
        memory_free(usb_ms);

        return NULL;
    }

    usb_config_t* config = usb_device->configurations[usb_device->selected_config];

    usb_ms->config = config;

    usb_ms->interface_number = interface->desc->interface_number;


    if(interface->endpoints[0]->in) {
        usb_ms->in_endpoint = 0;
        usb_ms->out_endpoint = 1;
    } else {
        usb_ms->out_endpoint = 0;
        usb_ms->in_endpoint = 1;
    }


    PRINTLOG(USB, LOG_DEBUG, "in endpoint: 0x%x out endpoint 0x%x", usb_ms->in_endpoint, usb_ms->out_endpoint);

    if (!usb_device_request(usb_device,
                            NULL,
                            USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                            USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_MASS_STORAGE_REQUEST_RESET,
                            0, usb_ms->interface_number, 0, NULL)) {
        PRINTLOG(USB, LOG_ERROR, "cannot reset mass storage device");
        memory_free(usb_ms);

        return NULL;
    }

    boolean_t ready = false;
    int8_t tries = 5;

    do {
        time_timer_spinsleep(1000);

        if (!usb_device_request(usb_device,
                                NULL,
                                USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                                USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_MASS_STORAGE_REQUEST_GET_MAX_LUN,
                                0, usb_ms->interface_number, 1, &usb_ms->max_lun)) {
            PRINTLOG(USB, LOG_ERROR, "cannot get max lun of mass storage device");
        } else {
            ready = true;
        }


    }while(tries-- > 0 && !ready);

    if(!ready) {
        PRINTLOG(USB, LOG_ERROR, "cannot get max lun of mass storage device");
        memory_free(usb_ms);

        return NULL;
    }

    PRINTLOG(USB, LOG_DEBUG, "max lun: %d", usb_ms->max_lun);

    if(usb_ms_inquiry(usb_ms) != 0) {
        memory_free(usb_ms);

        return NULL;
    }

    if(usb_ms_test_unit_ready_with_retry(usb_ms) != 0) {
        memory_free(usb_ms->inquiry_data);
        memory_free(usb_ms);

        return NULL;
    }

    scsi_command_read_capacity_16_t read_capacity_16 = {0};
    read_capacity_16.opcode = SCSI_COMMAND_OPCODE_READ_CAPACITY_16;
    read_capacity_16.allocation_length[3] = sizeof(scsi_capacity_16_t);

    if (!usb_ms_send_command(usb_ms, sizeof(scsi_capacity_16_t), USB_MS_FLAG_DATA_IN, 0, sizeof(scsi_command_read_capacity_16_t), (uint8_t*)&read_capacity_16)) {
        PRINTLOG(USB, LOG_ERROR, "cannot send read capacity 16 command to mass storage device");
        memory_free(usb_ms->inquiry_data);
        memory_free(usb_ms);

        return NULL;
    }


    scsi_capacity_16_t capacity_16 = {0};

    if(!usb_ms_read_write(usb_ms, true, sizeof(scsi_capacity_16_t), (uint8_t*)&capacity_16)) {
        PRINTLOG(USB, LOG_ERROR, "cannot read capacity 16 from mass storage device");
        PRINTLOG(USB, LOG_DEBUG, "try 10 bytes command");
        usb_ms->command_size_16_supported = false;
    } else {
        if(!usb_ms_get_status(usb_ms)) {
            PRINTLOG(USB, LOG_ERROR, "may be 16 bytes command not supported, try 10 bytes command");
            usb_ms->command_size_16_supported = false;
        } else {
            usb_ms->command_size_16_supported = true;

            uint64_t last_lba = BYTE_SWAP64(capacity_16.last_logical_block_address);
            uint32_t block_size = BYTE_SWAP32(capacity_16.logical_block_length);

            PRINTLOG(USB, LOG_DEBUG, "16 bytes command supported");

            usb_ms->lba_count = last_lba + 1;
            usb_ms->block_size = block_size;

        }
    }

    if(!usb_ms->command_size_16_supported)
    {
        PRINTLOG(USB, LOG_DEBUG, "16 bytes command not supported, use 10 bytes command");

        scsi_command_read_capacity_10_t read_capacity_10 = {0};
        read_capacity_10.opcode = SCSI_COMMAND_OPCODE_READ_CAPACITY_10;

        if (!usb_ms_send_command(usb_ms, sizeof(scsi_capacity_10_t), USB_MS_FLAG_DATA_IN, 0, sizeof(scsi_command_read_capacity_10_t), (uint8_t*)&read_capacity_10)) {
            PRINTLOG(USB, LOG_ERROR, "cannot send read capacity 10 command to mass storage device");
            memory_free(usb_ms->inquiry_data);
            memory_free(usb_ms);

            return NULL;
        }

        scsi_capacity_10_t capacity_10 = {0};

        if(!usb_ms_read_write(usb_ms, true, sizeof(scsi_capacity_10_t), (uint8_t*)&capacity_10)) {
            PRINTLOG(USB, LOG_ERROR, "cannot read capacity 10 from mass storage device");
            memory_free(usb_ms->inquiry_data);
            memory_free(usb_ms);

            return NULL;
        }

        if(!usb_ms_get_status(usb_ms)) {
            PRINTLOG(USB, LOG_ERROR, "cannot get csw from mass storage device, read capacity 10 failed");
            memory_free(usb_ms->inquiry_data);
            memory_free(usb_ms);

            return NULL;
        }

        uint32_t last_lba = BYTE_SWAP32(capacity_10.last_logical_block_address);
        uint32_t block_size = BYTE_SWAP32(capacity_10.logical_block_length);

        usb_ms->lba_count = last_lba + 1;
        usb_ms->block_size = block_size;
    }

    PRINTLOG(USB, LOG_DEBUG, "capacity: 0x%llx, block size: 0x%x", usb_ms->lba_count, usb_ms->block_size);


    return usb_ms;
}
