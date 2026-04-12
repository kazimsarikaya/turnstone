/**
 * @file smbios.h
 * @brief SM BIOS header file
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___SMBIOS_H
/*! macro for avoiding multiple inclusion error */
#define ___SMBIOS_H 0

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @enum smbios_structure_type_t
 * @brief SMBIOS structure type
 */
typedef enum smbios_structure_type_t {
    SMBIOS_STRUCTURE_TYPE_BIOS_INFORMATION                     = 0, ///< BIOS information
    SMBIOS_STRUCTURE_TYPE_SYSTEM_INFORMATION                   = 1, ///< System information
    SMBIOS_STRUCTURE_TYPE_BASEBOARD_INFORMATION                = 2, ///< Baseboard information
    SMBIOS_STRUCTURE_TYPE_SYSTEM_ENCLOSURE_OR_CHASSIS          = 3, ///< System enclosure or chassis
    SMBIOS_STRUCTURE_TYPE_PROCESSOR_INFORMATION                = 4, ///< Processor information
    SMBIOS_STRUCTURE_TYPE_MEMORY_CONTROLLER_INFORMATION        = 5, ///< Memory controller information
    SMBIOS_STRUCTURE_TYPE_MEMORY_MODULE_INFORMATION            = 6, ///< Memory module information
    SMBIOS_STRUCTURE_TYPE_CACHE_INFORMATION                    = 7, ///< Cache information
    SMBIOS_STRUCTURE_TYPE_PORT_CONNECTOR_INFORMATION           = 8, ///< Port connector information
    SMBIOS_STRUCTURE_TYPE_SYSTEM_SLOTS                         = 9, ///< System slots
    SMBIOS_STRUCTURE_TYPE_ONBOARD_DEVICES_INFORMATION          = 10, ///< Onboard devices information
    SMBIOS_STRUCTURE_TYPE_OEM_STRINGS                          = 11, ///< OEM strings
    SMBIOS_STRUCTURE_TYPE_SYSTEM_CONFIGURATION_OPTIONS         = 12, ///< System configuration options
    SMBIOS_STRUCTURE_TYPE_BIOS_LANGUAGE_INFORMATION            = 13, ///< BIOS language information
    SMBIOS_STRUCTURE_TYPE_GROUP_ASSOCIATIONS                   = 14, ///< Group associations
    SMBIOS_STRUCTURE_TYPE_SYSTEM_EVENT_LOG                     = 15, ///< System event log
    SMBIOS_STRUCTURE_TYPE_PHYSICAL_MEMORY_ARRAY                = 16, ///< Physical memory array
    SMBIOS_STRUCTURE_TYPE_MEMORY_DEVICE                        = 17, ///< Memory device
    SMBIOS_STRUCTURE_TYPE_32_BIT_MEMORY_ERROR_INFORMATION      = 18, ///< 32-bit memory error information
    SMBIOS_STRUCTURE_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS          = 19, ///< Memory array mapped address
    SMBIOS_STRUCTURE_TYPE_MEMORY_DEVICE_MAPPED_ADDRESS         = 20, ///< Memory device mapped address
    SMBIOS_STRUCTURE_TYPE_BUILT_IN_POINTING_DEVICE             = 21, ///< Built-in pointing device
    SMBIOS_STRUCTURE_TYPE_PORTABLE_BATTERY                     = 22, ///< Portable battery
    SMBIOS_STRUCTURE_TYPE_SYSTEM_RESET                         = 23, ///< System reset
    SMBIOS_STRUCTURE_TYPE_HARDWARE_SECURITY                    = 24, ///< Hardware security
    SMBIOS_STRUCTURE_TYPE_SYSTEM_POWER_CONTROLS                = 25, ///< System power controls
    SMBIOS_STRUCTURE_TYPE_VOLTAGE_PROBE                        = 26, ///< Voltage probe
    SMBIOS_STRUCTURE_TYPE_COOLING_DEVICE                       = 27, ///< Cooling device
    SMBIOS_STRUCTURE_TYPE_TEMPERATURE_PROBE                    = 28, ///< Temperature probe
    SMBIOS_STRUCTURE_TYPE_ELECTRICAL_CURRENT_PROBE             = 29, ///< Electrical current probe
    SMBIOS_STRUCTURE_TYPE_OUT_OF_BAND_REMOTE_ACCESS            = 30, ///< Out-of-band remote access
    SMBIOS_STRUCTURE_TYPE_BOOT_INTEGRITY_SERVICES_BOOT_RECORD  = 31, ///< Boot integrity services boot record
    SMBIOS_STRUCTURE_TYPE_SYSTEM_BOOT_INFORMATION              = 32, ///< System boot information
    SMBIOS_STRUCTURE_TYPE_64_BIT_MEMORY_ERROR_INFORMATION      = 33, ///< 64-bit memory error information
    SMBIOS_STRUCTURE_TYPE_MANAGEMENT_DEVICE                    = 34, ///< Management device
    SMBIOS_STRUCTURE_TYPE_MANAGEMENT_DEVICE_COMPONENT          = 35, ///< Management device component
    SMBIOS_STRUCTURE_TYPE_MANAGEMENT_DEVICE_THRESHOLD_DATA     = 36, ///< Management device threshold data
    SMBIOS_STRUCTURE_TYPE_MEMORY_CHANNEL                       = 37, ///< Memory channel
    SMBIOS_STRUCTURE_TYPE_IPMI_DEVICE_INFORMATION              = 38, ///< IPMI device information
    SMBIOS_STRUCTURE_TYPE_SYSTEM_POWER_SUPPLY                  = 39, ///< System power supply
    SMBIOS_STRUCTURE_TYPE_ADDITIONAL_INFORMATION               = 40, ///< Additional information
    SMBIOS_STRUCTURE_TYPE_ONBOARD_DEVICES_EXTENDED_INFORMATION = 41, ///< Onboard devices extended information
    SMBIOS_STRUCTURE_TYPE_MANAGEMENT_CONTROLLER_HOST_INTERFACE = 42, ///< Management controller host interface
    SMBIOS_STRUCTURE_TYPE_TPM_DEVICE                           = 43, ///< TPM device
    SMBIOS_STRUCTURE_TYPE_PROCESSOR_ADDITIONAL_INFORMATION     = 44, ///< Processor additional information
    SMBIOS_STRUCTURE_TYPE_FIRMWARE_INVENTORY_INFORMATION       = 45, ///< Firmware inventory information
    SMBIOS_STRUCTURE_TYPE_STRING_PROPERTY                      = 46, ///< String property
    SMBIOS_STRUCTURE_TYPE_INACTIVE                             = 126, ///< Inactive
    SMBIOS_STRUCTURE_TYPE_END_OF_TABLE                         = 127, ///< End of table
} smbios_structure_type_t; ///< SMBIOS structure type

int8_t smbios_print_all_structures(void);
int8_t smbios_print_structure(smbios_structure_type_t type);

int8_t smbios_print_all_structures_from_raw_data(uint8_t* smbios_table, uint8_t major_version, uint8_t minor_version);


#ifdef __cplusplus
}
#endif

#endif // smbios.h
