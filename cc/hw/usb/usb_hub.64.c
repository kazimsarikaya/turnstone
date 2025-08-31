/**
 * @file usb_hub.64.c
 * @brief USB HUB device driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/usb.h>
#include <hashmap.h>
#include <logging.h>
#include <time/timer.h>
#include <strings.h>
#include <pipeline.h>

MODULE("turnstone.kernel.hw.usb");

typedef struct usb_hub_status_t {
    uint16_t hub_status;
    uint16_t hub_change;
} __attribute__((packed)) usb_hub_status_t;

typedef struct usb_driver_t {
    usb_device_t*           usb_device;
    usb_interface_t*        interface;
    usb_pipeline_callback_f pipeline_callback;
    uint32_t                expected_packet_size;
    usb_hub_status_t        old_status;
    usb_hub_status_t        new_status;
} usb_driver_t;

static int8_t usb_hub_clear_feature (usb_device_t * usb_device, uint8_t port, usb_hub_feature_selector_t feature) {
    if(!usb_device) {
        PRINTLOG(USB, LOG_ERROR, "invalid device");

        return -1;
    }

    if(port > 0) {
        // port feature
        if(!usb_device_request(usb_device,
                               NULL,
                               USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_OTHER,
                               USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_CLEAR_FEATURE,
                               feature, port,
                               0, 0)) {
            PRINTLOG(USB, LOG_ERROR, "cannot clear feature %d on port %d", feature, port);

            return -1;
        }
    } else {
        // device feature
        if(!usb_device_request(usb_device,
                               NULL,
                               USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_DEVICE,
                               USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_CLEAR_FEATURE,
                               feature, 0,
                               0, 0)) {
            PRINTLOG(USB, LOG_ERROR, "cannot clear feature %d on device", feature);

            return -1;
        }
    }

    return 0;
}

static int8_t usb_hub_set_feature(usb_device_t* usb_device, uint8_t port, usb_hub_feature_selector_t feature) {
    if(!usb_device) {
        PRINTLOG(USB, LOG_ERROR, "invalid device");

        return -1;
    }

    if(port > 0) {
        // port feature
        PRINTLOG(USB, LOG_TRACE, "setting feature %d on port %d", feature, port);
        if(!usb_device_request(usb_device,
                               NULL,
                               USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_OTHER,
                               USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_SET_FEATURE,
                               feature, port,
                               0, 0)) {
            PRINTLOG(USB, LOG_ERROR, "cannot set feature %d on port %d", feature, port);

            return -1;
        }
    } else {
        // device feature
        PRINTLOG(USB, LOG_TRACE, "setting feature %d on device", feature);
        if(!usb_device_request(usb_device,
                               NULL,
                               USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_DEVICE,
                               USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_SET_FEATURE,
                               feature, 0,
                               0, 0)) {
            PRINTLOG(USB, LOG_ERROR, "cannot set feature %d on device", feature);

            return -1;
        }
    }

    return 0;
}

static int8_t usb_hub_get_status(usb_device_t* usb_device, uint8_t port, usb_hub_status_t* status) {
    if(!usb_device) {
        PRINTLOG(USB, LOG_ERROR, "invalid device");

        return -1;
    }

    if(!status) {
        PRINTLOG(USB, LOG_ERROR, "invalid status buffer");

        return -1;
    }

    if(port > 0) {
        // port status
        PRINTLOG(USB, LOG_TRACE, "getting status on port %d", port);
        if(!usb_device_request(usb_device,
                               NULL,
                               USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_OTHER,
                               USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_STATUS,
                               0, port,
                               sizeof(usb_hub_status_t), (uint8_t*)status)) {
            PRINTLOG(USB, LOG_ERROR, "cannot get status on port %d", port);

            return -1;
        }
    } else {
        // device status
        PRINTLOG(USB, LOG_TRACE, "getting status on device");
        if(!usb_device_request(usb_device,
                               NULL,
                               USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_DEVICE,
                               USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_STATUS,
                               0, 0,
                               sizeof(usb_hub_status_t), (uint8_t*)status)) {
            PRINTLOG(USB, LOG_ERROR, "cannot get status on device");

            return -1;
        }
    }

    return 0;
}

static int8_t usb_hub_pipeline_callback(const usb_driver_t* driver, uint8_t endpoint, pipeline_t* pipeline) {
    UNUSED(endpoint);
    usb_driver_t* usb_driver = (usb_driver_t*)driver;

    if(!usb_driver) {
        PRINTLOG(USB, LOG_ERROR, "invalid hub driver");

        return -1;
    }

    if(!pipeline) {
        PRINTLOG(USB, LOG_ERROR, "invalid pipeline");

        return -1;
    }

    uint64_t rc = pipeline_read(pipeline, sizeof(uint16_t), (uint8_t*)&usb_driver->new_status.hub_change);

    if(rc != sizeof(uint16_t)) {
        PRINTLOG(USB, LOG_ERROR, "cannot read hub status");

        return -1;
    }

    PRINTLOG(USB, LOG_TRACE, "hub status change: 0x%04x", usb_driver->new_status.hub_change);


    return 0;
}

static int8_t usb_hub_get_descriptor(usb_device_t* usb_device) {
    usb_config_t* config = usb_device->configurations[usb_device->selected_config];

    if(!config) {
        PRINTLOG(USB, LOG_ERROR, "no configuration selected");

        return -1;
    }

    if(!config->hub) {
        PRINTLOG(USB, LOG_TRACE, "no hub descriptor");

        uint32_t hub_dsec_length = usb_device->max_packet_size;;
        uint8_t* hub_desc_buffer = memory_malloc(hub_dsec_length);

        if(!hub_desc_buffer) {
            PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for hub descriptor");

            return -1;
        }

        if(!usb_device_request(usb_device,
                               NULL,
                               USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_DEVICE,
                               USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                               USB_HUB_DESC_TYPE_HUB << 8, 0,
                               hub_dsec_length, hub_desc_buffer)) {
            PRINTLOG(USB, LOG_ERROR, "cannot get hub descriptor");
            memory_free(hub_desc_buffer);

            return -1;
        }

        usb_hub_desc_t* hub_desc = (usb_hub_desc_t*)hub_desc_buffer;

        PRINTLOG(USB, LOG_TRACE, "hub descriptor length: 0x%x", hub_desc->length);

        if(hub_desc->length > hub_dsec_length) {
            // need to reallocate buffer
            hub_dsec_length = hub_desc->length;
            memory_free(hub_desc_buffer);
            hub_desc_buffer = memory_malloc(hub_dsec_length);

            if(!hub_desc_buffer) {
                PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for hub descriptor");

                return -1;
            }

            if(!usb_device_request(usb_device,
                                   NULL,
                                   USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_DEVICE,
                                   USB_REQUEST_DIRECTION_DEVICE_TO_HOST, USB_REQUEST_GET_DESCRIPTOR,
                                   USB_HUB_DESC_TYPE_HUB << 8, 0,
                                   hub_dsec_length, hub_desc_buffer)) {
                PRINTLOG(USB, LOG_ERROR, "cannot get hub descriptor");
                memory_free(hub_desc_buffer);

                return -1;
            }

            hub_desc = (usb_hub_desc_t*)hub_desc_buffer;
        }

        config->hub = hub_desc;
    }

    return 0;
}

static int8_t usb_hub_power_on_ports(usb_device_t* usb_device) {
    usb_config_t* config = usb_device->configurations[usb_device->selected_config];

    uint64_t delay_ms = config->hub->power_on_to_good * 2;

    for(uint8_t port = 1; port <= config->hub->num_ports; port++) {
        PRINTLOG(USB, LOG_DEBUG, "powering on port %d", port);

        if(usb_hub_clear_feature(usb_device, port, USB_HUB_FEATURE_PORT_POWER) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot power on port %d", port);

            return -1;
        }
    }

    PRINTLOG(USB, LOG_DEBUG, "all ports powered on, waiting %llims for stabilization", delay_ms);
    time_timer_msleep(delay_ms);

    return 0;
}

int8_t usb_hub_init(usb_device_t* usb_device, usb_interface_t* interface) {
    if(!usb_device) {
        PRINTLOG(USB, LOG_ERROR, "invalid device");

        return -1;
    }

    usb_driver_t* hub_driver = memory_malloc(sizeof(usb_driver_t));

    if(!hub_driver) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for hub driver");

        return -1;
    }
    usb_device->is_hub = true;
    hub_driver->usb_device = usb_device;
    hub_driver->interface = interface;
    hub_driver->pipeline_callback = usb_hub_pipeline_callback;
    hub_driver->expected_packet_size = sizeof(uint16_t);

    interface->driver = hub_driver;

    if(usb_hub_get_descriptor(usb_device) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot get hub descriptor");
        memory_free(hub_driver);

        return -1;
    }

    usb_config_t* config = usb_device->configurations[usb_device->selected_config];

    usb_device->hub_num_ports = config->hub->num_ports;
    usb_device->hub_status_endpoint_address = interface->endpoints[0]->desc->endpoint_address;

    PRINTLOG(USB, LOG_INFO, "initializing hub device with %d ports", config->hub->num_ports);
    PRINTLOG(USB, LOG_DEBUG, "hub characteristics: 0x%04x", config->hub->characteristics);
    PRINTLOG(USB, LOG_DEBUG, "hub power on to good: %d", config->hub->power_on_to_good);
    PRINTLOG(USB, LOG_DEBUG, "hub control current: %d", config->hub->hub_control_current);

    if(!usb_device_request(usb_device,
                           NULL,
                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_DEVICE,
                           USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_EVALUATE_CONTEXT,
                           0, 1,
                           0, 0)) {
        PRINTLOG(USB, LOG_ERROR, "cannot set device address");

        return -1;
    }

    if(usb_hub_power_on_ports(usb_device) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot power on ports");
        memory_free(hub_driver);

        return -1;
    }


    for(uint8_t port = 1; port <= config->hub->num_ports; port++) {
        usb_hub_status_t status = {0};

        if(usb_hub_get_status(usb_device, port, &status) != 0) {
            PRINTLOG(USB, LOG_ERROR, "cannot get status on port %d", port);
            memory_free(hub_driver);

            return -1;
        }

        PRINTLOG(USB, LOG_DEBUG, "port %d status: 0x%04x change: 0x%04x", port, status.hub_status, status.hub_change);

        if(status.hub_status & (1 << 0)) {
            PRINTLOG(USB, LOG_DEBUG, "port %d connected", port);

            if(usb_hub_set_feature(usb_device, port, USB_HUB_FEATURE_PORT_RESET) != 0) {
                PRINTLOG(USB, LOG_ERROR, "cannot reset port %d", port);
                memory_free(hub_driver);

                return -1;
            }

            if(usb_hub_get_status(usb_device, port, &status) != 0) {
                PRINTLOG(USB, LOG_ERROR, "cannot get status on port %d", port);
                memory_free(hub_driver);

                return -1;
            }

            PRINTLOG(USB, LOG_DEBUG, "port %d status: 0x%04x change: 0x%04x", port, status.hub_status, status.hub_change);

            if(status.hub_change & (1 << 0)) {
                PRINTLOG(USB, LOG_DEBUG, "port %d connection change detected", port);

                if(usb_hub_clear_feature(usb_device, port, USB_HUB_FEATURE_C_PORT_CONNECTION) != 0) {
                    PRINTLOG(USB, LOG_ERROR, "cannot clear connection change on port %d", port);
                    memory_free(hub_driver);

                    return -1;
                }
            }

            if(status.hub_change & (1 << 1)) {
                PRINTLOG(USB, LOG_DEBUG, "port %d enable change detected", port);

                if(usb_hub_clear_feature(usb_device, port, USB_HUB_FEATURE_C_PORT_ENABLE) != 0) {
                    PRINTLOG(USB, LOG_ERROR, "cannot clear enable change on port %d", port);
                    memory_free(hub_driver);

                    return -1;
                }
            }

            if(status.hub_change & (1 << 2)) {
                PRINTLOG(USB, LOG_DEBUG, "port %d suspend change detected", port);

                if(usb_hub_clear_feature(usb_device, port, USB_HUB_FEATURE_C_PORT_SUSPEND) != 0) {
                    PRINTLOG(USB, LOG_ERROR, "cannot clear suspend change on port %d", port);
                    memory_free(hub_driver);

                    return -1;
                }
            }

            if(status.hub_change & (1 << 3)) {
                PRINTLOG(USB, LOG_DEBUG, "port %d over current change detected", port);

                if(usb_hub_clear_feature(usb_device, port, USB_HUB_FEATURE_C_PORT_OVER_CURRENT) != 0) {
                    PRINTLOG(USB, LOG_ERROR, "cannot clear over current change on port %d", port);
                    memory_free(hub_driver);

                    return -1;
                }
            }

            if(status.hub_change & (1 << 4)) {
                PRINTLOG(USB, LOG_DEBUG, "port %d reset change detected", port);

                if(usb_hub_clear_feature(usb_device, port, USB_HUB_FEATURE_C_PORT_RESET) != 0) {
                    PRINTLOG(USB, LOG_ERROR, "cannot clear reset change on port %d", port);
                    memory_free(hub_driver);

                    return -1;
                }
            }

            if(usb_hub_get_status(usb_device, port, &status) != 0) {
                PRINTLOG(USB, LOG_ERROR, "cannot get status on port %d", port);
                memory_free(hub_driver);

                return -1;
            }

            PRINTLOG(USB, LOG_DEBUG, "port %d status: 0x%04x change: 0x%04x", port, status.hub_status, status.hub_change);

            boolean_t can_init_device = true;

            if(status.hub_status & (1 << 0)) {
                PRINTLOG(USB, LOG_DEBUG, "port %d connected", port);
            } else {
                PRINTLOG(USB, LOG_ERROR, "port %d not connected after reset", port);
                can_init_device = false;
            }

            if(status.hub_status & (1 << 1)) {
                PRINTLOG(USB, LOG_DEBUG, "port %d enabled", port);
            } else {
                PRINTLOG(USB, LOG_ERROR, "port %d not enabled after reset", port);
                can_init_device = false;
            }

            if(status.hub_status & (1 << 2)) {
                PRINTLOG(USB, LOG_DEBUG, "port %d suspended", port);
                can_init_device = false;
            }

            if(status.hub_status & (1 << 3)) {
                PRINTLOG(USB, LOG_DEBUG, "port %d over current", port);
                can_init_device = false;
            }

            if(status.hub_status & (1 << 4)) {
                PRINTLOG(USB, LOG_DEBUG, "port %d is low speed device", port);
            } else {
                PRINTLOG(USB, LOG_DEBUG, "port %d is high/full speed device", port);
            }


            if(can_init_device &&
               usb_device_init(usb_device, usb_device->controller, port - 1,
                               (status.hub_status & (1 << 4)) ?
                               USB_ENDPOINT_SPEED_LOW : USB_ENDPOINT_SPEED_FULL
                               ) != 0) {
                PRINTLOG(USB, LOG_ERROR, "cannot initialize device on port %d", port);
                memory_free(hub_driver);

                return -1;
            }

        } else {
            PRINTLOG(USB, LOG_DEBUG, "port %d not connected", port);
        }
    }


    PRINTLOG(USB, LOG_INFO, "hub device initialized");

    return 0;
}
