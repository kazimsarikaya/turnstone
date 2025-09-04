/**
 * @file usb_vendor.h
 * @brief USB vendor related definitions
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___USB_VENDOR_H
#define ___USB_VENDOR_H

#include <types.h>
#include <driver/usb.h>


#define USB_VENDOR_ID_TP_LINK        0x2357
#define USB_PRODUCT_ID_TP_LINK_UE300 0x0601

#ifdef __cplusplus
extern "C" {
#endif

int8_t usb_vendor_read(usb_driver_t* usb_driver,
                       uint8_t       request,
                       uint16_t      value,
                       uint16_t      index,
                       void*         buf,
                       uint16_t      len);

int8_t usb_vendor_write(usb_driver_t* usb_driver,
                        uint8_t       request,
                        uint16_t      value,
                        uint16_t      index,
                        void*         buf,
                        uint16_t      len);

int8_t usb_device_rtl815x_init(usb_device_t* device, usb_interface_t* interface);


#ifdef __cplusplus
}
#endif

#endif
