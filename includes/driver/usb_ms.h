/**
 * @file usb_mass_storage_disk.h
 * @brief USB Mass Storage Disk
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___USB_MASS_STORAGE_DISK_H
#define ___USB_MASS_STORAGE_DISK_H

#include <types.h>
#include <disk.h>
#include <driver/usb.h>
#include <driver/scsi.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t      usb_mass_storage_get_disk_count(void);
usb_driver_t* usb_mass_storage_get_disk_by_id(uint64_t id);

disk_t* usb_mass_storage_disk_impl_open(usb_driver_t* usb_ms, uint8_t lun);

#ifdef ___USB_MASS_STORAGE_IMPLEMENTATION

#define USB_MS_FLAG_DATA_OUT 0x00
#define USB_MS_FLAG_DATA_IN  0x80
#define USB_MS_FLAG_NO_DATA  0x00

boolean_t usb_ms_read_write(usb_driver_t* usb_driver, boolean_t read, uint32_t dtl, uint8_t* data);
boolean_t usb_ms_send_command(usb_driver_t* usb_driver, uint32_t dtl, uint8_t flags, uint8_t lun, uint8_t command_length, uint8_t* command);
boolean_t usb_ms_get_status(usb_driver_t* usb_driver);

int8_t usb_ms_inquiry(usb_driver_t* usb_ms);
int8_t usb_ms_sense(usb_driver_t* usb_ms, scsi_sense_data_t* sense);
int8_t usb_ms_test_unit_ready(usb_driver_t* usb_ms);
int8_t usb_ms_test_unit_ready_with_retry(usb_driver_t* usb_ms);


usb_driver_t* usb_ms_bulk_only_init(usb_device_t* usb_device);
boolean_t     usb_ms_bulk_only_read_write(usb_driver_t* usb_driver, boolean_t read, uint32_t dtl, uint8_t* data);
boolean_t     usb_ms_bulk_only_send_command(usb_driver_t* usb_driver, uint32_t dtl, uint8_t flags, uint8_t lun, uint8_t command_length, uint8_t* command);
boolean_t     usb_ms_bulk_only_get_status(usb_driver_t* usb_driver);


usb_driver_t* usb_ms_uas_init(usb_device_t* usb_device);
boolean_t     usb_ms_uas_read_write(usb_driver_t* usb_driver, boolean_t read, uint32_t dtl, uint8_t* data);
boolean_t     usb_ms_uas_send_command(usb_driver_t* usb_driver, uint32_t dtl, uint8_t flags, uint8_t lun, uint8_t command_length, uint8_t* command);
boolean_t     usb_ms_uas_get_status(usb_driver_t* usb_driver);


#endif



#ifdef __cplusplus
}
#endif

#endif /* ___USB_MASS_STORAGE_DISK_H */
