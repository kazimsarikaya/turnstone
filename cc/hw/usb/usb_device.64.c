/**
 * @file usb_device.64.c
 * @brief USB device driver
 */

#include <driver/usb.h>
#include <hashmap.h>
#include <video.h>
#include <time/timer.h>
#include <strings.h>

MODULE("turnstone.kernel.hw.usb");

boolean_t usb_device_request(usb_device_t*           usb_device,
                             usb_request_type_t      request_type,
                             usb_request_recipient_t request_recipient,
                             usb_request_direction_t request_direction,
                             uint32_t                request,
                             uint16_t                value,
                             uint16_t                index,
                             uint16_t                length,
                             void*                   data);
void      usb_device_print_desc(usb_device_desc_t device_desc);
boolean_t usb_device_get_langs(usb_device_t* usb_device, wchar_t* langs);
boolean_t usb_device_get_string(usb_device_t* usb_device, wchar_t lang_id, uint32_t str_index, wchar_t* str);

hashmap_t* usb_devices = NULL;


void usb_device_print_desc(usb_device_desc_t device_desc) {
    PRINTLOG(USB, LOG_TRACE, "device class: %x", device_desc.device_class);
    PRINTLOG(USB, LOG_TRACE, "device subclass: %x", device_desc.device_subclass);
    PRINTLOG(USB, LOG_TRACE, "device protocol: %x", device_desc.device_protocol);
    PRINTLOG(USB, LOG_TRACE, "max packet size: %d", device_desc.max_packet_size);
    PRINTLOG(USB, LOG_TRACE, "vendor id: %x", device_desc.vendor_id);
    PRINTLOG(USB, LOG_TRACE, "product id: %x", device_desc.product_id);
    PRINTLOG(USB, LOG_TRACE, "number of configuration: %x", device_desc.num_configurations);
    PRINTLOG(USB, LOG_TRACE, "type: %x", device_desc.type);
    PRINTLOG(USB, LOG_TRACE, "length: %d", device_desc.length);
}


boolean_t usb_device_get_langs(usb_device_t* usb_device, wchar_t* langs) {
    langs[0] = 0;

    wchar_t buf[128] = {0};

    usb_string_desc_t* string_desc = (usb_string_desc_t*)buf;

    if (!usb_device_request(usb_device,
                            USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                            USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                            (USB_BASE_DESC_TYPE_STRING << 8) | 0, 0,
                            1, string_desc)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get string descriptor length");

        return false;
    }

    if (!usb_device_request(usb_device,
                            USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                            USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                            (USB_BASE_DESC_TYPE_STRING << 8) | 0, 0,
                            string_desc->length, string_desc)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get string descriptor");

        return false;
    }

    memory_memcopy(string_desc->string, langs, string_desc->length);

    return true;
}

boolean_t usb_device_get_string(usb_device_t* usb_device, wchar_t lang_id, uint32_t str_index, wchar_t* str) {

    str[0] = '\0';

    if (!str_index) {
        return false;
    }

    wchar_t buf[128] = {0};

    usb_string_desc_t* string_desc = (usb_string_desc_t*)buf;

    if (!usb_device_request(usb_device,
                            USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                            USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                            (USB_BASE_DESC_TYPE_STRING << 8) | str_index, lang_id,
                            1, string_desc)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get string descriptor length");

        return false;
    }

    if (!usb_device_request(usb_device,
                            USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                            USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                            (USB_BASE_DESC_TYPE_STRING << 8) | str_index, lang_id,
                            string_desc->length, string_desc)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get string descriptor");

        return false;
    }

    memory_memcopy(string_desc->string, str, string_desc->length);

    return true;
}

boolean_t usb_device_request(usb_device_t*           usb_device,
                             usb_request_type_t      request_type,
                             usb_request_recipient_t request_recipient,
                             usb_request_direction_t request_direction,
                             uint32_t                request,
                             uint16_t                value,
                             uint16_t                index,
                             uint16_t                length,
                             void*                   data) {

    usb_device_request_t usb_request = {0};

    usb_request.type = request_type | request_recipient | request_direction;
    usb_request.request = request;
    usb_request.value  = value;
    usb_request.index = index;
    usb_request.length = length;

    usb_transfer_t usb_transfer = {0};

    usb_transfer.device = usb_device;
    usb_transfer.endpoint = usb_device->endpoint;
    usb_transfer.request = &usb_request;
    usb_transfer.data = data;
    usb_transfer.length = length;

    if(usb_device->controller->control_transfer(usb_device->controller, &usb_transfer) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot send request");

        return false;
    }

    return usb_transfer.success;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
int8_t usb_device_init(usb_device_t* parent, usb_controller_t* controller, uint32_t port, uint32_t speed) {
    if(usb_devices == NULL) {
        usb_devices = hashmap_integer(64);

        if(!usb_devices) {
            PRINTLOG(USB, LOG_ERROR, "cannot create hashmap");

            return -1;
        }
    }

    usb_device_t* usb_device = memory_malloc(sizeof(usb_device_t));

    if(!usb_device) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb device");
        memory_free(usb_device);

        return -1;
    }

    usb_device->parent = parent;
    usb_device->controller = controller;
    usb_device->port = port;
    usb_device->speed = speed;
    usb_device->device_id = hashmap_size(usb_devices);

    hashmap_put(usb_devices, (void*)usb_device->device_id, usb_device);

    usb_device->max_packet_size = 8;

    usb_device_desc_t device_desc = {0};

    if(!usb_device_request(usb_device,
                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                           USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                           USB_BASE_DESC_TYPE_DEVICE << 8, 0,
                           8, &device_desc)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get device descriptor");
        hashmap_delete(usb_devices, (void*)usb_device->device_id);
        memory_free(usb_device);

        return -1;
    }

    PRINTLOG(USB, LOG_TRACE, "max packet size: %d", device_desc.max_packet_size);

    usb_device->max_packet_size = device_desc.max_packet_size;

    if(!usb_device_request(usb_device,
                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                           USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_SET_ADDRESS,
                           usb_device->device_id + 1, 0,
                           0, 0)) {
        PRINTLOG(USB, LOG_ERROR, "cannot set device address");
        hashmap_delete(usb_devices, (void*)usb_device->device_id);
        memory_free(usb_device);

        return -1;
    }

    usb_device->address = usb_device->device_id + 1;

    PRINTLOG(USB, LOG_TRACE, "device address: %x", usb_device->address);

    time_timer_spinsleep(5000);


    if(!usb_device_request(usb_device,
                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                           USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                           USB_BASE_DESC_TYPE_DEVICE << 8, 0,
                           sizeof(usb_device_desc_t), &device_desc)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get device descriptor");
        hashmap_delete(usb_devices, (void*)usb_device->device_id);
        memory_free(usb_device);

        return -1;
    }

    usb_device_print_desc(device_desc);

    wchar_t lang_ids[128] = {0};

    if(!usb_device_get_langs(usb_device, lang_ids)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get langs");
        hashmap_delete(usb_devices, (void*)usb_device->device_id);
        memory_free(usb_device);

        return -1;
    }

    wchar_t product[128] = {0};
    wchar_t vendor[128] = {0};
    wchar_t serial[128] = {0};

    if(!usb_device_get_string(usb_device, lang_ids[0], device_desc.product_string, product)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get product string");
    } else {
        char_t* product_str = wchar_to_char(product);
        PRINTLOG(USB, LOG_INFO, "device product: %s", product_str);
        usb_device->product = product_str;
    }

    if(!usb_device_get_string(usb_device, lang_ids[0], device_desc.vendor_string, vendor)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get vendor string");
    } else {
        char_t* vendor_str = wchar_to_char(vendor);
        PRINTLOG(USB, LOG_INFO, "device vendor: %s", vendor_str);
        usb_device->vendor = vendor_str;
    }

    if(!usb_device_get_string(usb_device, lang_ids[0], device_desc.serial_number_string, serial)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get serial string");
    } else {
        char_t* serial_str = wchar_to_char(serial);
        PRINTLOG(USB, LOG_INFO, "device serial: %s", serial_str);
        usb_device->serial = serial_str;
    }

    uint8_t* config_buffer = memory_malloc(256);

    if(!config_buffer) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for config buffer");
        hashmap_delete(usb_devices, (void*)usb_device->device_id);
        memory_free(usb_device->product);
        memory_free(usb_device->vendor);
        memory_free(usb_device->serial);
        memory_free(usb_device);

        return -1;
    }

    usb_endpoint_desc_t* endpoint_desc = NULL;
    usb_interface_desc_t* interface_desc = NULL;
    usb_hid_desc_t* hid_desc = NULL;
    int32_t picked_config = -1;
    int8_t found = 0;

    for(int32_t i = 0; i < device_desc.num_configurations; i++) {
        if(!usb_device_request(usb_device,
                               USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                               USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                               (USB_BASE_DESC_TYPE_CONFIG << 8) | i, 0,
                               4, config_buffer)) {
            PRINTLOG(USB, LOG_ERROR, "cannot get config descriptor");
            hashmap_delete(usb_devices, (void*)usb_device->device_id);
            memory_free(usb_device->product);
            memory_free(usb_device->vendor);
            memory_free(usb_device->serial);
            memory_free(usb_device);
            memory_free(config_buffer);

            return -1;
        }

        usb_config_desc_t* config_desc = (usb_config_desc_t*)config_buffer;

        PRINTLOG(USB, LOG_TRACE, "config descriptor total length: %d", config_desc->total_length);
        uint32_t total_length = config_desc->total_length;

        if(config_desc->total_length > 256) {
            memory_free(config_buffer);

            config_buffer = memory_malloc(total_length);

            if(!config_buffer) {
                PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for config buffer");
                hashmap_delete(usb_devices, (void*)usb_device->device_id);
                memory_free(usb_device->product);
                memory_free(usb_device->vendor);
                memory_free(usb_device->serial);
                memory_free(usb_device);

                return -1;
            }
        }

        if(!usb_device_request(usb_device,
                               USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                               USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                               (USB_BASE_DESC_TYPE_CONFIG << 8) | i, 0,
                               total_length, config_buffer)) {
            PRINTLOG(USB, LOG_ERROR, "cannot get config descriptor");
            hashmap_delete(usb_devices, (void*)usb_device->device_id);
            memory_free(usb_device->product);
            memory_free(usb_device->vendor);
            memory_free(usb_device->serial);
            memory_free(usb_device);
            memory_free(config_buffer);

            return -1;
        }

        config_desc = (usb_config_desc_t*)config_buffer;

        uint32_t idx = config_desc->length;
        found = 0;

        PRINTLOG(USB, LOG_TRACE, "config data between %d-%d", config_desc->length, config_desc->total_length);

        while(idx < total_length) {
            uint8_t length = config_buffer[idx];
            uint8_t type = config_buffer[idx + 1];

            if(type == USB_BASE_DESC_TYPE_INTERFACE) {
                interface_desc = (usb_interface_desc_t*)(config_buffer + idx);
                found++;
            } else if(type == USB_BASE_DESC_TYPE_ENDPOINT) {
                endpoint_desc = (usb_endpoint_desc_t*)(config_buffer + idx);
                found++;
            } else if(type == USB_HID_DESC_TYPE_HID) {
                hid_desc = (usb_hid_desc_t*)(config_buffer + idx);
            } else {
                PRINTLOG(USB, LOG_TRACE, "unknown descriptor type: %d length %d idx %d", type, length, idx);
            }

            idx += length;
        }

        if(found == 2) {
            picked_config = i;
            break;
        } else {
            hid_desc = NULL;
        }

    }

    if(found == 2) {

        if(!usb_device_request(usb_device,
                               USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                               USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_SET_CONFIGURATION,
                               picked_config, 0,
                               0, 0)) {
            PRINTLOG(USB, LOG_ERROR, "cannot set picked config 0x%x", picked_config);
            hashmap_delete(usb_devices, (void*)usb_device->device_id);
            memory_free(usb_device->product);
            memory_free(usb_device->vendor);
            memory_free(usb_device->serial);
            memory_free(usb_device);
            memory_free(config_buffer);

            return -1;
        }

        usb_device->interface = interface_desc;
        usb_device->endpoint = memory_malloc(sizeof(usb_endpoint_t));

        if(!usb_device->endpoint) {
            PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for endpoint");
            hashmap_delete(usb_devices, (void*)usb_device->device_id);
            memory_free(usb_device->product);
            memory_free(usb_device->vendor);
            memory_free(usb_device->serial);
            memory_free(usb_device);
            memory_free(config_buffer);

            return -1;
        }

        usb_device->endpoint->desc = endpoint_desc;
        usb_device->hid = hid_desc;

        PRINTLOG(USB, LOG_TRACE, "picked config: 0x%x", picked_config);

    } else {
        PRINTLOG(USB, LOG_ERROR, "cannot find interface and endpoint descriptors");
        hashmap_delete(usb_devices, (void*)usb_device->device_id);
        memory_free(usb_device->product);
        memory_free(usb_device->vendor);
        memory_free(usb_device->serial);
        memory_free(usb_device);
        memory_free(config_buffer);

        return -1;
    }

    PRINTLOG(USB, LOG_INFO, "device %s is ready", usb_device->product);

    return 0;
}
#pragma GCC diagnostic pop
