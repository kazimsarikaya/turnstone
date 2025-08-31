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
#include <time/timer.h>
#include <utils.h>


MODULE("turnstone.kernel.hw.usb.mass_storage");

#define USB_UAS_PIPE_ID_COMMAND         0x01
#define USB_UAS_PIPE_ID_STATUS          0x02
#define USB_UAS_PIPE_ID_DATA_IN         0x03
#define USB_UAS_PIPE_ID_DATA_OUT        0x04

#define USB_UAS_UI_COMMAND              0x01
#define USB_UAS_UI_SENSE                0x03
#define USB_UAS_UI_RESPONSE             0x04
#define USB_UAS_UI_TASK_MGMT            0x05
#define USB_UAS_UI_READ_READY           0x06
#define USB_UAS_UI_WRITE_READY          0x07

typedef struct usb_uas_iu_header_t {
    uint8_t  id;
    uint8_t  reserved;
    uint16_t tag;
} __attribute__((packed)) usb_uas_iu_header_t;

typedef struct usb_uas_iu_command_t {
    uint8_t  prio_taskattr;
    uint8_t  reserved_1;
    uint8_t  add_cdb_length;
    uint8_t  reserved_2;
    uint64_t lun;
    uint8_t  cdb[16];
    uint8_t  add_cdb[1];
} __attribute__((packed)) usb_uas_iu_command_t;

typedef struct usb_uas_iu_sense_t {
    uint16_t status_qualifier;
    uint8_t  status;
    uint8_t  reserved[7];
    uint16_t sense_length;
    uint8_t  sense_data[18];
} __attribute__((packed)) usb_uas_iu_sense_t;

typedef struct usb_uas_iu_response_t {
    uint8_t add_response_info[3];
    uint8_t response_code;
} __attribute__((packed)) usb_uas_iu_response_t;

typedef struct usb_uas_iu_task_mgmt_t {
    uint8_t  function;
    uint8_t  reserved;
    uint16_t task_tag;
    uint64_t lun;
} __attribute__((packed)) usb_uas_iu_task_mgmt_t;

typedef struct usb_uas_iu_t {
    usb_uas_iu_header_t hdr;
    union {
        usb_uas_iu_command_t   command;
        usb_uas_iu_sense_t     sense;
        usb_uas_iu_task_mgmt_t task;
        usb_uas_iu_response_t  response;
    };
} __attribute__((packed)) usb_uas_iu_t;

typedef struct usb_driver_t {
    usb_device_t *                device;
    usb_interface_t*              interface;
    usb_pipeline_callback_f       pipeline_callback;
    uint32_t                      expected_packet_size;
    uint64_t                      id;
    boolean_t                     is_uas;
    boolean_t                     command_size_16_supported;
    uint32_t                      max_lun;
    uint64_t                      lba_count;
    uint32_t                      block_size;
    scsi_standard_inquiry_data_t* inquiry_data;
    usb_config_t*                 config;
    uint8_t                       interface_number;
    uint8_t                       cmd_endpoint;
    uint8_t                       status_endpoint;
    uint8_t                       in_endpoint;
    uint8_t                       out_endpoint;
    uint32_t                      command_tag;
    lock_t*                       lock;
    boolean_t                     is_uas_cmd_sended;
    usb_uas_iu_t*                 async_iu;
} usb_driver_t;


boolean_t usb_ms_uas_read_write(usb_driver_t* usb_driver, boolean_t read, uint32_t dtl, uint8_t* data) {
    usb_transfer_t ut = {0};

    uint32_t stream_id = 1;

    ut.driver = usb_driver;

    if(read) {
        ut.endpoint = usb_driver->interface->endpoints[usb_driver->in_endpoint];
    } else {
        ut.endpoint = usb_driver->interface->endpoints[usb_driver->out_endpoint];
    }


    ut.length = dtl;
    ut.data = data;
    ut.stream_id = stream_id;

    int8_t res =  usb_driver->device->controller->bulk_transfer(usb_driver->device->controller, &ut);

    if(res != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot %s from mass storage device", read ? "read" : "write");
        lock_release(usb_driver->lock);

        return false;
    }

    return ut.complete && ut.success;
}

boolean_t usb_ms_uas_send_command(usb_driver_t* usb_driver, uint32_t dtl, uint8_t flags, uint8_t lun, uint8_t command_length, uint8_t* command) {
    UNUSED(dtl);
    UNUSED(flags);

    lock_acquire(usb_driver->lock);

    uint8_t* data = NULL;
    uint32_t length = 0;
    uint32_t stream_id = 1;
    usb_driver->command_tag = BYTE_SWAP16(stream_id); // FIXME: UAS does not use tags?
    usb_uas_iu_t iu = {0};
    iu.hdr.id = USB_UAS_UI_COMMAND;
    iu.hdr.tag = usb_driver->command_tag;
    iu.command.lun = lun;
    iu.command.add_cdb_length = 0;
    memory_memcopy(command, &iu.command.cdb, command_length);
    data = (uint8_t*)&iu;
    length = sizeof(usb_uas_iu_header_t) + sizeof(usb_uas_iu_command_t) + 1;


    usb_transfer_t ut = {0};

    ut.driver = usb_driver;
    ut.endpoint = usb_driver->interface->endpoints[usb_driver->cmd_endpoint];
    ut.length = length;
    ut.data = data;
    ut.stream_id = stream_id;

    int8_t res =  usb_driver->device->controller->bulk_transfer(usb_driver->device->controller, &ut);

    if(res != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot send command to mass storage device");
        lock_release(usb_driver->lock);

        return false;
    }

    usb_driver->is_uas_cmd_sended = true;

    return ut.complete && ut.success;
}

boolean_t usb_ms_uas_get_status(usb_driver_t* usb_driver) {
    usb_transfer_t ut = {0};

    uint8_t* data = NULL;
    uint32_t length = 0;

    uint32_t stream_id = 1;

    usb_uas_iu_t iu = {0};

    boolean_t is_async = false;

    iu.hdr.id = USB_UAS_UI_RESPONSE;
    iu.hdr.tag = usb_driver->command_tag;
    data = (uint8_t*)&iu;
    length = sizeof(usb_uas_iu_t); // sizeof(usb_uas_iu_header_t) + sizeof(usb_uas_iu_response_t);
    is_async = !usb_driver->is_uas_cmd_sended;
    usb_driver->is_uas_cmd_sended = false;

    if(is_async) {
        usb_driver->async_iu = memory_malloc(sizeof(usb_uas_iu_t));
        if(!usb_driver->async_iu) {
            PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for async iu");
            lock_release(usb_driver->lock);

            return false;
        }
        data = (uint8_t*)usb_driver->async_iu;
    }

    ut.driver = usb_driver;
    ut.endpoint = usb_driver->interface->endpoints[usb_driver->status_endpoint];
    ut.length = length;
    ut.data = data;
    ut.stream_id = stream_id;
    ut.is_async = is_async;

    int8_t res =  usb_driver->device->controller->bulk_transfer(usb_driver->device->controller, &ut);

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

    if(ut.is_async) {
        PRINTLOG(USB, LOG_TRACE, "async iu received");
        lock_release(usb_driver->lock);

        return true;
    }


    if(iu.hdr.tag != usb_driver->command_tag) {
        PRINTLOG(USB, LOG_ERROR, "invalid iu tag: 0x%x != 0x%x", iu.hdr.tag, usb_driver->command_tag);
        lock_release(usb_driver->lock);

        return false;
    }

    if(iu.hdr.id != USB_UAS_UI_RESPONSE) {
        if(iu.hdr.id == USB_UAS_UI_SENSE) {
            if(iu.sense.status != 0 || iu.sense.sense_length != 0){
                PRINTLOG(USB, LOG_TRACE, "sense status: 0x%x", iu.sense.status);
                PRINTLOG(USB, LOG_TRACE, "sense length: 0x%llx", BYTE_SWAP16(iu.sense.sense_length));
                PRINTLOG(USB, LOG_TRACE, "status qualifier: 0x%llx", BYTE_SWAP16(iu.sense.status_qualifier));
                PRINTLOG(USB, LOG_TRACE, "sense data: ");

                for(uint32_t i = 0; i < sizeof(iu.sense.sense_data); i++) {
                    PRINTLOG(USB, LOG_TRACE, "0x%02x ", iu.sense.sense_data[i]);
                }

                lock_release(usb_driver->lock);

                return false;

            }
        } else {
            PRINTLOG(USB, LOG_ERROR, "invalid iu id: 0x%x", iu.hdr.id);
            lock_release(usb_driver->lock);

            return false;
        }
    } else {
        if(iu.response.response_code != 0) {
            PRINTLOG(USB, LOG_ERROR, "invalid iu response code: 0x%x", iu.response.response_code);
            lock_release(usb_driver->lock);

            return false;
        }
    }

    PRINTLOG(USB, LOG_TRACE, "iu received");


    lock_release(usb_driver->lock);

    return true;
}

usb_driver_t* usb_ms_uas_init(usb_device_t * usb_device, usb_interface_t* interface)
{
    usb_driver_t* usb_ms = memory_malloc(sizeof(usb_driver_t));

    if (!usb_ms) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for mass storage device");

        return NULL;
    }

    usb_ms->device = usb_device;
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
    usb_ms->is_uas = true;

    usb_ms->interface_number = interface->desc->interface_number;

    for(uint32_t i = 0; i < interface->num_endpoints; i++) {
        if(!interface->endpoints[0]->num_cs_interfaces || !interface->endpoints[i]->cs_interfaces || !interface->endpoints[i]->endpoint_companion) {
            PRINTLOG(USB, LOG_ERROR, "invalid endpoint companion or cs interface for uas");
            memory_free(usb_ms);

            return NULL;
        }

        switch(interface->endpoints[i]->cs_interfaces[0]->interface_number) {
        case USB_UAS_PIPE_ID_COMMAND:
            usb_ms->cmd_endpoint = i;
            break;
        case USB_UAS_PIPE_ID_STATUS:
            usb_ms->status_endpoint = i;
            break;
        case USB_UAS_PIPE_ID_DATA_IN:
            usb_ms->in_endpoint = i;
            break;
        case USB_UAS_PIPE_ID_DATA_OUT:
            usb_ms->out_endpoint = i;
            break;
        default:
            PRINTLOG(USB, LOG_ERROR, "unknown uas pipe id: 0x%x", interface->endpoints[i]->cs_interfaces[0]->interface_number);
            memory_free(usb_ms);

            return NULL;
        }
    }

    PRINTLOG(USB, LOG_DEBUG, "cmd endpoint: 0x%x status endpoint 0x%x", usb_ms->cmd_endpoint, usb_ms->status_endpoint);
    PRINTLOG(USB, LOG_DEBUG, "in endpoint: 0x%x out endpoint 0x%x", usb_ms->in_endpoint, usb_ms->out_endpoint);


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

    if(!usb_ms_get_status(usb_ms)) {
        PRINTLOG(USB, LOG_ERROR, "cannot send pre-status for may failed capacity 16 command");
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

    if(!usb_ms->async_iu) {
        PRINTLOG(USB, LOG_ERROR, "async iu is NULL");
        memory_free(usb_ms->inquiry_data);
        memory_free(usb_ms);
        return NULL;
    }

    int64_t timeout = 1000;

    while(timeout-- > 0 && usb_ms->async_iu && !usb_ms->async_iu->hdr.id) {
        time_timer_spinsleep(1000);
    }

    if(timeout <= 0) {
        PRINTLOG(USB, LOG_TRACE, "no async iu received. 16 bytes command may be supported");
        memory_free(usb_ms->async_iu);
    } else {
        PRINTLOG(USB, LOG_TRACE, "async iu received. 16 bytes command not supported");
        memory_free(usb_ms->async_iu);
        usb_ms->async_iu = NULL;
        usb_ms->command_size_16_supported = false;
    }


    if(usb_ms->command_size_16_supported) {
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
