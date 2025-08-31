/**
 * @file usb_mouse.64.c
 * @brief USB mouse driver
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/usb.h>
#include <memory.h>
#include <logging.h>
#include <utils.h>
#include <graphics/screen.h>
#include <device/mouse.h>
#include <pipeline.h>

MODULE("turnstone.kernel.hw.usb.kbd");

typedef struct usb_mouse_report_t {
    struct {
        uint8_t left_button   : 1;
        uint8_t right_button  : 1;
        uint8_t middle_button : 1;
        uint8_t reserved      : 5;
    } __attribute__((packed)) buttons;
    int8_t x;
    int8_t y;
    int8_t wheel;
} __attribute__((packed)) usb_mouse_report_t;

typedef struct usb_qemu_tablet_report_t {
    struct {
        uint8_t left_button   : 1;
        uint8_t right_button  : 1;
        uint8_t middle_button : 1;
        uint8_t reserved      : 5;
    } __attribute__((packed)) buttons;
    int16_t x;
    int16_t y;
    int8_t  wheel;
} __attribute__((packed)) usb_qemu_tablet_report_t;

_Static_assert(sizeof(usb_mouse_report_t) == 4, "usb_mouse_report_t is not 4 bytes");
_Static_assert(sizeof(usb_qemu_tablet_report_t) == 6, "usb_qemu_tablet_report_t is not 6 bytes");

typedef struct usb_driver_t {
    usb_device_t*            usb_device;
    usb_interface_t*         interface;
    usb_pipeline_callback_f  pipeline_callback;
    uint32_t                 expected_packet_size;
    usb_mouse_report_t       old_usb_mouse_report;
    usb_mouse_report_t       new_usb_mouse_report;
    usb_qemu_tablet_report_t old_usb_qemu_tablet_report;
    usb_qemu_tablet_report_t new_usb_qemu_tablet_report;
    usb_transfer_t*          usb_transfer;
    uint32_t                 max_packet_size;
} usb_driver_t;

static void usb_mouse_handle_report(usb_driver_t* usb_mouse) {
    // TODO: this report relative movement, need to convert to absolute

    memory_memcopy(&usb_mouse->new_usb_mouse_report,
                   &usb_mouse->old_usb_mouse_report,
                   sizeof(usb_mouse_report_t));

    return;
}

static int8_t usb_mouse_pipeline_callback(const usb_driver_t* driver, uint8_t endpoint, pipeline_t* pipeline) {
    UNUSED(endpoint);
    usb_driver_t* usb_mouse = (usb_driver_t*)driver;

    uint64_t rc = pipeline_read(pipeline,
                                sizeof(usb_mouse_report_t),
                                (uint8_t*)&usb_mouse->new_usb_mouse_report);

    if(rc == sizeof(usb_mouse_report_t)) {
        usb_mouse_handle_report(usb_mouse);
    } else if(rc > 0) {
        PRINTLOG(USB, LOG_WARNING, "short read %llx/%li", rc, sizeof(usb_mouse_report_t));
    } else {
        PRINTLOG(USB, LOG_ERROR, "cannot read from pipeline");
    }

    memory_memclean(&usb_mouse->new_usb_mouse_report, sizeof(usb_mouse_report_t));

    return 0;
}

static void usb_qemu_tablet_handle_report(usb_driver_t* usb_qemu_tablet) {
    screen_info_t screen_info = screen_get_info();

    float32_t x_ratio = (float32_t)screen_info.width / 32767.0f;
    float32_t y_ratio = (float32_t)screen_info.height / 32767.0f;

    usb_qemu_tablet->new_usb_qemu_tablet_report.x = (int16_t)((float32_t)usb_qemu_tablet->new_usb_qemu_tablet_report.x * x_ratio);
    usb_qemu_tablet->new_usb_qemu_tablet_report.y = (int16_t)((float32_t)usb_qemu_tablet->new_usb_qemu_tablet_report.y * y_ratio);

    mouse_report_t report = {0};

    if(usb_qemu_tablet->new_usb_qemu_tablet_report.buttons.left_button) {
        report.buttons |= MOUSE_BUTTON_LEFT;
    }

    if(usb_qemu_tablet->new_usb_qemu_tablet_report.buttons.right_button) {
        report.buttons |= MOUSE_BUTTON_RIGHT;
    }

    if(usb_qemu_tablet->new_usb_qemu_tablet_report.buttons.middle_button) {
        report.buttons |= MOUSE_BUTTON_MIDDLE;
    }

    report.x = usb_qemu_tablet->new_usb_qemu_tablet_report.x;
    report.y = usb_qemu_tablet->new_usb_qemu_tablet_report.y;
    report.wheel = usb_qemu_tablet->new_usb_qemu_tablet_report.wheel;

    mouse_report(&report);

    memory_memcopy(&usb_qemu_tablet->new_usb_qemu_tablet_report, &usb_qemu_tablet->old_usb_qemu_tablet_report, sizeof(usb_qemu_tablet_report_t));
}

static int8_t usb_qemu_tablet_pipeline_callback(const usb_driver_t* driver, uint8_t endpoint, pipeline_t* pipeline) {
    UNUSED(endpoint);
    usb_driver_t* usb_qemu_tablet = (usb_driver_t*)driver;

    uint64_t rc = pipeline_read(pipeline,
                                sizeof(usb_qemu_tablet_report_t),
                                (uint8_t*)&usb_qemu_tablet->new_usb_qemu_tablet_report);

    if(rc == sizeof(usb_qemu_tablet_report_t)) {
        usb_qemu_tablet_handle_report(usb_qemu_tablet);
    } else if(rc > 0) {
        PRINTLOG(USB, LOG_WARNING, "short read %llx/%li", rc, sizeof(usb_qemu_tablet_report_t));
    } else {
        PRINTLOG(USB, LOG_ERROR, "cannot read from pipeline");
    }

    memory_memclean(&usb_qemu_tablet->new_usb_qemu_tablet_report, sizeof(usb_qemu_tablet_report_t));

    return 0;
}

int8_t usb_mouse_init(usb_device_t* usb_device, usb_interface_t* interface) {
    PRINTLOG(USB, LOG_INFO, "initializing usb mouse");
    usb_driver_t* usb_mouse = memory_malloc(sizeof(usb_driver_t));

    if(!usb_mouse) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb mouse");

        return -1;
    }

    usb_mouse->usb_device = usb_device;
    interface->driver = usb_mouse;

    usb_mouse->usb_transfer = memory_malloc(sizeof(usb_transfer_t));

    if(!usb_mouse->usb_transfer) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb transfer");
        memory_free(usb_mouse);

        return -1;
    }

    usb_mouse->usb_transfer->driver = usb_mouse;
    usb_mouse->interface = interface;
    usb_mouse->usb_transfer->endpoint = interface->endpoints[0];

    usb_mouse->max_packet_size = usb_mouse->usb_transfer->endpoint->desc->max_packet_size;
    usb_mouse->expected_packet_size = sizeof(usb_mouse_report_t);

    pipeline_t* pipeline = pipeline_create(usb_mouse->expected_packet_size * 1024);

    if(!pipeline) {
        PRINTLOG(USB, LOG_ERROR, "cannot create pipeline");
        memory_free(usb_mouse->usb_transfer);
        memory_free(usb_mouse);

        return -1;
    }

    if(!usb_device_request(usb_device,
                           interface,
                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_ENDPOINT,
                           USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_ENDPOINT_SETUP_PIPELINE,
                           usb_mouse->expected_packet_size, usb_mouse->usb_transfer->endpoint->desc->endpoint_address,
                           0, pipeline)) {
        PRINTLOG(USB, LOG_ERROR, "cannot setup endpoint pipeline");
        memory_free(usb_mouse->usb_transfer);
        memory_free(usb_mouse);


        return -1;
    }


    if (!usb_device_request(usb_device,
                            NULL,
                            USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                            USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_SET_IDLE,
                            0, interface->desc->interface_number, 0, NULL)) {
        PRINTLOG(USB, LOG_ERROR, "cannot set idle");
        memory_free(usb_mouse->usb_transfer);
        memory_free(usb_mouse);

        return -1;
    }

    if(usb_device->controller->controller_type == USB_CONTROLLER_TYPE_XHCI) {
        memory_free(usb_mouse->usb_transfer);
        usb_mouse->usb_transfer = NULL;
        usb_mouse->pipeline_callback = usb_mouse_pipeline_callback;
    } else {
        PRINTLOG(USB, LOG_ERROR, "unknown controller type %d", usb_device->controller->controller_type);
        memory_free(usb_mouse->usb_transfer);
        memory_free(usb_mouse);

        return -1;
    }

    PRINTLOG(USB, LOG_INFO, "usb mouse initialized");

    return 0;
}

int8_t usb_qemu_tablet_init(usb_device_t* usb_device, usb_interface_t* interface) {
    PRINTLOG(USB, LOG_INFO, "initializing usb qemu_tablet");
    usb_driver_t* usb_qemu_tablet = memory_malloc(sizeof(usb_driver_t));

    if(!usb_qemu_tablet) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb qemu_tablet");

        return -1;
    }

    usb_qemu_tablet->usb_device = usb_device;
    usb_qemu_tablet->interface = interface;
    interface->driver = usb_qemu_tablet;

    usb_qemu_tablet->usb_transfer = memory_malloc(sizeof(usb_transfer_t));

    if(!usb_qemu_tablet->usb_transfer) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb transfer");
        memory_free(usb_qemu_tablet);

        return -1;
    }

    usb_qemu_tablet->usb_transfer->driver = usb_qemu_tablet;
    usb_qemu_tablet->usb_transfer->endpoint = interface->endpoints[0];

    usb_qemu_tablet->max_packet_size = usb_qemu_tablet->usb_transfer->endpoint->desc->max_packet_size;
    usb_qemu_tablet->expected_packet_size = sizeof(usb_qemu_tablet_report_t);

    pipeline_t* pipeline = pipeline_create(usb_qemu_tablet->expected_packet_size * 1024);

    if(!pipeline) {
        PRINTLOG(USB, LOG_ERROR, "cannot create pipeline");
        memory_free(usb_qemu_tablet->usb_transfer);
        memory_free(usb_qemu_tablet);

        return -1;
    }

    if(!usb_device_request(usb_device,
                           interface,
                           USB_REQUEST_TYPE_STANDARD, USB_REQUEST_RECIPIENT_ENDPOINT,
                           USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_ENDPOINT_SETUP_PIPELINE,
                           usb_qemu_tablet->expected_packet_size, usb_qemu_tablet->usb_transfer->endpoint->desc->endpoint_address,
                           0, pipeline)) {
        PRINTLOG(USB, LOG_ERROR, "cannot setup endpoint pipeline");
        memory_free(usb_qemu_tablet->usb_transfer);
        memory_free(usb_qemu_tablet);


        return -1;
    }


    if (!usb_device_request(usb_device,
                            NULL,
                            USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                            USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_SET_IDLE,
                            0, interface->desc->interface_number, 0, NULL)) {
        PRINTLOG(USB, LOG_ERROR, "cannot set idle");
        memory_free(usb_qemu_tablet->usb_transfer);
        memory_free(usb_qemu_tablet);

        return -1;
    }

    if(usb_device->controller->controller_type == USB_CONTROLLER_TYPE_XHCI) {
        memory_free(usb_qemu_tablet->usb_transfer);
        usb_qemu_tablet->usb_transfer = NULL;
        usb_qemu_tablet->pipeline_callback = usb_qemu_tablet_pipeline_callback;
    } else {
        PRINTLOG(USB, LOG_ERROR, "unknown controller type %d", usb_device->controller->controller_type);
        memory_free(usb_qemu_tablet->usb_transfer);
        memory_free(usb_qemu_tablet);

        return -1;
    }


    PRINTLOG(USB, LOG_INFO, "usb qemu_tablet initialized");

    return 0;
}
