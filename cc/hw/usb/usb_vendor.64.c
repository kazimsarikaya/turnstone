/**
 * @file usb_vendor.64.c
 * @brief USB vendor specific functions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/usb_vendor.h>
#include <logging.h>

MODULE("turnstone.kernel.hw.usb");

typedef struct usb_driver_t {
    USB_DRIVER_COMMON_FIELDS
} usb_driver_t;

int8_t usb_vendor_read(usb_driver_t* usb_driver,
                       uint8_t       request,
                       uint16_t      value,
                       uint16_t      index,
                       void*         buf,
                       uint16_t      len) {
    if (!usb_driver || !buf) {
        PRINTLOG(USB, LOG_ERROR, "invalid parameters");
        return -1;
    }

    if (!usb_device_request(usb_driver->usb_device,
                            usb_driver->interface,
                            USB_REQUEST_TYPE_VENDOR,
                            USB_REQUEST_RECIPIENT_DEVICE,
                            USB_REQUEST_DIRECTION_DEVICE_TO_HOST,
                            request,
                            value,
                            index,
                            len,
                            buf)) {
        PRINTLOG(USB, LOG_ERROR, "vendor read req=0x%x val=0x%x idx=0x%x len=%d failed",
                 request, value, index, len);
        return -1;
    }

    return 0;
}

int8_t usb_vendor_write(usb_driver_t* usb_driver,
                        uint8_t       request,
                        uint16_t      value,
                        uint16_t      index,
                        void*         buf,
                        uint16_t      len) {

    if (!usb_driver || !buf) {
        PRINTLOG(USB, LOG_ERROR, "invalid parameters");
        return -1;
    }

    if (!usb_device_request(usb_driver->usb_device,
                            usb_driver->interface,
                            USB_REQUEST_TYPE_VENDOR,
                            USB_REQUEST_RECIPIENT_DEVICE,
                            USB_REQUEST_DIRECTION_HOST_TO_DEVICE,
                            request,
                            value,
                            index,
                            len,
                            buf)) {
        PRINTLOG(USB, LOG_ERROR, "vendor write req=0x%x val=0x%x idx=0x%x len=%d failed",
                 request, value, index, len);
        return -1;
    }

    return 0;
}

