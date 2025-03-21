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

typedef struct usb_driver_t {
    usb_device_t*            usb_device;
    usb_mouse_report_t       old_usb_mouse_report;
    usb_mouse_report_t       new_usb_mouse_report;
    usb_qemu_tablet_report_t old_usb_qemu_tablet_report;
    usb_qemu_tablet_report_t new_usb_qemu_tablet_report;
    usb_transfer_t*          usb_transfer;
} usb_driver_t;

static int8_t usb_mouse_transfer_cb(usb_controller_t* usb_controller, usb_transfer_t* usb_transfer) {
    usb_driver_t* usb_mouse = usb_transfer->device->driver;

    if(usb_transfer->success) {
        // TODO: this report relative movement, need to convert to absolute

        memory_memcopy(&usb_mouse->new_usb_mouse_report, &usb_mouse->old_usb_mouse_report, sizeof(usb_mouse_report_t));
    } else {
        PRINTLOG(USB, LOG_ERROR, "transfer failed");
    }

    memory_memclean(&usb_mouse->new_usb_mouse_report, sizeof(usb_mouse_report_t));
    usb_transfer->complete = false;
    usb_transfer->success = false;

    if(usb_controller->bulk_transfer(usb_controller, usb_transfer) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot start bulk transfer");

        return -1;
    }

    return 0;
}

static int8_t usb_qemu_tablet_transfer_cb(usb_controller_t* usb_controller, usb_transfer_t* usb_transfer) {
    usb_driver_t* usb_qemu_tablet = usb_transfer->device->driver;

    screen_info_t screen_info = screen_get_info();

    if(usb_transfer->success) {
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
    } else {
        PRINTLOG(USB, LOG_ERROR, "transfer failed");
    }

    memory_memclean(&usb_qemu_tablet->new_usb_qemu_tablet_report, sizeof(usb_qemu_tablet_report_t));
    usb_transfer->complete = false;
    usb_transfer->success = false;

    if(usb_controller->bulk_transfer(usb_controller, usb_transfer) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot start bulk transfer");

        return -1;
    }

    return 0;
}

int8_t usb_mouse_init(usb_device_t* usb_device) {
    usb_driver_t* usb_mouse = memory_malloc(sizeof(usb_driver_t));

    if(!usb_mouse) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb mouse");

        return -1;
    }

    usb_mouse->usb_device = usb_device;
    usb_device->driver = usb_mouse;

    usb_mouse->usb_transfer = memory_malloc(sizeof(usb_transfer_t));

    if(!usb_mouse->usb_transfer) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb transfer");
        memory_free(usb_mouse);

        return -1;
    }

    usb_mouse->usb_transfer->device = usb_device;
    usb_mouse->usb_transfer->endpoint = usb_device->configurations[usb_device->selected_config]->endpoints[0];
    usb_mouse->usb_transfer->transfer_callback = usb_mouse_transfer_cb;


    if (!usb_device_request(usb_device,
                            USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                            USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_SET_IDLE,
                            0, usb_device->configurations[usb_device->selected_config]->interface->interface_number, 0, NULL)) {
        PRINTLOG(USB, LOG_ERROR, "cannot set idle");
        memory_free(usb_mouse->usb_transfer);
        memory_free(usb_mouse);

        return -1;
    }

    usb_mouse->usb_transfer->data = (uint8_t*)&usb_mouse->new_usb_mouse_report;
    usb_mouse->usb_transfer->length = sizeof(usb_mouse_report_t);



    if(usb_device->controller->bulk_transfer(usb_device->controller, usb_mouse->usb_transfer) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot start bulk transfer");
        memory_free(usb_mouse->usb_transfer);
        memory_free(usb_mouse);

        return -1;
    }

    PRINTLOG(USB, LOG_INFO, "usb mouse initialized");

    return 0;
}

int8_t usb_qemu_tablet_init(usb_device_t* usb_device) {
    usb_driver_t* usb_qemu_tablet = memory_malloc(sizeof(usb_driver_t));

    if(!usb_qemu_tablet) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb qemu_tablet");

        return -1;
    }

    usb_qemu_tablet->usb_device = usb_device;
    usb_device->driver = usb_qemu_tablet;

    usb_qemu_tablet->usb_transfer = memory_malloc(sizeof(usb_transfer_t));

    if(!usb_qemu_tablet->usb_transfer) {
        PRINTLOG(USB, LOG_ERROR, "cannot allocate memory for usb transfer");
        memory_free(usb_qemu_tablet);

        return -1;
    }

    usb_qemu_tablet->usb_transfer->device = usb_device;
    usb_qemu_tablet->usb_transfer->endpoint = usb_device->configurations[usb_device->selected_config]->endpoints[0];
    usb_qemu_tablet->usb_transfer->transfer_callback = usb_qemu_tablet_transfer_cb;


    if (!usb_device_request(usb_device,
                            USB_REQUEST_TYPE_CLASS, USB_REQUEST_RECIPIENT_INTERFACE,
                            USB_REQUEST_DIRECTION_HOST_TO_DEVICE, USB_REQUEST_SET_IDLE,
                            0, usb_device->configurations[usb_device->selected_config]->interface->interface_number, 0, NULL)) {
        PRINTLOG(USB, LOG_ERROR, "cannot set idle");
        memory_free(usb_qemu_tablet->usb_transfer);
        memory_free(usb_qemu_tablet);

        return -1;
    }

    usb_qemu_tablet->usb_transfer->data = (uint8_t*)&usb_qemu_tablet->new_usb_qemu_tablet_report;
    usb_qemu_tablet->usb_transfer->length = sizeof(usb_qemu_tablet_report_t);



    if(usb_device->controller->bulk_transfer(usb_device->controller, usb_qemu_tablet->usb_transfer) != 0) {
        PRINTLOG(USB, LOG_ERROR, "cannot start bulk transfer");
        memory_free(usb_qemu_tablet->usb_transfer);
        memory_free(usb_qemu_tablet);

        return -1;
    }

    PRINTLOG(USB, LOG_INFO, "usb qemu_tablet initialized");

    return 0;
}
