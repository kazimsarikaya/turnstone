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

#ifdef __cplusplus
extern "C" {
#endif

uint64_t      usb_mass_storage_get_disk_count(void);
usb_driver_t* usb_mass_storage_get_disk_by_id(uint64_t id);

disk_t* usb_mass_storage_disk_impl_open(usb_driver_t* usb_mass_storage, uint8_t lun);

#ifdef __cplusplus
}
#endif

#endif /* ___USB_MASS_STORAGE_DISK_H */
