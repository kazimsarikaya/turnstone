/**
 * @file usb_device.64.c
 * @brief USB device driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/usb.h>
#include <driver/usb_xhci.h>
#include <driver/usb_vendor.h>
#include <list.h>
#include <logging.h>
#include <time/timer.h>
#include <strings.h>
#include <pipeline.h>

MODULE("turnstone.kernel.hw.usb");

static list_t* usb_devices = NULL;

typedef struct usb_driver_t {
    USB_DRIVER_COMMON_FIELDS;
} usb_driver_t;

static void usb_device_print_desc(usb_device_t* usb_device) {
    PRINTLOG(USB, LOG_TRACE, "device class: %x", usb_device->desc->device_class);
    PRINTLOG(USB, LOG_TRACE, "device subclass: %x", usb_device->desc->device_subclass);
    PRINTLOG(USB, LOG_TRACE, "device protocol: %x", usb_device->desc->device_protocol);
    PRINTLOG(USB, LOG_TRACE, "max packet size: 0x%x", usb_device->desc->max_packet_size);
    PRINTLOG(USB, LOG_TRACE, "vendor id: 0x%04x vendor name: %s",
             usb_device->desc->vendor_id, usb_device->vendor ? usb_device->vendor : "unknown");
    PRINTLOG(USB, LOG_TRACE, "product id: 0x%04x product: %s",
             usb_device->desc->product_id, usb_device->product ? usb_device->product : "unknown");
    PRINTLOG(USB, LOG_TRACE, "serial number string index: %x serial number: %s",
             usb_device->desc->serial_number_string, usb_device->serial ? usb_device->serial : "unknown");
    PRINTLOG(USB, LOG_TRACE, "device version: %x", usb_device->desc->device_version);
    PRINTLOG(USB, LOG_TRACE, "number of configuration: %x", usb_device->desc->num_configurations);
    PRINTLOG(USB, LOG_TRACE, "type: %x", usb_device->desc->type);
    PRINTLOG(USB, LOG_TRACE, "length: 0x%x", usb_device->desc->length);
}


static boolean_t usb_device_get_langs(usb_device_t* usb_device, char16_t* langs) {
    langs[0] = 0;

    char16_t buf[128] = {0};

    usb_string_desc_t* string_desc = (usb_string_desc_t*)buf;

    if (!usb_device_request(usb_device,
                            NULL,
                            USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                            USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                            (USB_BASE_DESC_TYPE_STRING << 8) | 0, 0,
                            usb_device->max_packet_size, string_desc)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get string descriptor length");

        return false;
    }

    if (!usb_device_request(usb_device,
                            NULL,
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

static boolean_t usb_device_get_string(usb_device_t* usb_device, char16_t lang_id, uint32_t str_index, char16_t* str) {

    str[0] = '\0';

    if (!str_index) {
        return false;
    }

    char16_t buf[128] = {0};

    usb_string_desc_t* string_desc = (usb_string_desc_t*)buf;

    if (!usb_device_request(usb_device,
                            NULL,
                            USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                            USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                            (USB_BASE_DESC_TYPE_STRING << 8) | str_index, lang_id,
                            usb_device->max_packet_size, string_desc)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get string descriptor length");

        return false;
    }

    if (!usb_device_request(usb_device,
                            NULL,
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
                             usb_interface_t*        interface,
                             usb_request_type_t      request_type,
                             usb_request_recipient_t request_recipient,
                             usb_request_direction_t request_direction,
                             uint32_t                request,
                             uint16_t                value,
                             uint16_t                index,
                             uint16_t                length,
                             void*                   data) {

    usb_device_request_t usb_request = {0};

    usb_request.type    = request_type | request_recipient | request_direction;
    usb_request.request = request;
    usb_request.value   = value;
    usb_request.index   = index;
    usb_request.length  = length;

    usb_transfer_t usb_transfer = {0};

    usb_endpoint_t* ep = NULL;

    /*
       if(usb_device->configurations) {
        if(usb_device->configurations[usb_device->selected_config]) {
            usb_config_t* config = usb_device->configurations[usb_device->selected_config];

            if(config->endpoints) {
                ep = config->endpoints[0];
            }
        }
       }
     */

    usb_driver_t dummy_driver = {0};
    dummy_driver.usb_device = usb_device;
    dummy_driver.interface  = interface;

    usb_transfer.driver   = &dummy_driver;
    usb_transfer.endpoint = ep;
    usb_transfer.request  = &usb_request;
    usb_transfer.data     = data;
    usb_transfer.length   = length;

    if(usb_device->controller->control_transfer(usb_device->controller, &usb_transfer) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot send request");

        return false;
    }

    if(request_type == USB_REQUEST_TYPE_STANDARD &&
       request_direction == USB_REQUEST_DIRECTION_HOST_TO_DEVICE &&
       request_recipient == USB_REQUEST_RECIPIENT_DEVICE &&
       request == USB_REQUEST_SET_ADDRESS) {
        usb_device->address = usb_request.value;
    }

    return usb_transfer.success;
};

static void usb_device_free(usb_device_t* usb_device) {
    if(!usb_device) {
        return;
    }

    list_list_delete(usb_devices, usb_device);

    if(usb_device->serial) {
        memory_free(usb_device->serial);
    }

    if(usb_device->vendor) {
        memory_free(usb_device->vendor);
    }

    if(usb_device->product) {
        memory_free(usb_device->product);
    }

    if(usb_device->descriptor_buffer) {
        memory_free(usb_device->descriptor_buffer);
    }

    if(usb_device->configurations) {
        for(uint32_t i = 0; i < usb_device->num_configurations; i++) {
            usb_config_t* config = usb_device->configurations[i];

            if(!config) {
                continue;
            }

            if(config->interfaces) {
                for(uint32_t k = 0; k < config->num_interfaces; k++) {
                    usb_interface_t* interface = config->interfaces[k];

                    if(!interface) {
                        continue;
                    }

                    if(interface->cs_interfaces) {
                        memory_free(interface->cs_interfaces);
                    }

                    if(interface->endpoints) {
                        for(uint32_t m = 0; m < interface->num_endpoints; m++) {
                            usb_endpoint_t* endpoint = interface->endpoints[m];

                            if(!endpoint) {
                                continue;
                            }

                            if(endpoint->cs_interfaces) {
                                memory_free(endpoint->cs_interfaces);
                            }

                            memory_free(endpoint);
                        }

                        memory_free(interface->endpoints);
                    }

                    if(interface->driver) {
                        if(interface->driver->free) {
                            interface->driver->free(interface->driver);
                        } else {
                            memory_free(interface->driver);
                        }
                    }

                    memory_free(interface);
                }

                memory_free(config->interfaces);
            }

            if(config->config_buffer) {
                memory_free(config->config_buffer);
            }

            memory_free(config);
        }

        memory_free(usb_device->configurations);
    }

    if(usb_device->controller && usb_device->controller_context) {
        if(usb_device->controller->destroy_controller_device_context) {
            usb_device->controller->destroy_controller_device_context(usb_device->controller, usb_device);
        }
    }

    memory_free(usb_device);

    PRINTLOG(USB, LOG_DEBUG, "usb device freed");
}

static int8_t usb_device_get_descriptor_strings(usb_device_t* usb_device) {
    char16_t lang_ids[128] = {0};

    if(!usb_device_get_langs(usb_device, lang_ids)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get langs");

        return -1;
    }

    char16_t product[128] = {0};
    char16_t vendor[128]  = {0};
    char16_t serial[128]  = {0};

    if(!usb_device_get_string(usb_device, lang_ids[0], usb_device->desc->product_string, product)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get product string");
    } else {
        char_t* product_str = wstr_to_str(product);
        usb_device->product = product_str;
    }

    if(!usb_device_get_string(usb_device, lang_ids[0], usb_device->desc->vendor_string, vendor)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get vendor string");
    } else {
        char_t* vendor_str = wstr_to_str(vendor);
        usb_device->vendor = vendor_str;
    }

    if(!usb_device_get_string(usb_device, lang_ids[0], usb_device->desc->serial_number_string, serial)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get serial string");
    } else {
        char_t* serial_str = wstr_to_str(serial);
        usb_device->serial = serial_str;
    }

    return 0;
}

static int8_t usb_device_get_config(usb_device_t* usb_device, usb_config_t* config) {
    uint32_t i = config->config_id;

    config->config_buffer = memory_malloc(usb_device->max_packet_size);

    if(!config->config_buffer) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for config buffer");

        return -1;
    }


    if(!usb_device_request(usb_device,
                           NULL,
                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                           USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                           (USB_BASE_DESC_TYPE_CONFIG << 8) | i, 0,
                           usb_device->max_packet_size, config->config_buffer)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get config descriptor");

        return -1;
    }

    usb_config_desc_t* config_desc = (usb_config_desc_t*)config->config_buffer;

    PRINTLOG(USB, LOG_TRACE, "config descriptor total length: 0x%x", config_desc->total_length);
    uint32_t total_length = config_desc->total_length;

    memory_free(config->config_buffer);

    config->config_buffer = memory_malloc(total_length);

    if(!config->config_buffer) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for config buffer");

        return -1;
    }

    if(!usb_device_request(usb_device,
                           NULL,
                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                           USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                           (USB_BASE_DESC_TYPE_CONFIG << 8) | i, 0,
                           total_length, config->config_buffer)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get config descriptor");

        return -1;
    }

    return 0;
}

static uint32_t usb_device_parse_cs_interfaces_of_interface(usb_config_t* config, usb_interface_t* interface, uint32_t idx) {
    uint32_t cs_interface_count    = 0;
    usb_config_desc_t* config_desc = (usb_config_desc_t*)config->config_buffer;

    uint32_t temp_idx = idx;
    while(temp_idx < config_desc->total_length) {
        uint8_t length = config->config_buffer[temp_idx];
        uint8_t type   = config->config_buffer[temp_idx + 1];

        if(length < 2) {
            PRINTLOG(USB, LOG_ERROR, "invalid descriptor length 0x%x for interface number 0x%x", length, interface->desc->interface_number);

            return (uint32_t)-1;
        }

        if(type == USB_CLASS_DESC_TYPE_CS_INTERFACE) {
            cs_interface_count++;
        } else {
            break;
        }
        temp_idx += length;
    }

    interface->num_cs_interfaces = cs_interface_count;

    if(cs_interface_count) {
        PRINTLOG(USB, LOG_DEBUG, "cs interface count: 0x%x", cs_interface_count);

        interface->cs_interfaces = memory_malloc(sizeof(usb_cs_interface_desc_t*) * cs_interface_count);

        if(!interface->cs_interfaces) {
            PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for cs interfaces with size 0x%lx", sizeof(usb_cs_interface_desc_t*) * cs_interface_count);

            return -1;
        }

        for(uint32_t k = 0; k < cs_interface_count; k++) {
            interface->cs_interfaces[k] = (usb_cs_interface_desc_t*)&config->config_buffer[idx];
            idx                        += interface->cs_interfaces[k]->length;
            PRINTLOG(USB, LOG_TRACE, "cs interface type 0x%x length 0x%x", interface->cs_interfaces[k]->type, interface->cs_interfaces[k]->length);
        }

    }

    PRINTLOG(USB, LOG_TRACE, "current index after cs interfaces: 0x%x", idx);

    return idx;
}

static uint32_t usb_device_parse_other_descs_of_interface(usb_config_t* config, usb_interface_t* interface, uint32_t idx) {
    usb_config_desc_t* config_desc = (usb_config_desc_t*)config->config_buffer;

    while(idx < config_desc->total_length) {
        uint8_t length = config->config_buffer[idx];
        uint8_t type   = config->config_buffer[idx + 1];

        if(length < 2) {
            PRINTLOG(USB, LOG_ERROR, "invalid descriptor length 0x%x for interface number 0x%x", length, interface->desc->interface_number);

            return (uint32_t)-1;
        }

        if(type == USB_BASE_DESC_TYPE_ENDPOINT) {
            break;
        } else if(type == USB_BASE_DESC_TYPE_INTERFACE) {
            if(interface->num_endpoints) {
                PRINTLOG(USB, LOG_ERROR, "expected endpoint descriptor but got interface descriptor for interface number 0x%x",
                         interface->desc->interface_number);
                return -1;
            }

            break;
        } else if(type == USB_HID_DESC_TYPE_HID) {
            PRINTLOG(USB, LOG_TRACE, "HID descriptor for interface number 0x%x", interface->desc->interface_number);
            interface->hid = (usb_hid_desc_t*)&config->config_buffer[idx];
        } else {
            PRINTLOG(USB, LOG_DEBUG, "unknown descriptor type 0x%x with length 0x%x for interface number 0x%x",
                     type, length, interface->desc->interface_number);
        }

        idx += length;
    }

    PRINTLOG(USB, LOG_TRACE, "current index after other descriptors: 0x%x", idx);

    return idx;
}

static uint32_t usb_device_parse_endpoints_of_interface(usb_config_t* config, usb_interface_t* interface, uint32_t idx) {
    usb_config_desc_t* config_desc = (usb_config_desc_t*)config->config_buffer;

    interface->endpoints = memory_malloc(sizeof(usb_endpoint_t*) * interface->num_endpoints);

    if(!interface->endpoints) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for endpoints with size 0x%lx", sizeof(usb_endpoint_t*) * interface->num_endpoints);

        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "endpoint count: 0x%x", interface->num_endpoints);

    for(uint32_t k = 0; k < interface->num_endpoints; k++) {
        if(idx >= config_desc->total_length) {
            PRINTLOG(USB, LOG_ERROR, "index out of bounds. idx: 0x%x total_length: 0x%x", idx, config_desc->total_length);

            return -1;
        }

        uint8_t length = config->config_buffer[idx];

        if(length < 2) {
            PRINTLOG(USB, LOG_ERROR, "invalid descriptor length 0x%x for endpoint index 0x%x of interface number 0x%x",
                     length, k, interface->desc->interface_number);

            return -1;
        }

        uint8_t type = config->config_buffer[idx + 1];

        if(type != USB_BASE_DESC_TYPE_ENDPOINT) {
            PRINTLOG(USB, LOG_ERROR, "expected endpoint descriptor type 0x%x got 0x%x", USB_BASE_DESC_TYPE_ENDPOINT, type);

            return -1;
        }

        usb_endpoint_desc_t* endpoint_desc = (usb_endpoint_desc_t*)&config->config_buffer[idx];

        interface->endpoints[k] = memory_malloc(sizeof(usb_endpoint_t));

        if(!interface->endpoints[k]) {
            PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for endpoint");

            return -1;
        }

        usb_endpoint_t* endpoint = interface->endpoints[k];

        endpoint->desc = endpoint_desc;
        endpoint->in   = endpoint_desc->endpoint_address >> 7;

        idx += length;

        if(idx == config_desc->total_length) {
            PRINTLOG(USB, LOG_TRACE, "no more data after endpoint descriptor for endpoint address 0x%x",
                     endpoint->desc->endpoint_address);

            if(k + 1 != interface->num_endpoints) {
                PRINTLOG(USB, LOG_ERROR, "expected more endpoints. expected: 0x%x got: 0x%x",
                         interface->num_endpoints, k + 1);

                return -1;
            }

            break;
        }

        uint8_t tmp_length = config->config_buffer[idx];
        uint8_t tmp_type   = config->config_buffer[idx + 1];

        if(tmp_length < 2) {
            PRINTLOG(USB, LOG_ERROR, "invalid descriptor length 0x%x for endpoint address 0x%x",
                     tmp_length, endpoint->desc->endpoint_address);

            return -1;
        }

        // maybe there is a endpoint companion descriptor
        if(tmp_type == USB_ENDPOINT_COMPANION_DESC_TYPE) {
            PRINTLOG(USB, LOG_DEBUG, "endpoint companion descriptor for endpoint address 0x%x",
                     endpoint->desc->endpoint_address);
            endpoint->endpoint_companion = (usb_endpoint_companion_desc_t*)(config->config_buffer + idx);
            idx                         += tmp_length;
        }

        uint32_t tmp_idx = idx;
        endpoint->num_cs_interfaces = 0;
        while(tmp_idx < config_desc->total_length) {
            tmp_length = config->config_buffer[tmp_idx];
            tmp_type   = config->config_buffer[tmp_idx + 1];

            if(tmp_length < 2) {
                PRINTLOG(USB, LOG_ERROR, "invalid descriptor length 0x%x for endpoint address 0x%x",
                         tmp_length, endpoint->desc->endpoint_address);

                return -1;
            }

            if(tmp_type == USB_CLASS_DESC_TYPE_CS_INTERFACE ||
               tmp_type == USB_CLASS_DESC_TYPE_CS_ENDPOINT) {
                endpoint->num_cs_interfaces++;
            } else {
                PRINTLOG(USB, LOG_TRACE, "stop counting cs interface descriptors for endpoint address 0x%x at type 0x%x",
                         endpoint->desc->endpoint_address, tmp_type);
                break;
            }

            tmp_idx += tmp_length;
        }

        PRINTLOG(USB, LOG_DEBUG, "cs interface/endpoint count: 0x%x", endpoint->num_cs_interfaces);

        if(endpoint->num_cs_interfaces) {
            endpoint->cs_interfaces = memory_malloc(sizeof(usb_cs_interface_desc_t*) * endpoint->num_cs_interfaces);

            if(!endpoint->cs_interfaces) {
                PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for cs interfaces with size 0x%lx",
                         sizeof(usb_cs_interface_desc_t*) * endpoint->num_cs_interfaces);

                return -1;
            }

            for(uint32_t l = 0; l < endpoint->num_cs_interfaces; l++) {
                endpoint->cs_interfaces[l] = (usb_cs_interface_desc_t*)(config->config_buffer + idx);
                idx                       += endpoint->cs_interfaces[l]->length;
            }
        }

    }

    PRINTLOG(USB, LOG_TRACE, "current index after endpoints: 0x%x", idx);

    return idx;
}

static uint32_t usb_device_count_real_interfaces(usb_config_t* config) {
    uint32_t interface_count       = 0;
    usb_config_desc_t* config_desc = (usb_config_desc_t*)config->config_buffer;

    uint32_t idx = config_desc->length;

    while(idx < config_desc->total_length) {
        uint8_t length = config->config_buffer[idx];
        uint8_t type   = config->config_buffer[idx + 1];

        if(length < 2) {
            PRINTLOG(USB, LOG_ERROR, "invalid descriptor length 0x%x for interface count", length);

            return (uint32_t)-1;
        }

        if(type == USB_BASE_DESC_TYPE_INTERFACE) {
            interface_count++;
        }

        idx += length;
    }

    return interface_count;
}

int8_t usb_device_deinit(usb_device_t* parent, usb_controller_t* controller, uint32_t port) {
    if(!usb_devices) {
        PRINTLOG(USB, LOG_ERROR, "no usb devices");

        return -1;
    }


    usb_device_t* usb_device = NULL;

    for(size_t i = 0; i < list_size(usb_devices); i++) {
        usb_device_t* dev = (usb_device_t*)list_get_data_at_position(usb_devices, i);

        if(dev->parent == parent && dev->controller == controller && dev->port == port) {
            usb_device = dev;
            break;
        }
    }

    if(!usb_device) {
        PRINTLOG(USB, LOG_ERROR, "cannot find usb device for controller 0x%p port %d", controller, port);

        return -1;
    }

    usb_device_free(usb_device);

    PRINTLOG(USB, LOG_DEBUG, "usb device deinitialized");

    return 0;
}

int8_t usb_device_init(usb_device_t* parent, usb_controller_t* controller, uint32_t port, uint32_t speed) {
    if(usb_devices == NULL) {
        usb_devices = list_create_list();

        if(!usb_devices) {
            PRINTLOG(USB, LOG_ERROR, "cannot create usb devices list");

            return -1;
        }
    }

    usb_device_t* usb_device = memory_malloc(sizeof(usb_device_t));

    if(!usb_device) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb device");
        memory_free(usb_device);

        return -1;
    }

    usb_device->parent     = parent;
    usb_device->controller = controller;
    usb_device->port       = port;
    usb_device->speed      = speed;

    uint64_t parent_device_id = 0;
    uint64_t parent_port      = 0;
    if(parent) {
        parent_device_id = parent->device_id;
        parent_port      = parent->port;
    }

    uint64_t controller_id = controller->controller_id;

    uint64_t device_id = (parent_device_id << 32) | (controller_id << 16) | (parent_port << 8) | port;

    usb_device->device_id = device_id;

    list_list_insert(usb_devices, usb_device);

    if(!usb_device_request(usb_device,
                           NULL,
                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                           USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_SET_ADDRESS,
                           0, 0,
                           0, 0)) {
        PRINTLOG(USB, LOG_ERROR, "cannot set device address");
        usb_device_free(usb_device);

        return -1;
    }

    PRINTLOG(USB, LOG_DEBUG, "device address: %x", usb_device->address);

    time_timer_spinsleep(5000);

    usb_device->max_packet_size = 8;

    usb_device->descriptor_buffer = memory_malloc(usb_device->max_packet_size);

    if(!usb_device->descriptor_buffer) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for descriptor buffer");
        usb_device_free(usb_device);

        return -1;
    }


    if(!usb_device_request(usb_device,
                           NULL,
                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                           USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                           USB_BASE_DESC_TYPE_DEVICE << 8, 0,
                           usb_device->max_packet_size, usb_device->descriptor_buffer)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get device descriptor");
        usb_device_free(usb_device);

        return -1;
    }

    usb_device->desc = (usb_device_desc_t*)usb_device->descriptor_buffer;

    PRINTLOG(USB, LOG_DEBUG, "max packet size: 0x%x", usb_device->desc->max_packet_size);

    usb_device->max_packet_size = usb_device->desc->max_packet_size;

    uint32_t desc_size = usb_device->desc->length;

    memory_free(usb_device->descriptor_buffer);
    usb_device->descriptor_buffer = memory_malloc(desc_size);

    if(!usb_device->descriptor_buffer) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for descriptor buffer with size 0x%x", desc_size);
        usb_device_free(usb_device);

        return -1;
    }


    if(!usb_device_request(usb_device,
                           NULL,
                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                           USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                           USB_BASE_DESC_TYPE_DEVICE << 8, 0,
                           desc_size, usb_device->descriptor_buffer)) {
        PRINTLOG(USB, LOG_ERROR, "cannot get device descriptor");
        usb_device_free(usb_device);

        return -1;
    }

    usb_device->desc = (usb_device_desc_t*)usb_device->descriptor_buffer;

    if(usb_device_get_descriptor_strings(usb_device) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot get descriptor strings");
        usb_device_free(usb_device);

        return -1;
    }

    usb_device->vendor_id      = usb_device->desc->vendor_id;
    usb_device->product_id     = usb_device->desc->product_id;
    usb_device->device_version = usb_device->desc->device_version;

    usb_device_print_desc(usb_device);

    usb_device->num_configurations = usb_device->desc->num_configurations;
    usb_device->configurations     = memory_malloc(sizeof(usb_config_desc_t*) * usb_device->num_configurations);

    if(!usb_device->configurations) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for configurations");
        usb_device_free(usb_device);

        return -1;
    }

    for(uint32_t i = 0; i < usb_device->num_configurations; i++) {
        usb_device->configurations[i] = memory_malloc(sizeof(usb_config_t));

        if(!usb_device->configurations[i]) {
            PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for configuration");
            usb_device_free(usb_device);

            return -1;
        }

        usb_config_t* config = usb_device->configurations[i];
        config->config_id = i;

        if(usb_device_get_config(usb_device, config) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot get config");
            usb_device_free(usb_device);

            return -1;
        }

        usb_config_desc_t* config_desc = (usb_config_desc_t*)config->config_buffer;

        config->configuration_value = config_desc->configuration_value;

        uint32_t idx = config_desc->length;

        PRINTLOG(USB, LOG_TRACE, "config data between 0x%x-0x%x", config_desc->length, config_desc->total_length);

        uint32_t real_interface_count = usb_device_count_real_interfaces(config);

        config->num_interfaces = config_desc->num_interfaces;

        if(real_interface_count != config->num_interfaces) {
            PRINTLOG(USB, LOG_WARNING, "interface count mismatch. expected: 0x%x got: 0x%x",
                     config->num_interfaces, real_interface_count);

            config->num_interfaces = real_interface_count;
        }


        config->interfaces = memory_malloc(sizeof(usb_interface_t*) * config_desc->num_interfaces);

        if(!config->interfaces) {
            PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for interfaces with size 0x%lx", sizeof(usb_interface_t*) * config_desc->num_interfaces);
            usb_device_free(usb_device);

            return -1;
        }

        PRINTLOG(USB, LOG_DEBUG, "interface count: 0x%x", config->num_interfaces);

        for(uint32_t j = 0; j < config->num_interfaces; j++) {
            if(idx >= config_desc->total_length) {
                PRINTLOG(USB, LOG_ERROR, "index out of bounds idx: 0x%x total length: 0x%x", idx, config_desc->total_length);
                usb_device_free(usb_device);

                return -1;
            }
            uint8_t length = config->config_buffer[idx];
            uint8_t type   = config->config_buffer[idx + 1];

            if(length < 2) {
                PRINTLOG(USB, LOG_ERROR, "invalid descriptor length 0x%x for interface index 0x%x", length, j);
                usb_device_free(usb_device);

                return -1;
            }

            if(type != USB_BASE_DESC_TYPE_INTERFACE) {
                PRINTLOG(USB, LOG_ERROR, "expected interface descriptor type 0x%x got 0x%x", USB_BASE_DESC_TYPE_INTERFACE, type);
                usb_device_free(usb_device);

                return -1;
            }


            config->interfaces[j] = memory_malloc(sizeof(usb_interface_t));

            if(!config->interfaces[j]) {
                PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for interface");
                usb_device_free(usb_device);

                return -1;
            }

            usb_interface_t* interface = config->interfaces[j];

            interface->desc          = (usb_interface_desc_t*)&config->config_buffer[idx];
            interface->num_endpoints = interface->desc->num_endpoints;

            PRINTLOG(USB, LOG_DEBUG, "interface number: 0x%x alt setting: 0x%x num endpoints: 0x%x",
                     interface->desc->interface_number, interface->desc->alternate_setting, interface->num_endpoints);
            PRINTLOG(USB, LOG_DEBUG, "interface class: 0x%x subclass: 0x%x protocol: 0x%x",
                     interface->desc->interface_class, interface->desc->interface_subclass, interface->desc->interface_protocol);

            idx += length;

            if(idx == config_desc->total_length) {
                PRINTLOG(USB, LOG_TRACE, "no more data after interface descriptor for interface number 0x%x",
                         interface->desc->interface_number);

                if(j + 1 != config->num_interfaces) {
                    PRINTLOG(USB, LOG_ERROR, "expected more interfaces. expected: 0x%x got: 0x%x",
                             config->num_interfaces, j + 1);
                    usb_device_free(usb_device);

                    return -1;
                }

                if(interface->num_endpoints != 0) {
                    PRINTLOG(USB, LOG_ERROR, "expected 0 endpoints for interface number 0x%x got: 0x%x",
                             interface->desc->interface_number, interface->num_endpoints);
                    usb_device_free(usb_device);

                    return -1;
                }

                break;
            } else if(idx > config_desc->total_length) {
                PRINTLOG(USB, LOG_ERROR, "index out of bounds idx: 0x%x total length: 0x%x", idx, config_desc->total_length);
                usb_device_free(usb_device);

                return -1;
            }

            idx = usb_device_parse_cs_interfaces_of_interface(config, interface, idx);

            if(idx == (uint32_t)-1) {
                PRINTLOG(USB, LOG_ERROR, "cannot parse cs interfaces of interface number 0x%x", interface->desc->interface_number);
                usb_device_free(usb_device);

                return -1;
            }

            if(idx == config_desc->total_length) {
                PRINTLOG(USB, LOG_TRACE, "no more data after cs interface descriptors for interface number 0x%x",
                         interface->desc->interface_number);

                if(interface->num_endpoints != 0) {
                    PRINTLOG(USB, LOG_ERROR, "expected 0 endpoints for interface number 0x%x got: 0x%x",
                             interface->desc->interface_number, interface->num_endpoints);
                    usb_device_free(usb_device);

                    return -1;
                }

                if(j + 1 != config->num_interfaces) {
                    PRINTLOG(USB, LOG_ERROR, "expected more interfaces. expected: 0x%x got: 0x%x",
                             config->num_interfaces, j + 1);
                    usb_device_free(usb_device);

                    return -1;
                }

                break;
            } else if(idx > config_desc->total_length) {
                PRINTLOG(USB, LOG_ERROR, "index out of bounds idx: 0x%x total length: 0x%x", idx, config_desc->total_length);
                usb_device_free(usb_device);

                return -1;
            }

            idx = usb_device_parse_other_descs_of_interface(config, interface, idx);

            if(idx == (uint32_t)-1) {
                PRINTLOG(USB, LOG_ERROR, "cannot parse cs interfaces of interface");
                usb_device_free(usb_device);

                return -1;
            }

            if(idx == config_desc->total_length) {
                PRINTLOG(USB, LOG_TRACE, "no more data after other descriptors for interface number 0x%x",
                         interface->desc->interface_number);

                if(interface->num_endpoints != 0) {
                    PRINTLOG(USB, LOG_ERROR, "expected 0 endpoints for interface number 0x%x got: 0x%x",
                             interface->desc->interface_number, interface->num_endpoints);
                    usb_device_free(usb_device);

                    return -1;
                }

                if(j + 1 != config->num_interfaces) {
                    PRINTLOG(USB, LOG_ERROR, "expected more interfaces. expected: 0x%x got: 0x%x",
                             config->num_interfaces, j + 1);
                    usb_device_free(usb_device);

                    return -1;
                }

                break;
            } else if(idx > config_desc->total_length) {
                PRINTLOG(USB, LOG_ERROR, "index out of bounds idx: 0x%x total length: 0x%x", idx, config_desc->total_length);
                usb_device_free(usb_device);

                return -1;
            }

            if(interface->num_endpoints) {
                idx = usb_device_parse_endpoints_of_interface(config, interface, idx);

                if(idx == (uint32_t)-1) {
                    PRINTLOG(USB, LOG_ERROR, "cannot parse endpoints of interface");
                    usb_device_free(usb_device);

                    return -1;
                }
            }

            PRINTLOG(USB, LOG_TRACE, "current index after interface: 0x%x", idx);

        }

        PRINTLOG(USB, LOG_TRACE, "current index after config: 0x%x", idx);

        if(idx != config_desc->total_length) {
            PRINTLOG(USB, LOG_WARNING, "did not parse whole config. expected: 0x%x got: 0x%x",
                     config_desc->total_length, idx);

            while(idx < config_desc->total_length) {
                uint8_t length = config->config_buffer[idx];
                uint8_t type   = config->config_buffer[idx + 1];

                if(length < 2) {
                    PRINTLOG(USB, LOG_ERROR, "invalid descriptor length 0x%x for config number 0x%x", length, i);
                    usb_device_free(usb_device);

                    return -1;
                }

                PRINTLOG(USB, LOG_DEBUG, "unknown descriptor type 0x%x with length 0x%x for config number 0x%x",
                         type, length, i);

                idx += length;
            }

        }
    }

    usb_device->selected_config = 0;

    uint8_t selected_config_value = usb_device->configurations[usb_device->selected_config]->configuration_value;

    if(!usb_device_request(usb_device,
                           NULL,
                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                           USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_SET_CONFIGURATION,
                           selected_config_value, 0,
                           0, 0)) {
        PRINTLOG(USB, LOG_ERROR, "cannot set selected config 0x%x and value 0x%x", usb_device->selected_config, selected_config_value);
        usb_device_free(usb_device);

        return -1;
    }


    PRINTLOG(USB, LOG_TRACE, "picked config: 0x%x and value 0x%x", usb_device->selected_config, selected_config_value);

    usb_config_t* selected_config = usb_device->configurations[usb_device->selected_config];

    if(!selected_config) {
        PRINTLOG(USB, LOG_ERROR, "no selected config");
        usb_device_free(usb_device);

        return -1;
    }

    if(selected_config->num_interfaces == 0) {
        PRINTLOG(USB, LOG_ERROR, "no interfaces in selected config");
        usb_device_free(usb_device);

        return -1;
    }

    for(uint32_t i = 0; i < selected_config->num_interfaces; i++) {
        usb_interface_t* interface = selected_config->interfaces[i];

        if(!interface) {
            PRINTLOG(USB, LOG_ERROR, "no interface at index 0x%x", i);
            usb_device_free(usb_device);

            return -1;
        }

        interface->interface_id = i;

        int32_t interface_number   = interface->desc->interface_number;
        int32_t interface_class    = interface->desc->interface_class;
        int32_t interface_subclass = interface->desc->interface_subclass;
        int32_t interface_protocol = interface->desc->interface_protocol;
        uint32_t num_endpoints     = interface->num_endpoints;


        PRINTLOG(USB, LOG_DEBUG, "interface 0%x (0x%x) class: 0x%x subclass: 0x%x protocol: 0x%x endpoint count 0x%x",
                 i,
                 interface_number,
                 interface_class,
                 interface_subclass,
                 interface_protocol,
                 num_endpoints);

        usb_endpoint_t** endpoints = interface->endpoints;

        for(uint32_t j = 0; j < num_endpoints; j++) {
            uint32_t ep_address         = endpoints[j]->desc->endpoint_address;
            uint32_t ep_max_packet_size = endpoints[j]->desc->max_packet_size;
            PRINTLOG(USB, LOG_DEBUG, "endpoint address: 0x%x 0x%x",
                     ep_address, ep_max_packet_size);

            uint32_t stream_count = 0;

            if(interface_protocol == USB_PROTOCOL_MASS_STORAGE_UAS) {
                stream_count = 2; // usb_xhci_get_max_psa_size(controller) / sizeof(usb_xhci_stream_context_t);
            }

            if(!usb_device_request(usb_device,
                                   interface,
                                   USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_ENDPOINT,
                                   USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_ENDPOINT_SETUP_ENDPOINT,
                                   ep_max_packet_size, ep_address,
                                   stream_count, 0)) {
                PRINTLOG(USB, LOG_ERROR, "cannot set endpoint 0x%x max packet size 0x%x",
                         ep_address, ep_max_packet_size);
                usb_device_free(usb_device);

                return -1;
            }

            PRINTLOG(USB, LOG_DEBUG, "set endpoint 0x%x max packet size 0x%x",
                     ep_address, ep_max_packet_size);
        }

        if(interface_class == USB_CLASS_HID) {
            if(interface_subclass == USB_SUBCLASS_HID_BOOT_INTERFACE_SUBCLASS) {
                if(interface_protocol == USB_PROTOCOL_HID_KEYBOARD) {
                    if(usb_keyboard_init(usb_device, interface) != 0) {
                        PRINTLOG(USB, LOG_ERROR, "cannot initialize keyboard");
                        usb_device_free(usb_device);

                        return -1;
                    }
                } else if(interface_protocol == USB_PROTOCOL_HID_MOUSE) {
                    if(usb_mouse_init(usb_device, interface) != 0) {
                        PRINTLOG(USB, LOG_ERROR, "cannot initialize mouse");
                        usb_device_free(usb_device);

                        return -1;
                    }
                }
            }

            if(strcmp(usb_device->vendor, "QEMU") == 0 && strcmp(usb_device->product, "QEMU USB Tablet") == 0) {
                if(usb_qemu_tablet_init(usb_device, interface) != 0) {
                    PRINTLOG(USB, LOG_ERROR, "cannot initialize tablet");
                    usb_device_free(usb_device);

                    return -1;
                }
            }
        }

        if(interface_class == USB_CLASS_HUB) {
            PRINTLOG(USB, LOG_INFO, "hub device detected");
            if(usb_hub_init(usb_device, interface) != 0) {
                PRINTLOG(USB, LOG_ERROR, "cannot initialize hub");
                usb_device_free(usb_device);

                return -1;
            }
        }

        if(interface_class == USB_CLASS_MASS_STORAGE) {
            if(interface_subclass == USB_SUBCLASS_MASS_STORAGE_SCSI_TRANSPARENT_COMMAND_SET) {
                if(interface_protocol == USB_PROTOCOL_MASS_STORAGE_BULK_ONLY ||
                   interface_protocol == USB_PROTOCOL_MASS_STORAGE_UAS) {
                    if(usb_mass_storage_init(usb_device, interface) != 0) {
                        PRINTLOG(USB, LOG_ERROR, "cannot initialize mass storage uas");
                        usb_device_free(usb_device);

                        return -1;
                    }
                } else {
                    PRINTLOG(USB, LOG_ERROR, "unknown mass storage protocol 0x%x", interface_protocol);
                    usb_device_free(usb_device);

                    return -1;
                }
            }
        }

        if(interface_class == USB_CLASS_AUDIO) {
            if(interface_subclass == USB_SUBCLASS_AUDIO_CONTROL) {
                if(usb_audio_control_init(usb_device, interface) != 0) {
                    PRINTLOG(USB, LOG_ERROR, "cannot initialize audio");
                    usb_device_free(usb_device);

                    return -1;
                }
            } else if(interface_subclass == USB_SUBCLASS_AUDIO_STREAMING) {
                if(num_endpoints >= 1) {
                    if(!usb_device_request(usb_device,
                                           NULL,
                                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_INTERFACE,
                                           USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_SET_INTERFACE,
                                           interface->desc->alternate_setting, interface->desc->interface_number,
                                           0, 0)) {
                        PRINTLOG(USB, LOG_ERROR, "cannot set interface 0x%x for config 0x%x",
                                 interface->desc->interface_number,
                                 usb_device->selected_config);
                        usb_device_free(usb_device);

                        return -1;
                    }

                    if(usb_audio_streaming_init(usb_device, interface) != 0) {
                        PRINTLOG(USB, LOG_ERROR, "cannot initialize audio");
                        usb_device_free(usb_device);

                        return -1;
                    }

                }
            }
        }


        if(interface_class == USB_CLASS_VENDOR_SPECIFIC && interface_subclass == USB_SUBCLASS_VENDOR_SPECIFIC) {
            if(usb_device->vendor_id == USB_VENDOR_ID_TP_LINK && usb_device->product_id == USB_PRODUCT_ID_TP_LINK_UE300) {
                if(usb_device_rtl815x_init(usb_device, interface) != 0) {
                    PRINTLOG(USB, LOG_ERROR, "cannot initialize tp-link ue300c");
                    usb_device_free(usb_device);

                    return -1;
                }
            } else {
                PRINTLOG(USB, LOG_WARNING, "unknown vendor specific device 0x%x:0x%x",
                         usb_device->vendor_id, usb_device->product_id);
            }


        }


    }

    PRINTLOG(USB, LOG_INFO, "device %s is ready", usb_device->product);

    return 0;
}
