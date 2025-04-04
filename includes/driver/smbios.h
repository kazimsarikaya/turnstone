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
 * @struct smbios_entrypoint_32_t
 * @brief 32-bit SMBIOS entry point structure
 */
typedef struct smbios_entrypoint_32_t {
    char_t   anchor_string[4]; ///< _SM_
    uint8_t  checksum; ///< Entry point structure checksum
    uint8_t  length; ///< Entry point structure length
    uint8_t  major_version; ///< SMBIOS major version
    uint8_t  minor_version; ///< SMBIOS minor version
    uint16_t max_structure_size; ///< Maximum size of SMBIOS structure
    uint8_t  entry_point_revision; ///< Entry point structure revision
    uint8_t  formatted_area[5]; ///< Formatted area
    char_t   intermediate_anchor_string[5]; ///< _DMI_
    uint8_t  intermediate_checksum; ///< Intermediate checksum
    uint16_t structure_table_length; ///< Structure table length
    uint32_t structure_table_address; ///< Structure table address
    uint16_t number_of_smbios_structures; ///< Number of SMBIOS structures
    uint8_t  bcd_revision; ///< BCD revision
}__attribute__((packed)) smbios_entrypoint_32_t; ///< 32-bit SMBIOS entry point structure

/**
 * @struct smbios_entrypoint_64_t
 * @brief 64-bit SMBIOS entry point structure
 */
typedef struct smbios_entrypoint_64_t {
    char_t   anchor_string[5]; ///< _SM3_
    uint8_t  checksum; ///< Entry point structure checksum
    uint8_t  length; ///< Entry point structure length
    uint8_t  major_version; ///< SMBIOS major version
    uint8_t  minor_version; ///< SMBIOS minor version
    uint8_t  docrev; ///< SMBIOS docrev
    uint8_t  entry_point_revision; ///< Entry point structure revision
    uint8_t  reserved; ///< Reserved
    uint32_t structure_table_maximum_size; ///< Maximum size of SMBIOS structure
    uint64_t structure_table_address; ///< Structure table address
}__attribute__((packed)) smbios_entrypoint_64_t; ///< 64-bit SMBIOS entry point structure

/**
 * @enum smbios_structure_type_t
 * @brief SMBIOS structure type
 */
typedef enum smbios_structure_type_t {
    SMBIOS_STRUCTURE_TYPE_BIOS_INFORMATION = 0, ///< BIOS information
    SMBIOS_STRUCTURE_TYPE_SYSTEM_INFORMATION = 1, ///< System information
    SMBIOS_STRUCTURE_TYPE_BASEBOARD_INFORMATION = 2, ///< Baseboard information
    SMBIOS_STRUCTURE_TYPE_SYSTEM_ENCLOSURE_OR_CHASSIS = 3, ///< System enclosure or chassis
    SMBIOS_STRUCTURE_TYPE_PROCESSOR_INFORMATION = 4, ///< Processor information
    SMBIOS_STRUCTURE_TYPE_MEMORY_CONTROLLER_INFORMATION = 5, ///< Memory controller information
    SMBIOS_STRUCTURE_TYPE_MEMORY_MODULE_INFORMATION = 6, ///< Memory module information
    SMBIOS_STRUCTURE_TYPE_CACHE_INFORMATION = 7, ///< Cache information
    SMBIOS_STRUCTURE_TYPE_PORT_CONNECTOR_INFORMATION = 8, ///< Port connector information
    SMBIOS_STRUCTURE_TYPE_SYSTEM_SLOTS = 9, ///< System slots
    SMBIOS_STRUCTURE_TYPE_ONBOARD_DEVICES_INFORMATION = 10, ///< Onboard devices information
    SMBIOS_STRUCTURE_TYPE_OEM_STRINGS = 11, ///< OEM strings
    SMBIOS_STRUCTURE_TYPE_SYSTEM_CONFIGURATION_OPTIONS = 12, ///< System configuration options
    SMBIOS_STRUCTURE_TYPE_BIOS_LANGUAGE_INFORMATION = 13, ///< BIOS language information
    SMBIOS_STRUCTURE_TYPE_GROUP_ASSOCIATIONS = 14, ///< Group associations
    SMBIOS_STRUCTURE_TYPE_SYSTEM_EVENT_LOG = 15, ///< System event log
    SMBIOS_STRUCTURE_TYPE_PHYSICAL_MEMORY_ARRAY = 16, ///< Physical memory array
    SMBIOS_STRUCTURE_TYPE_MEMORY_DEVICE = 17, ///< Memory device
    SMBIOS_STRUCTURE_TYPE_32_BIT_MEMORY_ERROR_INFORMATION = 18, ///< 32-bit memory error information
    SMBIOS_STRUCTURE_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS = 19, ///< Memory array mapped address
    SMBIOS_STRUCTURE_TYPE_MEMORY_DEVICE_MAPPED_ADDRESS = 20, ///< Memory device mapped address
    SMBIOS_STRUCTURE_TYPE_BUILT_IN_POINTING_DEVICE = 21, ///< Built-in pointing device
    SMBIOS_STRUCTURE_TYPE_PORTABLE_BATTERY = 22, ///< Portable battery
    SMBIOS_STRUCTURE_TYPE_SYSTEM_RESET = 23, ///< System reset
    SMBIOS_STRUCTURE_TYPE_HARDWARE_SECURITY = 24, ///< Hardware security
    SMBIOS_STRUCTURE_TYPE_SYSTEM_POWER_CONTROLS = 25, ///< System power controls
    SMBIOS_STRUCTURE_TYPE_VOLTAGE_PROBE = 26, ///< Voltage probe
    SMBIOS_STRUCTURE_TYPE_COOLING_DEVICE = 27, ///< Cooling device
    SMBIOS_STRUCTURE_TYPE_TEMPERATURE_PROBE = 28, ///< Temperature probe
    SMBIOS_STRUCTURE_TYPE_ELECTRICAL_CURRENT_PROBE = 29, ///< Electrical current probe
    SMBIOS_STRUCTURE_TYPE_OUT_OF_BAND_REMOTE_ACCESS = 30, ///< Out-of-band remote access
    SMBIOS_STRUCTURE_TYPE_BOOT_INTEGRITY_SERVICES_BOOT_RECORD = 31, ///< Boot integrity services boot record
    SMBIOS_STRUCTURE_TYPE_SYSTEM_BOOT_INFORMATION = 32, ///< System boot information
    SMBIOS_STRUCTURE_TYPE_64_BIT_MEMORY_ERROR_INFORMATION = 33, ///< 64-bit memory error information
    SMBIOS_STRUCTURE_TYPE_MANAGEMENT_DEVICE = 34, ///< Management device
    SMBIOS_STRUCTURE_TYPE_MANAGEMENT_DEVICE_COMPONENT = 35, ///< Management device component
    SMBIOS_STRUCTURE_TYPE_MANAGEMENT_DEVICE_THRESHOLD_DATA = 36, ///< Management device threshold data
    SMBIOS_STRUCTURE_TYPE_MEMORY_CHANNEL = 37, ///< Memory channel
    SMBIOS_STRUCTURE_TYPE_IPMI_DEVICE_INFORMATION = 38, ///< IPMI device information
    SMBIOS_STRUCTURE_TYPE_SYSTEM_POWER_SUPPLY = 39, ///< System power supply
    SMBIOS_STRUCTURE_TYPE_ADDITIONAL_INFORMATION = 40, ///< Additional information
    SMBIOS_STRUCTURE_TYPE_ONBOARD_DEVICES_EXTENDED_INFORMATION = 41, ///< Onboard devices extended information
    SMBIOS_STRUCTURE_TYPE_MANAGEMENT_CONTROLLER_HOST_INTERFACE = 42, ///< Management controller host interface
    SMBIOS_STRUCTURE_TYPE_TPM_DEVICE = 43, ///< TPM device
    SMBIOS_STRUCTURE_TYPE_PROCESSOR_ADDITIONAL_INFORMATION = 44, ///< Processor additional information
    SMBIOS_STRUCTURE_TYPE_FIRMWARE_INVENTORY_INFORMATION = 45, ///< Firmware inventory information
    SMBIOS_STRUCTURE_TYPE_STRING_PROPERTY = 46, ///< String property
    SMBIOS_STRUCTURE_TYPE_INACTIVE = 126, ///< Inactive
    SMBIOS_STRUCTURE_TYPE_END_OF_TABLE = 127, ///< End of table
} smbios_structure_type_t; ///< SMBIOS structure type

/**
 * @struct smbios_structure_header_t
 * @brief SMBIOS structure header
 */
typedef struct smbios_structure_header_t {
    uint8_t  type; ///< Structure type
    uint8_t  length; ///< Structure length
    uint16_t handle; ///< Structure handle
}__attribute__((packed)) smbios_structure_header_t; ///< SMBIOS structure header


/**
 * @struct smbios_bios_information_t
 * @brief SMBIOS BIOS information structure
 */
typedef struct smbios_bios_information_t {
    smbios_structure_header_t header; ///< Structure header
    uint8_t                   vendor; ///< Vendor
    uint8_t                   version; ///< Version
    uint16_t                  starting_address_segment; ///< Starting address segment
    uint8_t                   release_date; ///< Release date
    uint8_t                   rom_size; ///< ROM size
    uint64_t                  characteristics; ///< Characteristics
    uint8_t                   characteristics_extension_bytes[2]; ///< Extension bytes
    uint8_t                   system_bios_major_release; ///< System BIOS major release
    uint8_t                   system_bios_minor_release; ///< System BIOS minor release
    uint8_t                   embedded_controller_firmware_major_release; ///< Embedded controller firmware major release
    uint8_t                   embedded_controller_firmware_minor_release; ///< Embedded controller firmware minor release
    uint16_t                  extended_bios_rom_size; ///< Extended BIOS ROM size
}__attribute__((packed)) smbios_bios_information_t; ///< SMBIOS BIOS information structure

/**
 * @struct smbios_system_information_t
 * @brief SMBIOS system information structure
 */
typedef struct smbios_system_information_t {
    smbios_structure_header_t header; ///< Structure header
    uint8_t                   manufacturer; ///< Manufacturer
    uint8_t                   product_name; ///< Product name
    uint8_t                   version; ///< Version
    uint8_t                   serial_number; ///< Serial number
    uint8_t                   uuid[16]; ///< UUID
    uint8_t                   wake_up_type; ///< Wake up type
    uint8_t                   sku_number; ///< SKU number
    uint8_t                   family; ///< Family
}__attribute__((packed)) smbios_system_information_t; ///< SMBIOS system information structure

/**
 * @struct smbios_baseboard_information_t
 * @brief SMBIOS baseboard information structure
 */
typedef struct smbios_baseboard_information_t {
    smbios_structure_header_t header; ///< Structure header
    uint8_t                   manufacturer; ///< Manufacturer
    uint8_t                   product; ///< Product
    uint8_t                   version; ///< Version
    uint8_t                   serial_number; ///< Serial number
    uint8_t                   asset_tag; ///< Asset tag
    uint8_t                   feature_flags; ///< Feature flags
    uint8_t                   location_in_chassis; ///< Location in chassis
    uint16_t                  chassis_handle; ///< Chassis handle
    uint8_t                   board_type; ///< Board type
    uint8_t                   number_of_contained_object_handles; ///< Number of contained object handles
    uint16_t                  contained_object_handles[]; ///< Contained object handles
}__attribute__((packed)) smbios_baseboard_information_t; ///< SMBIOS baseboard information structure

/**
 * @struct smbios_system_enclosure_or_chassis_t
 * @brief SMBIOS system enclosure or chassis structure
 */
typedef struct smbios_system_enclosure_or_chassis_t {
    smbios_structure_header_t header; ///< Structure header
    uint8_t                   manufacturer; ///< Manufacturer
    uint8_t                   chassis_type; ///< Chassis type
    uint8_t                   version; ///< Version
    uint8_t                   serial_number; ///< Serial number
    uint8_t                   asset_tag; ///< Asset tag
    uint8_t                   boot_up_state; ///< Boot up state
    uint8_t                   power_supply_state; ///< Power supply state
    uint8_t                   thermal_state; ///< Thermal state
    uint8_t                   security_status; ///< Security status
    uint32_t                  oem_defined; ///< OEM defined
    uint8_t                   height; ///< Height
    uint8_t                   number_of_power_cords; ///< Number of power cords
    uint8_t                   contained_element_count; ///< Contained element count
    uint8_t                   contained_element_record_length; ///< Contained element record length
    uint8_t                   contained_elements[]; ///< Contained elements
    // uint8_t sku_number; ///< SKU number because of contained elements, this cannot be defined as a member
}__attribute__((packed)) smbios_system_enclosure_or_chassis_t; ///< SMBIOS system enclosure or chassis structure

/**
 * @struct smbios_processor_information_t
 * @brief SMBIOS processor information structure
 */
typedef struct smbios_processor_information_t {
    smbios_structure_header_t header; ///< Structure header
    uint8_t                   socket_designation; ///< Socket designation
    uint8_t                   processor_type; ///< Processor type
    uint8_t                   processor_family; ///< Processor family
    uint8_t                   processor_manufacturer; ///< Processor manufacturer
    uint64_t                  processor_id; ///< Processor ID
    uint8_t                   processor_version; ///< Processor version
    uint8_t                   voltage; ///< Voltage
    uint16_t                  external_clock; ///< External clock
    uint16_t                  max_speed; ///< Max speed
    uint16_t                  current_speed; ///< Current speed
    uint8_t                   status; ///< Status
    uint8_t                   processor_upgrade; ///< Processor upgrade
    uint16_t                  l1_cache_handle; ///< L1 cache handle
    uint16_t                  l2_cache_handle; ///< L2 cache handle
    uint16_t                  l3_cache_handle; ///< L3 cache handle
    uint8_t                   serial_number; ///< Serial number
    uint8_t                   asset_tag; ///< Asset tag
    uint8_t                   part_number; ///< Part number
    uint8_t                   core_count; ///< Core count
    uint8_t                   core_enabled; ///< Core enabled
    uint8_t                   thread_count; ///< Thread count
    uint16_t                  processor_characteristics; ///< Processor characteristics
    uint16_t                  processor_family_2; ///< Processor family 2
    uint16_t                  core_count_2; ///< Core count if real core count is greater than 255
    uint16_t                  core_enabled_2; ///< Core enabled if real core count is greater than 255
    uint16_t                  thread_count_2; ///< Thread count if real thread count is greater than 255
}__attribute__((packed)) smbios_processor_information_t; ///< SMBIOS processor information structure

/**
 * @struct smbios_cache_information_t
 * @brief SMBIOS cache information structure
 */
typedef struct smbios_cache_information_t {
    smbios_structure_header_t header; ///< Structure header
    uint8_t                   socket_designation; ///< Socket designation
    uint16_t                  cache_configuration; ///< Cache configuration
    uint16_t                  maximum_cache_size; ///< Maximum cache size
    uint16_t                  installed_size; ///< Installed size
    uint16_t                  supported_sram_type; ///< Supported SRAM type
    uint16_t                  current_sram_type; ///< Current SRAM type
    uint8_t                   cache_speed; ///< Cache speed
    uint8_t                   error_correction_type; ///< Error correction type
    uint8_t                   system_cache_type; ///< System cache type
    uint8_t                   associativity; ///< Associativity
    uint32_t                  maximum_cache_size_2; ///< Maximum cache size 2 (if maximum cache size is greater than 2047MB)
    uint32_t                  installed_size_2; ///< Installed size 2 (if installed size is greater than 2047MB)
}__attribute__((packed)) smbios_cache_information_t; ///< SMBIOS cache information structure

/**
 * @struct smbios_port_connector_information_t
 * @brief SMBIOS port connector information structure
 */
typedef struct smbios_port_connector_information_t {
    smbios_structure_header_t header; ///< Structure header
    uint8_t                   internal_reference_designator; ///< Internal reference designator
    uint8_t                   internal_connector_type; ///< Internal connector type
    uint8_t                   external_reference_designator; ///< External reference designator
    uint8_t                   external_connector_type; ///< External connector type
    uint8_t                   port_type; ///< Port type
}__attribute__((packed)) smbios_port_connector_information_t; ///< SMBIOS port connector information structure

/**
 * @struct smbios_system_slots_t
 * @brief SMBIOS system slots structure
 */
typedef struct smbios_system_slots_t {
    smbios_structure_header_t header; ///< Structure header
    uint8_t                   slot_designation; ///< Slot designation
    uint8_t                   slot_type; ///< Slot type
    uint8_t                   slot_data_bus_width; ///< Slot data bus width
    uint8_t                   current_usage; ///< Current usage
    uint8_t                   slot_length; ///< Slot length
    uint16_t                  slot_id; ///< Slot ID
    uint8_t                   slot_characteristics1; ///< Slot characteristics 1
    uint8_t                   slot_characteristics2; ///< Slot characteristics 2
    uint16_t                  segment_group_number; ///< Segment group number
    uint8_t                   bus_number; ///< Bus number
    uint8_t                   device_function_number; ///< Device function number
    uint8_t                   data_bus_width; ///< Data bus width
    uint8_t                   peer_grouping_count; ///< Peer grouping count (segment group number, bus number, device number, function number, data bus width)
    uint8_t                   peer_grouping[]; ///< Peer grouping
    // uint8_t slot_information; ///< Slot information because of peer grouping, this cannot be defined as a member
    // uint8_t slot_physical_width; ///< Slot physical width because of peer grouping, this cannot be defined as a member
    // uint16_t slot_pitch; ///< Slot pitch because of peer grouping, this cannot be defined as a member
    // uint8_t slot_height; ///< Slot height because of peer grouping, this cannot be defined as a member
}__attribute__((packed)) smbios_system_slots_t; ///< SMBIOS system slots structure






















#ifdef __cplusplus
}
#endif

#endif // smbios.h
