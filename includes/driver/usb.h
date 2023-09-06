/**
 * @file usb.h
 * @brief USB driver header file
 */

#ifndef ___USB_H
#define ___USB_H

#include <types.h>
#include <pci.h>

#define USB_EHCI 0x20
#define USB_XHCI 0x30



typedef enum usb_endpoint_speed_t {
    USB_ENDPOINT_SPEED_FULL = 0,
    USB_ENDPOINT_SPEED_LOW  = 1,
    USB_ENDPOINT_SPEED_HIGH = 2,
} usb_endpoint_speed_t;


typedef union usb_legacy_support_capabilities_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t capability_id        : 8;
        volatile uint32_t next_pointer         : 8;
        volatile uint32_t bios_owned_semaphore : 1;
        volatile uint32_t reserved             : 7;
        volatile uint32_t os_owned_semaphore   : 1;
        volatile uint32_t reserved2            : 7;
    } __attribute__((packed)) bits;
} usb_legacy_support_capabilities_t;

typedef union usb_legacy_support_status_t {
    volatile uint32_t raw;
    struct {
        volatile uint32_t smi_enable                        : 1;
        volatile uint32_t error_enable                      : 1;
        volatile uint32_t port_change_enable                : 1;
        volatile uint32_t frame_list_rollover_enable        : 1;
        volatile uint32_t host_system_error_enable          : 1;
        volatile uint32_t interrupt_on_async_advance_enable : 1;
        volatile uint32_t reserved                          : 7;
        volatile uint32_t os_ownership_enable               : 1;
        volatile uint32_t pci_command_enable                : 1;
        volatile uint32_t pci_bar_enable                    : 1;
        volatile uint32_t complete                          : 1;
        volatile uint32_t error                             : 1;
        volatile uint32_t port_change                       : 1;
        volatile uint32_t frame_list_rollover               : 1;
        volatile uint32_t host_system_error                 : 1;
        volatile uint32_t on_async_advance                  : 1;
        volatile uint32_t reserved2                         : 7;
        volatile uint32_t ownership_change                  : 1;
        volatile uint32_t pci_command                       : 1;
        volatile uint32_t pci_bar                           : 1;
    } __attribute__((packed)) bits;
} usb_legacy_support_status_t;

typedef enum usb_controler_type_t {
    USB_CONTROLLER_TYPE_EHCI = 0x20,
    USB_CONTROLLER_TYPE_XHCI = 0x30,
} usb_controler_type_t;

typedef struct usb_controller_metadata_t usb_controller_metadata_t;

typedef enum usb_base_desc_type_t {
    USB_BASE_DESC_TYPE_DEVICE = 0x01,
    USB_BASE_DESC_TYPE_CONFIG = 0x02,
    USB_BASE_DESC_TYPE_STRING = 0x03,
    USB_BASE_DESC_TYPE_INTERFACE = 0x04,
    USB_BASE_DESC_TYPE_ENDPOINT = 0x05,
} usb_base_desc_type_t;

typedef enum usb_hid_desc_type_t {
    USB_HID_DESC_TYPE_HID = 0x21,
    USB_HID_DESC_TYPE_REPORT = 0x22,
    USB_HID_DESC_TYPE_PHYSICAL = 0x23,
} usb_hid_desc_type_t;

typedef enum usb_hub_desc_type_t {
    USB_HUB_DESC_TYPE_HUB = 0x29,
} usb_hub_desc_type_t;

typedef enum usb_hub_characteristic_t {
    USB_HUB_CHARACTERISTIC_POWER_MASK = 0x03,
    USB_HUB_CHARACTERISTIC_GLOBAL= 0x00,
    USB_HUB_CHARACTERISTIC_INDIVIDUAL = 0x01,
    USB_HUB_CHARACTERISTIC_COMPOUND = 0x04,
    USB_HUB_CHARACTERISTIC_OVERCURRENT_MASK = 0x18,
    USB_HUB_CHARACTERISTIC_TT_TTI_MASK = 0x60,
    USB_HUB_CHARACTERISTIC_PORT_INDICATOR = 0x80,
} usb_hub_characteristic_t;

typedef struct usb_device_desc_t {
    uint8_t  length;
    uint8_t  type;
    uint16_t usb_version;
    uint8_t  device_class;
    uint8_t  device_subclass;
    uint8_t  device_protocol;
    uint8_t  max_packet_size;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_version;
    uint8_t  vendor_string;
    uint8_t  product_string;
    uint8_t  serial_number_string;
    uint8_t  num_configurations;
}__attribute__((packed)) usb_device_desc_t;

typedef struct usb_config_desc_t {
    uint8_t  length;
    uint8_t  type;
    uint16_t total_length;
    uint8_t  num_interfaces;
    uint8_t  configuration_value;
    uint8_t  configuration_string;
    uint8_t  attributes;
    uint8_t  max_power;
    //uint8_t  config_data[]; we cannot use like that may be padding exists
}__attribute__((packed)) usb_config_desc_t;

typedef struct usb_interface_desc_t {
    uint8_t length;
    uint8_t type;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t num_endpoints;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t interface_string;
}__attribute__((packed)) usb_interface_desc_t;

typedef struct usb_endpoint_desc_t {
    uint8_t  length;
    uint8_t  type;
    uint8_t  endpoint_address;
    uint8_t  attributes;
    uint16_t max_packet_size;
    uint8_t  interval;
}__attribute__((packed)) usb_endpoint_desc_t;

typedef struct usb_hub_desc_t {
    uint8_t  length;
    uint8_t  type;
    uint8_t  num_ports;
    uint16_t characteristics;
    uint8_t  power_on_to_good;
    uint8_t  hub_control_current;
    uint8_t  device_removable_mask;
    uint8_t  port_pwr_ctrl_mask;
}__attribute__((packed)) usb_hub_desc_t;

typedef struct usb_hid_desc_t {
    uint8_t  length;
    uint8_t  type;
    uint16_t hid_version;
    uint8_t  country_code;
    uint8_t  num_descriptors;
    uint8_t  descriptor_type;
    uint16_t descriptor_length;
}__attribute__((packed)) usb_hid_desc_t;

typedef struct usb_string_desc_t {
    uint8_t  length;
    uint8_t  type;
    uint16_t string[];
}__attribute__((packed)) usb_string_desc_t;

typedef enum usb_request_type_t {
    USB_REQUEST_TYPE_STANDARD = 0x00,
    USB_REQUEST_TYPE_CLASS = 0x20,
    USB_REQUEST_TYPE_VENDOR = 0x40,
    USB_REQUEST_TYPE_MASK = 0x60,
} usb_request_type_t;

typedef enum usb_request_recipient_t {
    USB_REQUEST_RECIPIENT_DEVICE = 0x00,
    USB_REQUEST_RECIPIENT_INTERFACE = 0x01,
    USB_REQUEST_RECIPIENT_ENDPOINT = 0x02,
    USB_REQUEST_RECIPIENT_OTHER = 0x03,
    USB_REQUEST_RECIPIENT_MASK = 0x1F,
} usb_request_recipient_t;

typedef enum usb_request_device_t {
    USB_REQUEST_GET_STATUS = 0x00,
    USB_REQUEST_CLEAR_FEATURE = 0x01,
    USB_REQUEST_SET_FEATURE = 0x03,
    USB_REQUEST_SET_ADDRESS = 0x05,
    USB_REQUEST_GET_DESCRIPTOR = 0x06,
    USB_REQUEST_SET_DESCRIPTOR = 0x07,
    USB_REQUEST_GET_CONFIGURATION = 0x08,
    USB_REQUEST_SET_CONFIGURATION = 0x09,
    USB_REQUEST_GET_INTERFACE = 0x0A,
    USB_REQUEST_SET_INTERFACE = 0x0B,
    USB_REQUEST_SYNCH_FRAME = 0x0C,
} usb_request_device_t;

typedef enum usb_request_direction_t {
    USB_REQUEST_DIRECTION_HOST_TO_DEVICE = 0x00,
    USB_REQUEST_DIRECTION_DEVICE_TO_HOST = 0x80,
} usb_request_direction_t;

typedef enum usb_request_hub_t {
    USB_REQUEST_CLEAR_TT_BUFFER = 0x08,
    USB_REQUEST_RESET_TT = 0x09,
    USB_REQUEST_GET_TT_STATE = 0x0A,
    USB_REQUEST_STOP_TT = 0x0B,
} usb_request_hub_t;

typedef enum usb_request_interface_t {
    USB_REQUEST_GET_REPORT = 0x01,
    USB_REQUEST_GET_IDLE = 0x02,
    USB_REQUEST_GET_PROTOCOL = 0x03,
    USB_REQUEST_SET_REPORT = 0x09,
    USB_REQUEST_SET_IDLE = 0x0A,
    USB_REQUEST_SET_PROTOCOL = 0x0B,
} usb_request_interface_t;

typedef enum usb_standart_feature_selector_t {
    USB_FEATURE_DEVICE_REMOTE_WAKEUP = 0x01,
    USB_FEATURE_ENDPOINT_HALT = 0x00,
    USB_FEATURE_TEST_MODE = 0x02,
} usb_standart_feature_selector_t;

typedef enum usb_hub_feature_selector_t {
    USB_FEATURE_C_HUB_LOCAL_POWER = 0x00,
    USB_FEATURE_C_HUB_OVER_CURRENT = 0x01,
    USB_FEATURE_PORT_CONNECTION = 0x00,
    USB_FEATURE_PORT_ENABLE = 0x01,
    USB_FEATURE_PORT_SUSPEND = 0x02,
    USB_FEATURE_PORT_OVER_CURRENT = 0x03,
    USB_FEATURE_PORT_RESET = 0x04,
    USB_FEATURE_PORT_POWER = 0x08,
    USB_FEATURE_PORT_LOW_SPEED = 0x09,
    USB_FEATURE_C_PORT_CONNECTION = 0x10,
    USB_FEATURE_C_PORT_ENABLE = 0x11,
    USB_FEATURE_C_PORT_SUSPEND = 0x12,
    USB_FEATURE_C_PORT_OVER_CURRENT = 0x13,
    USB_FEATURE_C_PORT_RESET = 0x14,
    USB_FEATURE_PORT_TEST = 0x15,
    USB_FEATURE_PORT_INDICATOR = 0x16,
} usb_hub_feature_selector_t;

typedef struct usb_device_request_t {
    uint8_t  type;
    uint8_t  request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
}__attribute__((packed)) usb_device_request_t;

typedef struct usb_endpoint_t {
    usb_endpoint_desc_t* desc;
    uint32_t             toggle;
} usb_endpoint_t;

typedef struct usb_device_t usb_device_t;

typedef struct usb_transfer_t {
    usb_device_t*         device;
    usb_endpoint_t*       endpoint;
    usb_device_request_t* request;
    uint8_t*              data;
    uint32_t              length;
    boolean_t             complete;
    boolean_t             success;
} usb_transfer_t;

typedef struct usb_controller_t usb_controller_t;

typedef struct usb_controller_t {
    usb_controler_type_t         controller_type;
    uint64_t                     controller_id;
    const pci_dev_t*             pci_dev;
    const pci_capability_msix_t* msix_cap;
    usb_controller_metadata_t*   metadata;
    boolean_t                    initialized;
    int8_t (*probe_all_ports)(usb_controller_t* controller);
    int8_t (*probe_port)(usb_controller_t* controller, uint8_t port);
    int8_t (*reset_port)(usb_controller_t* controller, uint8_t port);
    int8_t (*control_transfer)(usb_controller_t* controller, usb_transfer_t* transfer);
} usb_controller_t;

typedef struct usb_device_t usb_device_t;

typedef struct usb_device_t {
    uint64_t              device_id;
    usb_device_t*         parent;
    usb_controller_t*     controller;
    uint32_t              port;
    uint32_t              speed;
    uint32_t              address;
    uint32_t              max_packet_size;
    usb_endpoint_t*       endpoint;
    usb_interface_desc_t* interface;
    usb_hid_desc_t*       hid;
    char_t*               vendor;
    char_t*               product;
    char_t*               serial;
} usb_device_t;

int8_t usb_init(void);

int8_t usb_device_init(usb_device_t* parent, usb_controller_t* controller, uint32_t port, uint32_t speed);

int8_t usb_probe_all_devices_all_ports(void);

#endif
