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


MODULE("turnstone.kernel.hw.usb.mass_storage");

typedef struct usb_driver_t {
    usb_device_t *                device;
    uint8_t                       interface_number;
    uint8_t                       in_endpoint;
    uint8_t                       out_endpoint;
    uint32_t                      max_lun;
    uint32_t                      cbw_tag;
    uint64_t                      lba_count;
    uint32_t                      block_size;
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

        return false;
    }

    future_get_data_and_destroy(ut.transfer_future);


    return ut.complete && ut.success;
}

boolean_t usb_mass_storage_send_cbw(usb_driver_t* usb_driver, uint32_t dtl, uint8_t flags, uint8_t lun, uint8_t command_length, uint8_t* command) {
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

        return false;
    }

    future_get_data_and_destroy(ut.transfer_future);

    if(!ut.complete || !ut.success) {
        PRINTLOG(USB, LOG_ERROR, "cannot get csw from mass storage device");

        return false;
    }

    if(csw.signature != USB_MASS_STORAGE_CSW_SIGNATURE) {
        PRINTLOG(USB, LOG_ERROR, "invalid csw signature: 0x%x != 0x%x", csw.signature, USB_MASS_STORAGE_CSW_SIGNATURE);

        return false;
    }

    if(csw.tag != usb_driver->cbw_tag) {
        PRINTLOG(USB, LOG_ERROR, "invalid csw tag: 0x%x != 0x%x", csw.tag, usb_driver->cbw_tag);

        return false;
    }

    if(csw.status != 0) {
        PRINTLOG(USB, LOG_ERROR, "invalid csw status: 0x%x", csw.status);

        return false;
    }

    PRINTLOG(USB, LOG_INFO, "csw received. data residue: %d", csw.data_residue);

    return true;
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t usb_mass_storage_init(usb_device_t * usb_device)
{
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

    boolean_t protect = usb_ms->inquiry_data->protect & 0x01;

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

    if(protect) {
        scsi_command_read_capacity_16_t read_capacity_16 = {0};
        read_capacity_16.opcode = SCSI_COMMAND_OPCODE_READ_CAPACITY_16;
        read_capacity_16.allocation_length[3] = sizeof(scsi_capacity_16_t);

        if (!usb_mass_storage_send_cbw(usb_ms, sizeof(scsi_capacity_16_t), USB_MASS_STORAGE_CBW_FLAG_DATA_IN, 0, sizeof(scsi_command_read_capacity_16_t), (uint8_t*)&read_capacity_16)) {
            PRINTLOG(USB, LOG_ERROR, "cannot send read capacity command to mass storage device");
            memory_free(usb_ms->inquiry_data);
            memory_free(usb_ms);

            return -1;
        }

        scsi_capacity_16_t capacity = {0};

        if(!usb_mass_storage_read_write(usb_ms, true, sizeof(scsi_capacity_16_t), (uint8_t*)&capacity)) {
            PRINTLOG(USB, LOG_ERROR, "cannot read capacity from mass storage device");
            memory_free(usb_ms->inquiry_data);
            memory_free(usb_ms);

            return -1;
        }

        if(!usb_mass_storage_get_csw(usb_ms)) {
            PRINTLOG(USB, LOG_ERROR, "cannot get csw from mass storage device, read capacity failed");
            memory_free(usb_ms->inquiry_data);
            memory_free(usb_ms);

            return -1;
        }

        uint64_t last_lba = BYTE_SWAP64(capacity.last_logical_block_address);
        uint32_t block_size = BYTE_SWAP32(capacity.logical_block_length);

        PRINTLOG(USB, LOG_DEBUG, "capacity: 0x%llx 0x%x", last_lba, block_size);

        usb_ms->lba_count = last_lba + 1;
        usb_ms->block_size = block_size;
    } else {
        scsi_command_read_capacity_10_t read_capacity_10 = {0};
        read_capacity_10.opcode = SCSI_COMMAND_OPCODE_READ_CAPACITY_10;

        if (!usb_mass_storage_send_cbw(usb_ms, sizeof(scsi_capacity_10_t), USB_MASS_STORAGE_CBW_FLAG_DATA_IN, 0, sizeof(scsi_command_read_capacity_10_t), (uint8_t*)&read_capacity_10)) {
            PRINTLOG(USB, LOG_ERROR, "cannot send read capacity command to mass storage device");
            memory_free(usb_ms->inquiry_data);
            memory_free(usb_ms);

            return -1;
        }

        scsi_capacity_10_t capacity = {0};

        if(!usb_mass_storage_read_write(usb_ms, true, sizeof(scsi_capacity_10_t), (uint8_t*)&capacity)) {
            PRINTLOG(USB, LOG_ERROR, "cannot read capacity from mass storage device");
            memory_free(usb_ms->inquiry_data);
            memory_free(usb_ms);

            return -1;
        }

        if(!usb_mass_storage_get_csw(usb_ms)) {
            PRINTLOG(USB, LOG_ERROR, "cannot get csw from mass storage device, read capacity failed");
            memory_free(usb_ms->inquiry_data);
            memory_free(usb_ms);

            return -1;
        }

        uint32_t last_lba = BYTE_SWAP32(capacity.last_logical_block_address);
        uint32_t block_size = BYTE_SWAP32(capacity.logical_block_length);

        PRINTLOG(USB, LOG_DEBUG, "capacity: 0x%x 0x%x", last_lba, block_size);

        usb_ms->lba_count = last_lba + 1;
        usb_ms->block_size = block_size;
    }

    return 0;
}
#pragma GCC diagnostic pop
