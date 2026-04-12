/**
 * @file smbios.64.c
 * @brief SMBIOS parser
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <driver/smbios.h>
#include <systeminfo.h>
#include <logging.h>
#include <stdbufs.h>
#include <strings.h>

MODULE("turnstone.kernel.hw.smbios");

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
 * @struct smbios_structure_header_t
 * @brief SMBIOS structure header
 */
typedef struct smbios_structure_header_t {
    uint8_t  type; ///< Structure type
    uint8_t  length; ///< Structure length
    uint16_t handle; ///< Structure handle
}__attribute__((packed)) smbios_structure_header_t; ///< SMBIOS structure header

#define SMBIOS_MAX_STRINGS 256

static int32_t smbios_collect_strings(smbios_structure_header_t* header, char_t** strings, uint32_t max_strings) {
    uint32_t string_index = 0;
    uint8_t* string_area  = (uint8_t*)header + header->length;

    while(string_index < max_strings && !(string_area[0] == 0 && string_area[1] == 0)) {
        strings[string_index] = (char_t*)string_area;
        string_area          += strlen((char_t*)string_area) + 1;
        string_index++;
    }

    return string_index;
}

static int8_t smbios_print_type_0(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    if(string_count < 2) {
        PRINTLOG(KERNEL, LOG_ERROR, "not enough strings for BIOS information structure");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("BIOS Information (type %i) handle: 0x%04x\n", header->type, header->handle);

    // 2.0+
    if(major_version >= 2) {
        printf("  Vendor: %s\n", *data?strings[*data - 1]:"Not Specified");
        data++;

        printf("  Version: %s\n", *data?strings[*data - 1]:"Not Specified");
        data++;

        uint16_t starting_address_segment = *((uint16_t*)(void*)data);
        uint32_t runtime_size             = (0x10000 - starting_address_segment) * 16; // Convert segment to bytes
        printf("  Starting Address Segment: 0x%04x\n", starting_address_segment);
        printf("  Runtime Size: %u KB\n", runtime_size / 1024);
        data += 2;

        printf("  Release Date: %s\n", *data?strings[*data - 1]:"Not Specified");
        data++;

        int32_t rom_size = 64 * (*data + 1); // Convert from 64KB units to bytes
        printf("  ROM Size: %u KB\n", rom_size);
        data++;

        uint64_t characteristics = *((uint64_t*)(void*)data);

        printf("BIOS Characteristics:\n");
        if (characteristics & (1ULL << 0)) {
            printf("  - Reserved\n");
        }
        if (characteristics & (1ULL << 1)) {
            printf("  - Reserved\n");
        }
        if (characteristics & (1ULL << 2)) {
            printf("  - Unknown\n");
        }
        if (characteristics & (1ULL << 3)) {
            printf("  - BIOS characteristics not supported\n");
        }
        if (characteristics & (1ULL << 4)) {
            printf("  - ISA is supported\n");
        }
        if (characteristics & (1ULL << 5)) {
            printf("  - MCA is supported\n");
        }
        if (characteristics & (1ULL << 6)) {
            printf("  - EISA is supported\n");
        }
        if (characteristics & (1ULL << 7)) {
            printf("  - PCI is supported\n");
        }
        if (characteristics & (1ULL << 8)) {
            printf("  - PC Card (PCMCIA) is supported\n");
        }
        if (characteristics & (1ULL << 9)) {
            printf("  - Plug and Play BIOS is supported\n");
        }
        if (characteristics & (1ULL << 10)) {
            printf("  - APM is supported\n");
        }
        if (characteristics & (1ULL << 11)) {
            printf("  - BIOS is upgradeable (Flash)\n");
        }
        if (characteristics & (1ULL << 12)) {
            printf("  - BIOS shadowing is allowed\n");
        }
        if (characteristics & (1ULL << 13)) {
            printf("  - VL-VESA is supported\n");
        }
        if (characteristics & (1ULL << 14)) {
            printf("  - ESCD support is available\n");
        }
        if (characteristics & (1ULL << 15)) {
            printf("  - Boot from CD is supported\n");
        }
        if (characteristics & (1ULL << 16)) {
            printf("  - Selectable boot is supported\n");
        }
        if (characteristics & (1ULL << 17)) {
            printf("  - BIOS ROM is socketed\n");
        }
        if (characteristics & (1ULL << 18)) {
            printf("  - Boot from PC Card (PCMCIA) is supported\n");
        }
        if (characteristics & (1ULL << 19)) {
            printf("  - EDD (Enhanced Disk Drive) Specification is supported\n");
        }
        if (characteristics & (1ULL << 20)) {
            printf("  - Int 13h - Japanese Floppy for NEC 9800 Series is supported\n");
        }
        if (characteristics & (1ULL << 21)) {
            printf("  - Int 13h - Japanese Floppy for Toshiba is supported\n");
        }
        if (characteristics & (1ULL << 22)) {
            printf("  - Int 13h - 5.25 in./360 KB Floppy Services are supported\n");
        }
        if (characteristics & (1ULL << 23)) {
            printf("  - Int 13h - 5.25 in./1.2 MB Floppy Services are supported\n");
        }
        if (characteristics & (1ULL << 24)) {
            printf("  - Int 13h - 3.5 in./720 KB Floppy Services are supported\n");
        }
        if (characteristics & (1ULL << 25)) {
            printf("  - Int 13h - 3.5 in./2.88 MB Floppy Services are supported\n");
        }
        if (characteristics & (1ULL << 26)) {
            printf("  - Int 05h - Print Screen Service is supported\n");
        }
        if (characteristics & (1ULL << 27)) {
            printf("  - Int 09h - 8042 Keyboard Services are supported\n");
        }
        if (characteristics & (1ULL << 28)) {
            printf("  - Int 14h - Serial Services are supported\n");
        }
        if (characteristics & (1ULL << 29)) {
            printf("  - Int 17h - Printer Services are supported\n");
        }
        if (characteristics & (1ULL << 30)) {
            printf("  - Int 10h - CGA/Mono Video Services are supported\n");
        }
        if (characteristics & (1ULL << 31)) {
            printf("  - NEC PC-98\n");
        }
        data += 8;
    }

    // 2.4+
    if(major_version > 2 || (major_version == 2 && minor_version >= 4)) {
        uint8_t extension_byte_1 = *data;
        printf("  - BIOS Characteristics Extension Byte 1:\n");
        if (extension_byte_1 & (1 << 0)) {
            printf("    - ACPI is supported\n");
        }
        if (extension_byte_1 & (1 << 1)) {
            printf("    - USB Legacy is supported\n");
        }
        if (extension_byte_1 & (1 << 2)) {
            printf("    - AGP is supported\n");
        }
        if (extension_byte_1 & (1 << 3)) {
            printf("    - I2O boot is supported\n");
        }
        if (extension_byte_1 & (1 << 4)) {
            printf("    - LS-120 boot is supported\n");
        }
        if (extension_byte_1 & (1 << 5)) {
            printf("    - ATAPI ZIP drive boot is supported\n");
        }
        if (extension_byte_1 & (1 << 6)) {
            printf("    - 1394 boot is supported\n");
        }
        if (extension_byte_1 & (1 << 7)) {
            printf("    - Smart battery is supported\n");
        }
        data++;

        uint8_t extension_byte_2 = *data;
        printf("  - BIOS Characteristics Extension Byte 2:\n");
        if (extension_byte_2 & (1 << 0)) {
            printf("    - BIOS Boot Specification is supported\n");
        }
        if (extension_byte_2 & (1 << 1)) {
            printf("    - Function key-initiated network boot is supported\n");
        }
        if (extension_byte_2 & (1 << 2)) {
            printf("    - Targeted content distribution is supported\n");
        }
        if (extension_byte_2 & (1 << 3)) {
            printf("    - UEFI Specification is supported\n");
        }
        if (extension_byte_2 & (1 << 4)) {
            printf("    - System is a virtual machine\n");
        }
        if (extension_byte_2 & (1 << 5)) {
            printf("    - Manifacturing mode is supported\n");
        }
        if (extension_byte_2 & (1 << 6)) {
            printf("    - Manifacturing mode is enabled\n");
        }
        if (extension_byte_2 & (1 << 7)) {
            printf("    - Reserved for future assignment\n");
        }
        data++;

        printf("  System BIOS Major Release: %u\n", *data);
        data++;

        printf("  System BIOS Minor Release: %u\n", *data);
        data++;

        printf("  Embedded Controller Firmware Major Release: 0x%02x\n", *data);
        data++;

        printf("  Embedded Controller Firmware Minor Release: 0x%02x\n", *data);
        data++;
    }

    // 3.1+
    if(major_version == 3 && minor_version >= 1) {
        uint16_t extended_bios_rom_size       = *((uint16_t*)(void*)data);
        uint8_t extended_bios_rom_size_unit   = extended_bios_rom_size >> 14; // Get the unit (bits 15-14)
        uint32_t extended_bios_rom_size_value = extended_bios_rom_size & 0x3FFF; // Get the size value (bits 13-0)
        if(extended_bios_rom_size_unit == 1) {
            extended_bios_rom_size_value *= 1024; // convert gb to mb
        }

        printf("  Extended BIOS ROM Size: %u MB\n", extended_bios_rom_size_value);
        data += 2;
    }

    return 0;

}

static int8_t smbios_print_type_1(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    if(string_count < 4) {
        PRINTLOG(KERNEL, LOG_ERROR, "not enough strings for System Information structure");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("System Information (type %i) handle: 0x%04x\n", header->type, header->handle);

    printf("  Manufacturer: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    printf("  Product Name: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    printf("  Version: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    printf("  Serial Number: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    // 2.1+
    if (major_version > 2 || (major_version == 2 && minor_version >= 1)) {
        uint8_t* uuid_data = data;
        printf("  UUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
               uuid_data[3], uuid_data[2], uuid_data[1], uuid_data[0],
               uuid_data[5], uuid_data[4],
               uuid_data[7], uuid_data[6],
               uuid_data[8], uuid_data[9],
               uuid_data[10], uuid_data[11], uuid_data[12], uuid_data[13], uuid_data[14], uuid_data[15]);
        data += 16;

        uint8_t wakeup_type = *data;
        printf("  Wake-up Type: ");
        switch (wakeup_type) {
        case 0: printf("Reserved\n"); break;
        case 1: printf("Other\n"); break;
        case 2: printf("Unknown\n"); break;
        case 3: printf("APM Timer\n"); break;
        case 4: printf("Modem Ring\n"); break;
        case 5: printf("LAN Remote\n"); break;
        case 6: printf("Power Switch\n"); break;
        case 7: printf("PCI PME#\n"); break;
        case 8: printf("AC Power Restored\n"); break;
        default: printf("Undefined (0x%02x)\n", wakeup_type); break;
        }
        data++;
    }

    // 2.4+
    if (major_version > 2 || (major_version == 2 && minor_version >= 4)) {
        printf("  SKU Number: %s\n", *data ? strings[*data - 1] : "Not Specified");
        data++;

        printf("  Family: %s\n", *data ? strings[*data - 1] : "Not Specified");
        data++;
    }

    return 0;
}

static int8_t smbios_print_type_2(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    if(string_count < 4) {
        PRINTLOG(KERNEL, LOG_ERROR, "not enough strings for Baseboard Information structure");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("Baseboard Information (type %i) handle: 0x%04x\n", header->type, header->handle);

    printf("  Manufacturer: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    printf("  Product Name: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    printf("  Version: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    printf("  Serial Number: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    printf("  Asset Tag: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint8_t feature_flags = *data;
    printf("  Feature Flags:\n");
    if (feature_flags & (1 << 0)) {
        printf("    - Board is a hosting board\n");
    }
    if (feature_flags & (1 << 1)) {
        printf("    - Board is required to function with other boards\n");
    }
    if (feature_flags & (1 << 2)) {
        printf("    - Board is removable\n");
    }
    if (feature_flags & (1 << 3)) {
        printf("    - Board is replaceable\n");
    }
    if (feature_flags & (1 << 4)) {
        printf("    - Board is hot swappable\n");
    }
    data++;

    uint8_t location_in_chassis = *data;
    printf("  Location In Chassis: %s\n", location_in_chassis ? strings[location_in_chassis - 1] : "Not Specified");
    data++;

    uint16_t chassis_handle = *((uint16_t*)(void*)data);
    printf("  Chassis Handle: 0x%04x\n", chassis_handle);
    data += 2;

    uint8_t board_type = *data;
    printf("  Board Type: ");
    switch (board_type) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("Server Blade\n"); break;
    case 0x04: printf("Connectivity Switch\n"); break;
    case 0x05: printf("System Management Module\n"); break;
    case 0x06: printf("Processor Module\n"); break;
    case 0x07: printf("I/O Module\n"); break;
    case 0x08: printf("Memory Module\n"); break;
    case 0x09: printf("Daughter Board\n"); break;
    case 0x0A: printf("Motherboard\n"); break;
    case 0x0B: printf("Processor/Memory Module\n"); break;
    case 0x0C: printf("Processor/IO Module\n"); break;
    case 0x0D: printf("Interconnect Board\n"); break;
    default: printf("Undefined (0x%02x)\n", board_type); break;
    }
    data++;

    uint8_t number_of_contained_object_handles = *data;
    printf("  Number Of Contained Object Handles: %u\n", number_of_contained_object_handles);
    data++;

    // Contained Object Handles (if any)
    for (uint8_t i = 0; i < number_of_contained_object_handles; i++) {
        uint16_t contained_object_handle = *((uint16_t*)(void*)data);
        printf("    Contained Object Handle %u: 0x%04x\n", i + 1, contained_object_handle);
        data += 2;
    }

    return 0;
}

static int8_t smbios_print_type_3(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    if(string_count < 1) {
        PRINTLOG(KERNEL, LOG_ERROR, "not enough strings for System Enclosure or Chassis structure");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("System Enclosure or Chassis (type %i) handle: 0x%04x\n", header->type, header->handle);

    printf("  Manufacturer: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint8_t type = *data;
    printf("  Type: ");
    switch (type & 0x7F) { // Mask out the Lock bit (bit 7)
    case 1: printf("Other\n"); break;
    case 2: printf("Unknown\n"); break;
    case 3: printf("Desktop\n"); break;
    case 4: printf("Low Profile Desktop\n"); break;
    case 5: printf("Pizza Box\n"); break;
    case 6: printf("Mini Tower\n"); break;
    case 7: printf("Tower\n"); break;
    case 8: printf("Portable\n"); break;
    case 9: printf("Laptop\n"); break;
    case 10: printf("Notebook\n"); break;
    case 11: printf("Hand Held\n"); break;
    case 12: printf("Docking Station\n"); break;
    case 13: printf("All in One\n"); break;
    case 14: printf("Sub Notebook\n"); break;
    case 15: printf("Space-saving\n"); break;
    case 16: printf("Lunch Box\n"); break;
    case 17: printf("Main Server Chassis\n"); break;
    case 18: printf("Expansion Chassis\n"); break;
    case 19: printf("SubChassis\n"); break;
    case 20: printf("Bus Expansion Chassis\n"); break;
    case 21: printf("Peripheral Chassis\n"); break;
    case 22: printf("RAID Chassis\n"); break;
    case 23: printf("Rack Mount Chassis\n"); break;
    case 24: printf("Sealed-case PC\n"); break;
    case 25: printf("Multi-system Chassis\n"); break;
    case 26: printf("CompactPCI\n"); break;
    case 27: printf("AdvancedTCA\n"); break;
    case 28: printf("Blade\n"); break;
    case 29: printf("Blade Enclosure\n"); break;
    case 30: printf("Tablet\n"); break;
    case 31: printf("Convertible\n"); break;
    case 32: printf("Detachable\n"); break;
    case 33: printf("IoT Gateway\n"); break;
    case 34: printf("Embedded PC\n"); break;
    case 35: printf("Mini PC\n"); break;
    case 36: printf("Stick PC\n"); break;
    default: printf("Undefined (0x%02x)\n", type & 0x7F); break;
    }
    if (type & 0x80) {
        printf("  Chassis Lock: Present\n");
    } else {
        printf("  Chassis Lock: Not Present\n");
    }
    data++;

    printf("  Version: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    printf("  Serial Number: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    printf("  Asset Tag Number: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint8_t boot_up_state = *data;
    printf("  Boot-up State: ");
    switch (boot_up_state) {
    case 1: printf("Other\n"); break;
    case 2: printf("Unknown\n"); break;
    case 3: printf("Safe\n"); break;
    case 4: printf("Warning\n"); break;
    case 5: printf("Critical\n"); break;
    case 6: printf("Non-recoverable\n"); break;
    default: printf("Undefined (0x%02x)\n", boot_up_state); break;
    }
    data++;

    uint8_t power_supply_state = *data;
    printf("  Power Supply State: ");
    switch (power_supply_state) {
    case 1: printf("Other\n"); break;
    case 2: printf("Unknown\n"); break;
    case 3: printf("Safe\n"); break;
    case 4: printf("Warning\n"); break;
    case 5: printf("Critical\n"); break;
    case 6: printf("Non-recoverable\n"); break;
    default: printf("Undefined (0x%02x)\n", power_supply_state); break;
    }
    data++;

    uint8_t thermal_state = *data;
    printf("  Thermal State: ");
    switch (thermal_state) {
    case 1: printf("Other\n"); break;
    case 2: printf("Unknown\n"); break;
    case 3: printf("Safe\n"); break;
    case 4: printf("Warning\n"); break;
    case 5: printf("Critical\n"); break;
    case 6: printf("Non-recoverable\n"); break;
    default: printf("Undefined (0x%02x)\n", thermal_state); break;
    }
    data++;

    uint8_t security_status = *data;
    printf("  Security Status: ");
    switch (security_status) {
    case 1: printf("Other\n"); break;
    case 2: printf("Unknown\n"); break;
    case 3: printf("None\n"); break;
    case 4: printf("External Interface Locked Out\n"); break;
    case 5: printf("External Interface Enabled\n"); break;
    default: printf("Undefined (0x%02x)\n", security_status); break;
    }
    data++;

    // 2.3+
    if (major_version > 2 || (major_version == 2 && minor_version >= 3)) {
        uint32_t oem_defined = *((uint32_t*)(void*)data);
        printf("  OEM Defined: 0x%08x\n", oem_defined);
        data += 4;

        uint8_t height = *data;
        printf("  Height: %u U\n", height);
        data++;

        uint8_t number_of_power_cords = *data;
        printf("  Number Of Power Cords: %u\n", number_of_power_cords);
        data++;

        uint8_t contained_element_count = *data;
        printf("  Contained Element Count: %u\n", contained_element_count);
        data++;

        uint8_t contained_element_record_length = *data;
        printf("  Contained Element Record Length: %u\n", contained_element_record_length);
        data++;

        // Contained Elements (if any)
        for (uint8_t i = 0; i < contained_element_count; i++) {
            printf("    Contained Element %u:\n", i + 1);
            // Parse contained element data based on contained_element_record_length
            // This part is complex and depends on the specific format of contained elements.
            // For now, we'll just advance the pointer.
            data += contained_element_record_length;
        }
    }

    // 2.7+
    if (major_version > 2 || (major_version == 2 && minor_version >= 7)) {
        uint8_t sku_number_string_number = *data;
        printf("  SKU Number: %s\n", sku_number_string_number ? strings[sku_number_string_number - 1] : "Not Specified");
        data++;
    }

    return 0;
}

static void smbios_print_type_4_processor_family(uint16_t processor_family) {
    switch (processor_family) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("8086\n"); break;
    case 0x04: printf("80286\n"); break;
    case 0x05: printf("Intel386™ processor\n"); break;
    case 0x06: printf("Intel486™ processor\n"); break;
    case 0x07: printf("8087\n"); break;
    case 0x08: printf("80287\n"); break;
    case 0x09: printf("80387\n"); break;
    case 0x0A: printf("80487\n"); break;
    case 0x0B: printf("Intel® Pentium® processor\n"); break;
    case 0x0C: printf("Pentium® Pro processor\n"); break;
    case 0x0D: printf("Pentium® II processor\n"); break;
    case 0x0E: printf("Pentium® processor with MMX™ technology\n"); break;
    case 0x0F: printf("Intel® Celeron® processor\n"); break;
    case 0x10: printf("Pentium® II Xeon™ processor\n"); break;
    case 0x11: printf("Pentium® III processor\n"); break;
    case 0x12: printf("M1 Family\n"); break;
    case 0x13: printf("M2 Family\n"); break;
    case 0x14: printf("Intel® Celeron® M processor\n"); break;
    case 0x15: printf("Intel® Pentium® 4 HT processor\n"); break;
    case 0x16: printf("Intel® Processor\n"); break;
    case 0x18: printf("AMD Duron™ Processor Family\n"); break;
    case 0x19: printf("K5 Family [1]\n"); break;
    case 0x1A: printf("K6 Family [1]\n"); break;
    case 0x1B: printf("K6-2 [1]\n"); break;
    case 0x1C: printf("K6-3 [1]\n"); break;
    case 0x1D: printf("AMD Athlon™ Processor Family\n"); break;
    case 0x1E: printf("AMD29000 Family\n"); break;
    case 0x1F: printf("K6-2+\n"); break;
    case 0x20: printf("Power PC Family\n"); break;
    case 0x21: printf("Power PC 601\n"); break;
    case 0x22: printf("Power PC 603\n"); break;
    case 0x23: printf("Power PC 603+\n"); break;
    case 0x24: printf("Power PC 604\n"); break;
    case 0x25: printf("Power PC 620\n"); break;
    case 0x26: printf("Power PC x704\n"); break;
    case 0x27: printf("Power PC 750\n"); break;
    case 0x28: printf("Intel® Core™ Duo processor\n"); break;
    case 0x29: printf("Intel® Core™ Duo mobile processor\n"); break;
    case 0x2A: printf("Intel® Core™ Solo mobile processor\n"); break;
    case 0x2B: printf("Intel® Atom™ processor\n"); break;
    case 0x2C: printf("Intel® Core™ M processor\n"); break;
    case 0x2D: printf("Intel(R) Core(TM) m3 processor\n"); break;
    case 0x2E: printf("Intel(R) Core(TM) m5 processor\n"); break;
    case 0x2F: printf("Intel(R) Core(TM) m7 processor\n"); break;
    case 0x30: printf("Alpha Family [2]\n"); break;
    case 0x31: printf("Alpha 21064\n"); break;
    case 0x32: printf("Alpha 21066\n"); break;
    case 0x33: printf("Alpha 21164\n"); break;
    case 0x34: printf("Alpha 21164PC\n"); break;
    case 0x35: printf("Alpha 21164a\n"); break;
    case 0x36: printf("Alpha 21264\n"); break;
    case 0x37: printf("Alpha 21364\n"); break;
    case 0x38: printf("AMD Turion™ II Ultra Dual-Core Mobile M Processor Family\n"); break;
    case 0x39: printf("AMD Turion™ II Dual-Core Mobile M Processor Family\n"); break;
    case 0x3A: printf("AMD Athlon™ II Dual-Core M Processor Family\n"); break;
    case 0x3B: printf("AMD Opteron™ 6100 Series Processor\n"); break;
    case 0x3C: printf("AMD Opteron™ 4100 Series Processor\n"); break;
    case 0x3D: printf("AMD Opteron™ 6200 Series Processor\n"); break;
    case 0x3E: printf("AMD Opteron™ 4200 Series Processor\n"); break;
    case 0x3F: printf("AMD FX™ Series Processor\n"); break;
    case 0x40: printf("MIPS Family\n"); break;
    case 0x41: printf("MIPS R4000\n"); break;
    case 0x42: printf("MIPS R4200\n"); break;
    case 0x43: printf("MIPS R4400\n"); break;
    case 0x44: printf("MIPS R4600\n"); break;
    case 0x45: printf("MIPS R10000\n"); break;
    case 0x46: printf("AMD C-Series Processor\n"); break;
    case 0x47: printf("AMD E-Series Processor\n"); break;
    case 0x48: printf("AMD A-Series Processor\n"); break;
    case 0x49: printf("AMD G-Series Processor\n"); break;
    case 0x4A: printf("AMD Z-Series Processor\n"); break;
    case 0x4B: printf("AMD R-Series Processor\n"); break;
    case 0x4C: printf("AMD Opteron™ 4300 Series Processor\n"); break;
    case 0x4D: printf("AMD Opteron™ 6300 Series Processor\n"); break;
    case 0x4E: printf("AMD Opteron™ 3300 Series Processor\n"); break;
    case 0x4F: printf("AMD FirePro™ Series Processor\n"); break;
    case 0x50: printf("SPARC Family\n"); break;
    case 0x51: printf("SuperSPARC\n"); break;
    case 0x52: printf("microSPARC II\n"); break;
    case 0x53: printf("microSPARC IIep\n"); break;
    case 0x54: printf("UltraSPARC\n"); break;
    case 0x55: printf("UltraSPARC II\n"); break;
    case 0x56: printf("UltraSPARC Iii\n"); break;
    case 0x57: printf("UltraSPARC III\n"); break;
    case 0x58: printf("UltraSPARC IIIi\n"); break;
    case 0x60: printf("68040 Family\n"); break;
    case 0x61: printf("68xxx\n"); break;
    case 0x62: printf("68000\n"); break;
    case 0x63: printf("68010\n"); break;
    case 0x64: printf("68020\n"); break;
    case 0x65: printf("68030\n"); break;
    case 0x66: printf("AMD Athlon(TM) X4 Quad-Core Processor Family\n"); break;
    case 0x67: printf("AMD Opteron(TM) X1000 Series Processor\n"); break;
    case 0x68: printf("AMD Opteron(TM) X2000 Series APU\n"); break;
    case 0x69: printf("AMD Opteron(TM) A-Series Processor\n"); break;
    case 0x6A: printf("AMD Opteron(TM) X3000 Series APU\n"); break;
    case 0x6B: printf("AMD Zen Processor Family\n"); break;
    case 0x70: printf("Hobbit Family\n"); break;
    case 0x78: printf("Crusoe™ TM5000 Family\n"); break;
    case 0x79: printf("Crusoe™ TM3000 Family\n"); break;
    case 0x7A: printf("Efficeon™ TM8000 Family\n"); break;
    case 0x80: printf("Weitek\n"); break;
    case 0x82: printf("Itanium™ processor\n"); break;
    case 0x83: printf("AMD Athlon™ 64 Processor Family\n"); break;
    case 0x84: printf("AMD Opteron™ Processor Family\n"); break;
    case 0x85: printf("AMD Sempron™ Processor Family\n"); break;
    case 0x86: printf("AMD Turion™ 64 Mobile Technology\n"); break;
    case 0x87: printf("Dual-Core AMD Opteron™ Processor Family\n"); break;
    case 0x88: printf("AMD Athlon™ 64 X2 Dual-Core Processor Family\n"); break;
    case 0x89: printf("AMD Turion™ 64 X2 Mobile Technology\n"); break;
    case 0x8A: printf("Quad-Core AMD Opteron™ Processor Family\n"); break;
    case 0x8B: printf("Third-Generation AMD Opteron™ Processor Family\n"); break;
    case 0x8C: printf("AMD Phenom™ FX Quad-Core Processor Family\n"); break;
    case 0x8D: printf("AMD Phenom™ X4 Quad-Core Processor Family\n"); break;
    case 0x8E: printf("AMD Phenom™ X2 Dual-Core Processor Family\n"); break;
    case 0x8F: printf("AMD Athlon™ X2 Dual-Core Processor Family\n"); break;
    case 0x90: printf("PA-RISC Family\n"); break;
    case 0x91: printf("PA-RISC 8500\n"); break;
    case 0x92: printf("PA-RISC 8000\n"); break;
    case 0x93: printf("PA-RISC 7300LC\n"); break;
    case 0x94: printf("PA-RISC 7200\n"); break;
    case 0x95: printf("PA-RISC 7100LC\n"); break;
    case 0x96: printf("PA-RISC 7100\n"); break;
    case 0xA0: printf("V30 Family\n"); break;
    case 0xA1: printf("Quad-Core Intel® Xeon® processor 3200 Series\n"); break;
    case 0xA2: printf("Dual-Core Intel® Xeon® processor 3000 Series\n"); break;
    case 0xA3: printf("Quad-Core Intel® Xeon® processor 5300 Series\n"); break;
    case 0xA4: printf("Dual-Core Intel® Xeon® processor 5100 Series\n"); break;
    case 0xA5: printf("Dual-Core Intel® Xeon® processor 5000 Series\n"); break;
    case 0xA6: printf("Dual-Core Intel® Xeon® processor LV\n"); break;
    case 0xA7: printf("Dual-Core Intel® Xeon® processor ULV\n"); break;
    case 0xA8: printf("Dual-Core Intel® Xeon® processor 7100 Series\n"); break;
    case 0xA9: printf("Quad-Core Intel® Xeon® processor 5400 Series\n"); break;
    case 0xAA: printf("Quad-Core Intel® Xeon® processor\n"); break;
    case 0xAB: printf("Dual-Core Intel® Xeon® processor 5200 Series\n"); break;
    case 0xAC: printf("Dual-Core Intel® Xeon® processor 7200 Series\n"); break;
    case 0xAD: printf("Quad-Core Intel® Xeon® processor 7300 Series\n"); break;
    case 0xAE: printf("Quad-Core Intel® Xeon® processor 7400 Series\n"); break;
    case 0xAF: printf("Multi-Core Intel® Xeon® processor 7400 Series\n"); break;
    case 0xB0: printf("Pentium® III Xeon™ processor\n"); break;
    case 0xB1: printf("Pentium® III Processor with Intel® SpeedStep™ Technology\n"); break;
    case 0xB2: printf("Pentium® 4 Processor\n"); break;
    case 0xB3: printf("Intel® Xeon® processor\n"); break;
    case 0xB4: printf("AS400 Family\n"); break;
    case 0xB5: printf("Intel® Xeon™ processor MP\n"); break;
    case 0xB6: printf("AMD Athlon™ XP Processor Family\n"); break;
    case 0xB7: printf("AMD Athlon™ MP Processor Family\n"); break;
    case 0xB8: printf("Intel® Itanium® 2 processor\n"); break;
    case 0xB9: printf("Intel® Pentium® M processor\n"); break;
    case 0xBA: printf("Intel® Celeron® D processor\n"); break;
    case 0xBB: printf("Intel® Pentium® D processor\n"); break;
    case 0xBC: printf("Intel® Pentium® Processor Extreme Edition\n"); break;
    case 0xBD: printf("Intel® Core™ Solo Processor\n"); break;
    case 0xBF: printf("Intel® Core™ 2 Duo Processor\n"); break;
    case 0xC0: printf("Intel® Core™ 2 Solo processor\n"); break;
    case 0xC1: printf("Intel® Core™ 2 Extreme processor\n"); break;
    case 0xC2: printf("Intel® Core™ 2 Quad processor\n"); break;
    case 0xC3: printf("Intel® Core™ 2 Extreme mobile processor\n"); break;
    case 0xC4: printf("Intel® Core™ 2 Duo mobile processor\n"); break;
    case 0xC5: printf("Intel® Core™ 2 Solo mobile processor\n"); break;
    case 0xC6: printf("Intel® Core™ i7 processor\n"); break;
    case 0xC7: printf("Dual-Core Intel® Celeron® processor\n"); break;
    case 0xC8: printf("IBM390 Family\n"); break;
    case 0xC9: printf("G4\n"); break;
    case 0xCA: printf("G5\n"); break;
    case 0xCB: printf("ESA/390 G6\n"); break;
    case 0xCC: printf("z/Architecture base\n"); break;
    case 0xCD: printf("Intel® Core™ i5 processor\n"); break;
    case 0xCE: printf("Intel® Core™ i3 processor\n"); break;
    case 0xCF: printf("Intel® Core™ i9 processor\n"); break;
    case 0xD2: printf("VIA C7™-M Processor Family\n"); break;
    case 0xD3: printf("VIA C7™-D Processor Family\n"); break;
    case 0xD4: printf("VIA C7™ Processor Family\n"); break;
    case 0xD5: printf("VIA Eden™ Processor Family\n"); break;
    case 0xD6: printf("Multi-Core Intel® Xeon® processor\n"); break;
    case 0xD7: printf("Dual-Core Intel® Xeon® processor 3xxx Series\n"); break;
    case 0xD8: printf("Quad-Core Intel® Xeon® processor 3xxx Series\n"); break;
    case 0xD9: printf("VIA Nano™ Processor Family\n"); break;
    case 0xDA: printf("Dual-Core Intel® Xeon® processor 5xxx Series\n"); break;
    case 0xDB: printf("Quad-Core Intel® Xeon® processor 5xxx Series\n"); break;
    case 0xDD: printf("Dual-Core Intel® Xeon® processor 7xxx Series\n"); break;
    case 0xDE: printf("Quad-Core Intel® Xeon® processor 7xxx Series\n"); break;
    case 0xDF: printf("Multi-Core Intel® Xeon® processor 7xxx Series\n"); break;
    case 0xE0: printf("Multi-Core Intel® Xeon® processor 3400 Series\n"); break;
    case 0xE4: printf("AMD Opteron™ 3000 Series Processor\n"); break;
    case 0xE5: printf("AMD Sempron™ II Processor\n"); break;
    case 0xE6: printf("Embedded AMD Opteron™ Quad-Core Processor Family\n"); break;
    case 0xE7: printf("AMD Phenom™ Triple-Core Processor Family\n"); break;
    case 0xE8: printf("AMD Turion™ Ultra Dual-Core Mobile Processor Family\n"); break;
    case 0xE9: printf("AMD Turion™ Dual-Core Mobile Processor Family\n"); break;
    case 0xEA: printf("AMD Athlon™ Dual-Core Processor Family\n"); break;
    case 0xEB: printf("AMD Sempron™ SI Processor Family\n"); break;
    case 0xEC: printf("AMD Phenom™ II Processor Family\n"); break;
    case 0xED: printf("AMD Athlon™ II Processor Family\n"); break;
    case 0xEE: printf("Six-Core AMD Opteron™ Processor Family\n"); break;
    case 0xEF: printf("AMD Sempron™ M Processor Family\n"); break;
    case 0xFA: printf("i860\n"); break;
    case 0xFB: printf("i960\n"); break;
    case 0x100: printf("ARMv7\n"); break;
    case 0x101: printf("ARMv8\n"); break;
    case 0x102: printf("ARMv9\n"); break;
    case 0x103: printf("Reserved for future use by ARM\n"); break;
    case 0x104: printf("SH-3\n"); break;
    case 0x105: printf("SH-4\n"); break;
    case 0x118: printf("ARM\n"); break;
    case 0x119: printf("StrongARM\n"); break;
    case 0x12C: printf("6x86\n"); break;
    case 0x12D: printf("MediaGX\n"); break;
    case 0x12E: printf("MII\n"); break;
    case 0x140: printf("WinChip\n"); break;
    case 0x15E: printf("DSP\n"); break;
    case 0x1F4: printf("Video Processor\n"); break;
    case 0x200: printf("RISC-V RV32\n"); break;
    case 0x201: printf("RISC-V RV64\n"); break;
    case 0x202: printf("RISC-V RV128\n"); break;
    case 0x258: printf("LoongArch\n"); break;
    case 0x259: printf("Loongson™ 1 Processor Family\n"); break;
    case 0x25A: printf("Loongson™ 2 Processor Family\n"); break;
    case 0x25B: printf("Loongson™ 3 Processor Family\n"); break;
    case 0x25C: printf("Loongson™ 2K Processor Family\n"); break;
    case 0x25D: printf("Loongson™ 3A Processor Family\n"); break;
    case 0x25E: printf("Loongson™ 3B Processor Family\n"); break;
    case 0x25F: printf("Loongson™ 3C Processor Family\n"); break;
    case 0x260: printf("Loongson™ 3D Processor Family\n"); break;
    case 0x261: printf("Loongson™ 3E Processor Family\n"); break;
    case 0x262: printf("Dual-Core Loongson™ 2K Processor 2xxx Series\n"); break;
    case 0x26C: printf("Quad-Core Loongson™ 3A Processor 5xxx Series\n"); break;
    case 0x26D: printf("Multi-Core Loongson™ 3A Processor 5xxx Series\n"); break;
    case 0x26E: printf("Quad-Core Loongson™ 3B Processor 5xxx Series\n"); break;
    case 0x26F: printf("Multi-Core Loongson™ 3B Processor 5xxx Series\n"); break;
    case 0x270: printf("Multi-Core Loongson™ 3C Processor 5xxx Series\n"); break;
    case 0x271: printf("Multi-Core Loongson™ 3D Processor 5xxx Series\n"); break;
    default: printf("Undefined (0x%04x)\n", processor_family); break;
    }
}

static int8_t smbios_print_type_4(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    if(string_count < 3) {
        PRINTLOG(KERNEL, LOG_ERROR, "not enough strings for Processor Information structure");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("Processor Information (type %i) handle: 0x%04x\n", header->type, header->handle);

    printf("  Socket Designation: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint8_t processor_type = *data;
    printf("  Processor Type: ");
    switch (processor_type) {
    case 1: printf("Other\n"); break;
    case 2: printf("Unknown\n"); break;
    case 3: printf("Central Processor\n"); break;
    case 4: printf("Math Processor\n"); break;
    case 5: printf("DSP Processor\n"); break;
    case 6: printf("Video Processor\n"); break;
    default: printf("Undefined (0x%02x)\n", processor_type); break;
    }
    data++;

    uint8_t processor_family = *data;
    if(processor_family != 0xFE) {
        printf("  Processor Family: ");
        smbios_print_type_4_processor_family(processor_family);
    }
    data++;

    printf("  Processor Manufacturer: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint64_t processor_id = *((uint64_t*)(void*)data);
    printf("  Processor ID: 0x%016llx\n", processor_id);
    data += 8;

    uint8_t processor_version_string_number = *data;
    printf("  Processor Version: %s\n", processor_version_string_number ? strings[processor_version_string_number - 1] : "Not Specified");
    data++;

    uint8_t voltage = *data;
    printf("  Voltage: ");
    if (voltage & 0x80) { // Bit 7 - Voltage Mode
        printf("%.1f V\n", (voltage & 0x7F) / 10.0);
    } else {
        boolean_t voltage_valid = voltage & 0x7F; // Bits 0-6 - Voltage Value
        if(voltage & 0x01) {
            printf("5 V ");
        }
        if(voltage & 0x02) {
            printf("3.3 V ");
        }
        if(voltage & 0x04) {
            printf("2.9 V ");
        }
        if(!voltage_valid) {
            printf("Unknown Voltage");
        }
        printf("\n");
    }
    data++;

    uint16_t external_clock = *((uint16_t*)(void*)data);
    printf("  External Clock: %u MHz\n", external_clock);
    data += 2;

    uint16_t max_speed = *((uint16_t*)(void*)data);
    printf("  Max Speed: %u MHz\n", max_speed);
    data += 2;

    uint16_t current_speed = *((uint16_t*)(void*)data);
    printf("  Current Speed: %u MHz\n", current_speed);
    data += 2;

    uint8_t status = *data;
    printf("  Status: ");
    if (status & (1 << 6)) { // Bit 6 - CPU Socket Populated
        printf("Populated, ");
    } else {
        printf("Socket Empty, ");
    }
    switch ((status) & 0x07) { // Bits 0-2 - CPU Status
    case 0: printf("Unknown\n"); break;
    case 1: printf("Enabled\n"); break;
    case 2: printf("Disabled by User\n"); break;
    case 3: printf("Disabled by BIOS\n"); break;
    case 4: printf("Idle\n"); break;
    case 7: printf("Other\n"); break;
    default: printf("Undefined (0x%02x)\n", (status >> 3) & 0x07); break;
    }
    data++;

    uint8_t upgrade = *data;
    printf("  Upgrade: ");
    switch (upgrade) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("Daughter Board\n"); break;
    case 0x04: printf("ZIF Socket\n"); break;
    case 0x05: printf("Replaceable Piggy Back\n"); break;
    case 0x06: printf("None\n"); break;
    case 0x07: printf("LIF Socket\n"); break;
    case 0x08: printf("Slot 1\n"); break;
    case 0x09: printf("Slot 2\n"); break;
    case 0x0A: printf("370-pin Socket\n"); break;
    case 0x0B: printf("Slot A\n"); break;
    case 0x0C: printf("Slot M\n"); break;
    case 0x0D: printf("Socket 423\n"); break;
    case 0x0E: printf("Socket A (462)\n"); break;
    case 0x0F: printf("Socket 478\n"); break;
    case 0x10: printf("Socket 754\n"); break;
    case 0x11: printf("Socket 940\n"); break;
    case 0x12: printf("Socket 939\n"); break;
    case 0x13: printf("Socket mPGA604\n"); break;
    case 0x14: printf("Socket LGA771\n"); break;
    case 0x15: printf("Socket LGA775\n"); break;
    case 0x16: printf("Socket S1\n"); break;
    case 0x17: printf("Socket AM2\n"); break;
    case 0x18: printf("Socket F (1207)\n"); break;
    case 0x19: printf("Socket LGA1366\n"); break;
    case 0x1A: printf("Socket G34\n"); break;
    case 0x1B: printf("Socket AM3\n"); break;
    case 0x1C: printf("Socket C32\n"); break;
    case 0x1D: printf("Socket LGA1156\n"); break;
    case 0x1E: printf("Socket LGA1567\n"); break;
    case 0x1F: printf("Socket PGA988A\n"); break;
    case 0x20: printf("Socket BGA1288\n"); break;
    case 0x21: printf("Socket rPGA988B\n"); break;
    case 0x22: printf("Socket BGA1023\n"); break;
    case 0x23: printf("Socket BGA1224\n"); break;
    case 0x24: printf("Socket LGA1155\n"); break;
    case 0x25: printf("Socket LGA1356\n"); break;
    case 0x26: printf("Socket LGA2011\n"); break;
    case 0x27: printf("Socket FS1\n"); break;
    case 0x28: printf("Socket FS2\n"); break;
    case 0x29: printf("Socket FM1\n"); break;
    case 0x2A: printf("Socket FM2\n"); break;
    case 0x2B: printf("Socket LGA2011-3\n"); break;
    case 0x2C: printf("Socket LGA1356-3\n"); break;
    case 0x2D: printf("Socket LGA1150\n"); break;
    case 0x2E: printf("Socket BGA1168\n"); break;
    case 0x2F: printf("Socket BGA1234\n"); break;
    case 0x30: printf("Socket BGA1364\n"); break;
    case 0x31: printf("Socket AM4\n"); break;
    case 0x32: printf("Socket LGA1151\n"); break;
    case 0x33: printf("Socket BGA1356\n"); break;
    case 0x34: printf("Socket BGA1440\n"); break;
    case 0x35: printf("Socket BGA1515\n"); break;
    case  0x36: printf("Socket LGA3647-1\n"); break;
    case 0x37: printf("Socket SP3\n"); break;
    case 0x38: printf("Socket SP3r2\n"); break;
    case 0x39: printf("Socket LGA2066\n"); break;
    case 0x3A: printf("Socket BGA1392\n"); break;
    case 0x3B: printf("Socket BGA1510\n"); break;
    case 0x3C: printf("Socket BGA1528\n"); break;
    case 0x3D: printf("Socket LGA4189\n"); break;
    case 0x3E: printf("Socket LGA1200\n"); break;
    case 0x3F: printf("Socket LGA4677\n"); break;
    case 0x40: printf("Socket LGA1700\n"); break;
    case 0x41: printf("Socket BGA1744\n"); break;
    case 0x42: printf("Socket BGA1781\n"); break;
    case 0x43: printf("Socket BGA1211\n"); break;
    case 0x44: printf("Socket BGA2422\n"); break;
    case 0x45: printf("Socket LGA1211\n"); break;
    case 0x46: printf("Socket LGA2422\n"); break;
    case 0x47: printf("Socket LGA5773\n"); break;
    case 0x48: printf("Socket BGA5773\n"); break;
    case 0x49: printf("Socket AM5\n"); break;
    case 0x4A: printf("Socket SP5\n"); break;
    case 0x4B: printf("Socket SP6\n"); break;
    case 0x4C: printf("Socket BGA883\n"); break;
    case 0x4D: printf("Socket BGA1190\n"); break;
    case 0x4E: printf("Socket BGA4129\n"); break;
    case 0x4F: printf("Socket LGA4710\n"); break;
    case 0x50: printf("Socket LGA7529\n"); break;
    default: printf("Undefined (0x%02x)\n", upgrade); break;
    }
    data++;

    // 2.1+
    if (major_version > 2 || (major_version == 2 && minor_version >= 1)) {
        uint16_t l1_cache_handle = *((uint16_t*)(void*)data);
        printf("  L1 Cache Handle: 0x%04x\n", l1_cache_handle == 0xFFFF ? 0 : l1_cache_handle);
        data += 2;

        uint16_t l2_cache_handle = *((uint16_t*)(void*)data);
        printf("  L2 Cache Handle: 0x%04x\n", l2_cache_handle == 0xFFFF ? 0 : l2_cache_handle);
        data += 2;

        uint16_t l3_cache_handle = *((uint16_t*)(void*)data);
        printf("  L3 Cache Handle: 0x%04x\n", l3_cache_handle == 0xFFFF ? 0 : l3_cache_handle);
        data += 2;
    }

    // 2.3+
    if (major_version > 2 || (major_version == 2 && minor_version >= 3)) {
        uint8_t serial_number_string_number = *data;
        printf("  Serial Number: %s\n", serial_number_string_number ? strings[serial_number_string_number - 1] : "Not Specified");
        data++;

        uint8_t asset_tag_string_number = *data;
        printf("  Asset Tag: %s\n", asset_tag_string_number ? strings[asset_tag_string_number - 1] : "Not Specified");
        data++;

        uint8_t part_number_string_number = *data;
        printf("  Part Number: %s\n", part_number_string_number ? strings[part_number_string_number - 1] : "Not Specified");
        data++;
    }

    // 2.5+
    if (major_version > 2 || (major_version == 2 && minor_version >= 5)) {
        uint8_t core_count = *data;
        if(core_count != 0xFF) {
            printf("  Core Count: %u\n", core_count);
        }
        data++;

        uint8_t core_enabled = *data;
        if(core_enabled != 0xFF) {
            printf("  Core Enabled: %u\n", core_enabled);
        }
        data++;

        uint8_t thread_count = *data;
        if(thread_count != 0xFF) {
            printf("  Thread Count: %u\n", thread_count);
        }
        data++;

        uint16_t processor_characteristics = *((uint16_t*)(void*)data);
        printf("  Processor Characteristics:\n");
        if (processor_characteristics & (1 << 0)) {
            printf("    - Reserved\n");
        }
        if (processor_characteristics & (1 << 1)) {
            printf("    - Unknown\n");
        }
        if (processor_characteristics & (1 << 2)) {
            printf("    - 64-bit Capable\n");
        }
        if (processor_characteristics & (1 << 3)) {
            printf("    - Multi-Core\n");
        }
        if (processor_characteristics & (1 << 4)) {
            printf("    - Hardware Thread\n");
        }
        if (processor_characteristics & (1 << 5)) {
            printf("    - Execute Protection\n");
        }
        if (processor_characteristics & (1 << 6)) {
            printf("    - Enhanced Virtualization\n");
        }
        if (processor_characteristics & (1 << 7)) {
            printf("    - Power/Performance Control\n");
        }
        if (processor_characteristics & (1 << 8)) {
            printf("    - 128-bit Capable\n");
        }
        if (processor_characteristics & (1 << 9)) {
            printf("    - Arm64 SoC ID\n");
        }
        data += 2;
    }

    // 2.6+
    if (major_version > 2 || (major_version == 2 && minor_version >= 6)) {
        uint16_t processor_family2 = *((uint16_t*)(void*)data);
        if(processor_family2 > 0xFF) {
            printf("  Processor Family 2: ");
            smbios_print_type_4_processor_family(processor_family2);
        }
        data += 2;
    }

    // 3.0+
    if (major_version >= 3) {
        uint16_t core_count = *(uint16_t*)(void*)data;
        if(core_count > 0xFF) {
            printf("  Core Count: %u\n", core_count);
        }
        data += 2;

        uint16_t core_enabled = *(uint16_t*)(void*)data;
        if(core_enabled > 0xFF) {
            printf("  Core Enabled: %u\n", core_enabled);
        }
        data += 2;

        uint16_t thread_count = *(uint16_t*)(void*)data;
        if(thread_count > 0xFF) {
            printf("  Thread Count: %u\n", thread_count);
        }
        data += 2;
    }

    if(data - (uint8_t*)header >= header->length) {
        // No more data to read
        return 0;
    }

    // 3.6+
    if (major_version > 3 || (major_version == 3 && minor_version >= 6)) {
        uint16_t thread_enabled = *(uint16_t*)(void*)data;
        printf("  Thread Enabled: %u\n", thread_enabled);
        data += 2;
    }

    return 0;
}

static int8_t smbios_print_type_7(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    if(string_count < 1) {
        PRINTLOG(KERNEL, LOG_ERROR, "not enough strings for Cache Information structure");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("Cache Information (type %i), handle: 0x%04x\n", header->type, header->handle);

    printf("  Socket Designation: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint16_t cache_configuration = *((uint16_t*)(void*)data);
    printf("  Cache Configuration: 0x%04x\n", cache_configuration);
    printf("    - Cache Level: L%u\n", (cache_configuration & 0x0007) + 1); // Bits 0-2
    printf("    - Socketed: %s\n", (cache_configuration & 0x0008) ? "Yes" : "No"); // Bit 3
    printf("    - Location: ");
    switch ((cache_configuration >> 5) & 0x03) { // Bits 6-5 - Cache Type
    case 0: printf("Internal\n"); break;
    case 1: printf("External\n"); break;
    case 2: printf("Reserved\n"); break;
    case 3: printf("Unknown\n"); break;
    }
    printf("    - Enabled: %s\n", (cache_configuration & (1 << 7)) ? "Yes" : "No"); // Bit 7
    printf("    - Operational Mode: "); // Bits 9-8
    switch ((cache_configuration >> 8) & 0x03) {
    case 0: printf("Write Through\n"); break;
    case 1: printf("Write Back\n"); break;
    case 2: printf("Varies With Memory Address\n"); break;
    case 3: printf("Unknown\n"); break;
    }
    data += 2;

    uint16_t maximum_cache_size          = *((uint16_t*)(void*)data);
    boolean_t maximum_cache_size_is_64kb = maximum_cache_size & 0x8000; // Bit 15 indicates if size is in 64KB units
    maximum_cache_size &= 0x7FFF; // Bits 0-14 - Cache Size
    if (maximum_cache_size_is_64kb) {
        maximum_cache_size = maximum_cache_size * 64; // Convert to KB
    }
    printf("  Maximum Cache Size: %u KB\n", maximum_cache_size);
    data += 2;

    uint16_t installed_cache_size          = *((uint16_t*)(void*)data);
    boolean_t installed_cache_size_is_64kb = installed_cache_size & 0x8000; // Bit 15 indicates if size is in 64KB units
    installed_cache_size &= 0x7FFF; // Bits 0-14 - Cache Size
    if (installed_cache_size_is_64kb) {
        installed_cache_size = installed_cache_size * 64; // Convert to KB
    }
    printf("  Installed Cache Size: %u KB\n", installed_cache_size);
    data += 2;

    uint16_t supported_sram_type = *((uint16_t*)(void*)data);
    printf("  Supported SRAM Type:\n");
    if (supported_sram_type & (1 << 0)) {
        printf("    - Other\n");
    }
    if (supported_sram_type & (1 << 1)) {
        printf("    - Unknown\n");
    }
    if (supported_sram_type & (1 << 2)) {
        printf("    - Non-Burst\n");
    }
    if (supported_sram_type & (1 << 3)) {
        printf("    - Burst\n");
    }
    if (supported_sram_type & (1 << 4)) {
        printf("    - Pipelined Burst\n");
    }
    if (supported_sram_type & (1 << 5)) {
        printf("    - Synchronous\n");
    }
    if (supported_sram_type & (1 << 6)) {
        printf("    - Asynchronous\n");
    }
    data += 2;

    uint16_t current_sram_type = *((uint16_t*)(void*)data);
    printf("  Current SRAM Type:\n");
    if (current_sram_type & (1 << 0)) {
        printf("    - Other\n");
    }
    if (current_sram_type & (1 << 1)) {
        printf("    - Unknown\n");
    }
    if (current_sram_type & (1 << 2)) {
        printf("    - Non-Burst\n");
    }
    if (current_sram_type & (1 << 3)) {
        printf("    - Burst\n");
    }
    if (current_sram_type & (1 << 4)) {
        printf("    - Pipelined Burst\n");
    }
    if (current_sram_type & (1 << 5)) {
        printf("    - Synchronous\n");
    }
    if (current_sram_type & (1 << 6)) {
        printf("    - Asynchronous\n");
    }
    data += 2;

    // 2.1+
    if (major_version > 2 || (major_version == 2 && minor_version >= 1)) {
        uint8_t cache_speed = *data;
        printf("  Cache Speed: %u ns\n", cache_speed);
        data++;

        uint8_t error_correction_type = *data;
        printf("  Error Correction Type: ");
        switch (error_correction_type) {
        case 1: printf("Other\n"); break;
        case 2: printf("Unknown\n"); break;
        case 3: printf("None\n"); break;
        case 4: printf("Parity\n"); break;
        case 5: printf("Single-bit ECC\n"); break;
        case 6: printf("Multi-bit ECC\n"); break;
        default: printf("Undefined (0x%02x)\n", error_correction_type); break;
        }
        data++;

        uint8_t system_cache_type = *data;
        printf("  System Cache Type: ");
        switch (system_cache_type) {
        case 1: printf("Other\n"); break;
        case 2: printf("Unknown\n"); break;
        case 3: printf("Instruction\n"); break;
        case 4: printf("Data\n"); break;
        case 5: printf("Unified\n"); break;
        default: printf("Undefined (0x%02x)\n", system_cache_type); break;
        }
        data++;

        uint8_t associativity = *data;
        printf("  Associativity: ");
        switch (associativity) {
        case 1: printf("Other\n"); break;
        case 2: printf("Unknown\n"); break;
        case 3: printf("Direct Mapped\n"); break;
        case 4: printf("2-way Set-Associative\n"); break;
        case 5: printf("4-way Set-Associative\n"); break;
        case 6: printf("Full Associative\n"); break;
        case 7: printf("8-way Set-Associative\n"); break;
        case 8: printf("16-way Set-Associative\n"); break;
        case 9: printf("12-way Set-Associative\n"); break;
        case 10: printf("24-way Set-Associative\n"); break;
        case 11: printf("32-way Set-Associative\n"); break;
        case 12: printf("48-way Set-Associative\n"); break;
        case 13: printf("64-way Set-Associative\n"); break;
        case 14: printf("20-way Set-Associative\n"); break;
        default: printf("Undefined (0x%02x)\n", associativity); break;
        }
        data++;
    }

    if(data - (uint8_t*)header >= header->length) {
        // No more data to read
        return 0;
    }

    // 3.1+
    if (major_version > 3 || (major_version == 3 && minor_version >= 1)) {
        uint32_t maximum_cache_size_2          = *((uint32_t*)(void*)data);
        boolean_t maximum_cache_size_2_is_64kb = maximum_cache_size_2 & 0x80000000; // Bit 31 indicates if size is in 64KB units
        maximum_cache_size_2 &= 0x7FFFFFFF; // Bits 0-30 - Cache Size
        if (maximum_cache_size_2_is_64kb) {
            maximum_cache_size_2 = maximum_cache_size_2 * 64; // Convert to KB
        }
        printf("  Maximum Cache Size (Extended): %u KB\n", maximum_cache_size_2);
        data += 4;

        uint32_t installed_cache_size_2          = *((uint32_t*)(void*)data);
        boolean_t installed_cache_size_2_is_64kb = installed_cache_size_2 & 0x80000000; // Bit 31 indicates if size is in 64KB units
        installed_cache_size_2 &= 0x7FFFFFFF; // Bits 0-30 - Cache Size
        if (installed_cache_size_2_is_64kb) {
            installed_cache_size_2 = installed_cache_size_2 * 64; // Convert to KB
        }
        printf("  Installed Cache Size (Extended): %u KB\n", installed_cache_size_2);
        data += 4;
    }

    return 0;
}

static void smbios_print_type_8_connector_type(uint8_t connector_type) {
    switch (connector_type) {
    case 0x00: printf("None\n"); break;
    case 0x01: printf("Centronics\n"); break;
    case 0x02: printf("Mini Centronics\n"); break;
    case 0x03: printf("Proprietary\n"); break;
    case 0x04: printf("DB-25 pin male\n"); break;
    case 0x05: printf("DB-25 pin female\n"); break;
    case 0x06: printf("DB-15 pin male\n"); break;
    case 0x07: printf("DB-15 pin female\n"); break;
    case 0x08: printf("DB-9 pin male\n"); break;
    case 0x09: printf("DB-9 pin female\n"); break;
    case 0x0A: printf("RJ-11\n"); break;
    case 0x0B: printf("RJ-45\n"); break;
    case 0x0C: printf("50-pin MiniSCSI\n"); break;
    case 0x0D: printf("Mini-DIN\n"); break;
    case 0x0E: printf("Micro-DIN\n"); break;
    case 0x0F: printf("PS/2\n"); break;
    case 0x10: printf("Infrared\n"); break;
    case 0x11: printf("HP-HIL\n"); break;
    case 0x12: printf("Access Bus (USB)\n"); break;
    case 0x13: printf("SSA SCSI\n"); break;
    case 0x14: printf("Circular DIN-8 male\n"); break;
    case 0x15: printf("Circular DIN-8 female\n"); break;
    case 0x16: printf("On Board IDE\n"); break;
    case 0x17: printf("On Board Floppy\n"); break;
    case 0x18: printf("9-pin Dual Inline (pin 10 cut)\n"); break;
    case 0x19: printf("25-pin Dual Inline (pin 26 cut)\n"); break;
    case 0x1A: printf("50-pin Dual Inline\n"); break;
    case 0x1B: printf("68-pin Dual Inline\n"); break;
    case 0x1C: printf("On Board Sound Input from CD-ROM\n"); break;
    case 0x1D: printf("Mini-Centronics Type-14\n"); break;
    case 0x1E: printf("Mini-Centronics Type-26\n"); break;
    case 0x1F: printf("Mini-jack (headphones)\n"); break;
    case 0x20: printf("BNC\n"); break;
    case 0x21: printf("1394\n"); break;
    case 0x22: printf("SAS/SATA Plug Receptacle\n"); break;
    case 0x23: printf("USB Type-C Receptacle\n"); break;
    case 0xA0: printf("PC-98\n"); break;
    case 0xA1: printf("PC-98Hireso\n"); break;
    case 0xA2: printf("PC-H98\n"); break;
    case 0xA3: printf("PC-98Note\n"); break;
    case 0xA4: printf("PC-98Full\n"); break;
    case 0xFF: printf("Other – Use Reference Designator Strings to supply information.\n"); break;
    }
}

static int8_t smbios_print_type_8(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    if(string_count < 2) {
        PRINTLOG(KERNEL, LOG_ERROR, "not enough strings for Port Connector Information structure");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("Port Connector Information (type %i), handle: 0x%04x\n", header->type, header->handle);

    printf("  Internal Reference Designator: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint8_t internal_connector_type = *data;
    printf("  Internal Connector Type: ");
    smbios_print_type_8_connector_type(internal_connector_type);
    data++;

    printf("  External Reference Designator: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint8_t external_connector_type = *data;
    printf("  External Connector Type: ");
    smbios_print_type_8_connector_type(external_connector_type);
    data++;

    uint8_t port_type = *data;
    printf("  Port Type: ");
    switch (port_type) {
    case 0x00: printf("None\n"); break;
    case 0x01: printf("Parallel Port XT/AT Compatible\n"); break;
    case 0x02: printf("Parallel Port PS/2\n"); break;
    case 0x03: printf("Parallel Port ECP\n"); break;
    case 0x04: printf("Parallel Port EPP\n"); break;
    case 0x05: printf("Parallel Port ECP/EPP\n"); break;
    case 0x06: printf("Serial Port XT/AT Compatible\n"); break;
    case 0x07: printf("Serial Port 16450 Compatible\n"); break;
    case 0x08: printf("Serial Port 16550 Compatible\n"); break;
    case 0x09: printf("Serial Port 16550A Compatible\n"); break;
    case 0x0A: printf("SCSI Port\n"); break;
    case 0x0B: printf("MIDI Port\n"); break;
    case 0x0C: printf("Joy Stick Port\n"); break;
    case 0x0D: printf("Keyboard Port\n"); break;
    case 0x0E: printf("Mouse Port\n"); break;
    case 0x0F: printf("SSA SCSI\n"); break;
    case 0x10: printf("USB\n"); break;
    case 0x11: printf("FireWire (IEEE P1394)\n"); break;
    case 0x12: printf("PCMCIA Type I 2\n"); break;
    case 0x13: printf("PCMCIA Type II\n"); break;
    case 0x14: printf("PCMCIA Type III\n"); break;
    case 0x15: printf("Card bus\n"); break;
    case 0x16: printf("Access Bus Port\n"); break;
    case 0x17: printf("SCSI II\n"); break;
    case 0x18: printf("SCSI Wide\n"); break;
    case 0x19: printf("PC-98\n"); break;
    case 0x1A: printf("PC-98-Hireso\n"); break;
    case 0x1B: printf("PC-H98\n"); break;
    case 0x1C: printf("Video Port\n"); break;
    case 0x1D: printf("Audio Port\n"); break;
    case 0x1E: printf("Modem Port\n"); break;
    case 0x1F: printf("Network Port\n"); break;
    case 0x20: printf("SATA\n"); break;
    case 0x21: printf("SAS\n"); break;
    case 0x22: printf("MFDP (Multi-Function Display Port)\n"); break;
    case 0x23: printf("Thunderbolt\n"); break;
    case 0xA0: printf("8251 Compatible\n"); break;
    case 0xA1: printf("8251 FIFO Compatible\n"); break;
    case 0x0FF: printf("Other\n"); break;
    }
    data++;

    return 0;
}

static int8_t smbios_print_type_9(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    if(string_count < 1) {
        PRINTLOG(KERNEL, LOG_ERROR, "not enough strings for System Slots structure");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("System Slots (type %i), handle: 0x%04x\n", header->type, header->handle);

    printf("  Slot Designation: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint8_t slot_type = *data;
    printf("  Slot Type: ");
    switch (slot_type) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("ISA\n"); break;
    case 0x04: printf("MCA\n"); break;
    case 0x05: printf("EISA\n"); break;
    case 0x06: printf("PCI\n"); break;
    case 0x07: printf("PC Card (PCMCIA)\n"); break;
    case 0x08: printf("VL-VESA\n"); break;
    case 0x09: printf("Proprietary\n"); break;
    case 0x0A: printf("Processor Card Slot\n"); break;
    case 0x0B: printf("Proprietary Memory Card Slot\n"); break;
    case 0x0C: printf("I/O Riser Card Slot\n"); break;
    case 0x0D: printf("NuBus\n"); break;
    case 0x0E: printf("PCI – 66MHz Capable\n"); break;
    case 0x0F: printf("AGP\n"); break;
    case 0x10: printf("AGP 2X\n"); break;
    case 0x11: printf("AGP 4X\n"); break;
    case 0x12: printf("PCI-X\n"); break;
    case 0x13: printf("AGP 8X\n"); break;
    case 0x14: printf("M.2 Socket 1-DP (Mechanical Key A)\n"); break;
    case 0x15: printf("M.2 Socket 1-SD (Mechanical Key E)\n"); break;
    case 0x16: printf("M.2 Socket 2 (Mechanical Key B)\n"); break;
    case 0x17: printf("M.2 Socket 3 (Mechanical Key M)\n"); break;
    case 0x18: printf("MXM Type I\n"); break;
    case 0x19: printf("MXM Type II\n"); break;
    case 0x1A: printf("MXM Type III (standard connector)\n"); break;
    case 0x1B: printf("MXM Type III (HE connector)\n"); break;
    case 0x1C: printf("MXM Type IV\n"); break;
    case 0x1D: printf("MXM 3.0 Type A\n"); break;
    case 0x1E: printf("MXM 3.0 Type B\n"); break;
    case 0x1F: printf("PCI Express Gen 2 SFF-8639 (U.2)\n"); break;
    case 0x20: printf("PCI Express Gen 3 SFF-8639 (U.2)\n"); break;
    case 0x21: printf("PCI Express Mini 52-pin (CEM spec. 2.0) with bottom-side keep-outs.\n"); break;
    case 0x22: printf("PCI Express Mini 52-pin (CEM spec. 2.0) without bottom-side keep-outs.\n"); break;
    case 0x23: printf("PCI Express Mini 76-pin (CEM spec. 2.0) Corresponds to Display-Mini card.\n"); break;
    case 0x24: printf("PCI Express Gen 4 SFF-8639 (U.2)\n"); break;
    case 0x25: printf("PCI Express Gen 5 SFF-8639 (U.2)\n"); break;
    case 0x26: printf("OCP NIC 3.0 Small Form Factor (SFF)\n"); break;
    case 0x27: printf("OCP NIC 3.0 Large Form Factor (LFF)\n"); break;
    case 0x28: printf("OCP NIC Prior to 3.0\n"); break;
    case 0x30: printf("CXL Flexbus 1.0\n"); break;
    case 0xA0: printf("PC-98/C20\n"); break;
    case 0xA1: printf("PC-98/C24\n"); break;
    case 0xA2: printf("PC-98/E\n"); break;
    case 0xA3: printf("PC-98/Local Bus\n"); break;
    case 0xA4: printf("PC-98/Card\n"); break;
    case 0xA5: printf("PCI Express\n"); break;
    case 0xA6: printf("PCI Express x1\n"); break;
    case 0xA7: printf("PCI Express x2\n"); break;
    case 0xA8: printf("PCI Express x4\n"); break;
    case 0xA9: printf("PCI Express x8\n"); break;
    case 0xAA: printf("PCI Express x16\n"); break;
    case 0xAB: printf("PCI Express Gen 2\n"); break;
    case 0xAC: printf("PCI Express Gen 2 x1\n"); break;
    case 0xAD: printf("PCI Express Gen 2 x2\n"); break;
    case 0xAE: printf("PCI Express Gen 2 x4\n"); break;
    case 0xAF: printf("PCI Express Gen 2 x8\n"); break;
    case 0xB0: printf("PCI Express Gen 2 x16\n"); break;
    case 0xB1: printf("PCI Express Gen 3\n"); break;
    case 0xB2: printf("PCI Express Gen 3 x1\n"); break;
    case 0xB3: printf("PCI Express Gen 3 x2\n"); break;
    case 0xB4: printf("PCI Express Gen 3 x4\n"); break;
    case 0xB5: printf("PCI Express Gen 3 x8\n"); break;
    case 0xB6: printf("PCI Express Gen 3 x16\n"); break;
    case 0xB8: printf("PCI Express Gen 4\n"); break;
    case 0xB9: printf("PCI Express Gen 4 x1\n"); break;
    case 0xBA: printf("PCI Express Gen 4 x2\n"); break;
    case 0xBB: printf("PCI Express Gen 4 x4\n"); break;
    case 0xBC: printf("PCI Express Gen 4 x8\n"); break;
    case 0xBD: printf("PCI Express Gen 4 x16\n"); break;
    case 0xBE: printf("PCI Express Gen 5\n"); break;
    case 0xBF: printf("PCI Express Gen 5 x1\n"); break;
    case 0xC0: printf("PCI Express Gen 5 x2\n"); break;
    case 0xC1: printf("PCI Express Gen 5 x4\n"); break;
    case 0xC2: printf("PCI Express Gen 5 x8\n"); break;
    case 0xC3: printf("PCI Express Gen 5 x16\n"); break;
    case 0xC4: printf("PCI Express Gen 6 and Beyond\n"); break;
    case 0xC5: printf("Enterprise and Datacenter 1U E1 Form Factor Slot (EDSFF E1.S, E1.L)\n"); break;
    case 0xC6: printf("Enterprise and Datacenter 3\" E3 Form Factor Slot (EDSFF E3.S, E3.L)\n"); break;
    default: printf("Undefined (0x%02x)\n", slot_type); break;
    }
    data++;

    uint8_t slot_data_bus_width = *data;
    printf("  Slot Data Bus Width: ");
    switch (slot_data_bus_width) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("8 bit\n"); break;
    case 0x04: printf("16 bit\n"); break;
    case 0x05: printf("32 bit\n"); break;
    case 0x06: printf("64 bit\n"); break;
    case 0x07: printf("128 bit\n"); break;
    case 0x08: printf("1x or x1\n"); break;
    case 0x09: printf("2x or x2\n"); break;
    case 0x0A: printf("4x or x4\n"); break;
    case 0x0B: printf("8x or x8\n"); break;
    case 0x0C: printf("12x or x12\n"); break;
    case 0x0D: printf("16x or x16\n"); break;
    case 0x0E: printf("32x or x32\n"); break;
    default: printf("Undefined (0x%02x)\n", slot_data_bus_width); break;
    }
    data++;

    uint8_t current_usage = *data;
    printf("  Current Usage: ");
    switch (current_usage) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("Available\n"); break;
    case 0x04: printf("In use\n"); break;
    case 0x05: printf("Unavailable\n"); break;
    default: printf("Undefined (0x%02x)\n", current_usage); break;
    }
    data++;

    uint8_t slot_length = *data;
    printf("  Slot Length: ");
    switch (slot_length) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("Short Length\n"); break;
    case 0x04: printf("Long Length\n"); break;
    case 0x05: printf("2.5\" drive form factor\n"); break;
    case 0x06: printf("3.5\" drive form factor\n"); break;
    default: printf("Undefined (0x%02x)\n", slot_length); break;
    }
    data++;

    uint16_t slot_id = *((uint16_t*)(void*)data);
    printf("  Slot ID: 0x%04x\n", slot_id);
    data += 2;

    uint8_t slot_characteristics_1 = *data;
    printf("  Slot Characteristics 1:\n");
    if (slot_characteristics_1 & (1 << 0)) {
        printf("    - Characteristics unknown\n");
    }
    if (slot_characteristics_1 & (1 << 1)) {
        printf("    - Provides 5.0 V\n");
    }
    if (slot_characteristics_1 & (1 << 2)) {
        printf("    - Provides 3.3 V\n");
    }
    if (slot_characteristics_1 & (1 << 3)) {
        printf("    - Shared slot\n");
    }
    if (slot_characteristics_1 & (1 << 4)) {
        printf("    - PC Card-16 is supported\n");
    }
    if (slot_characteristics_1 & (1 << 5)) {
        printf("    - CardBus is supported\n");
    }
    if (slot_characteristics_1 & (1 << 6)) {
        printf("    - Zoom Video is supported\n");
    }
    if (slot_characteristics_1 & (1 << 7)) {
        printf("    - Modem ring resume is supported\n");
    }
    data++;

    uint8_t slot_characteristics_2 = *data;
    printf("  Slot Characteristics 2:\n");
    if (slot_characteristics_2 & (1 << 0)) {
        printf("    - PME# signal is supported\n");
    }
    if (slot_characteristics_2 & (1 << 1)) {
        printf("    - Hot-plug devices are supported\n");
    }
    if (slot_characteristics_2 & (1 << 2)) {
        printf("    - SMBus signal is supported\n");
    }
    if (slot_characteristics_2 & (1 << 3)) {
        printf("    - PCIe bifurcation is supported\n");
    }
    if (slot_characteristics_2 & (1 << 4)) {
        printf("    - Async/Surprise removal is supported\n");
    }
    if (slot_characteristics_2 & (1 << 5)) {
        printf("    - Flexbus slot, CXL 1.0 Capable\n");
    }
    if (slot_characteristics_2 & (1 << 6)) {
        printf("    - Flexbus slot, CXL 2.0 Capable\n");
    }
    if (slot_characteristics_2 & (1 << 7)) {
        printf("    - Flexbus slot, CXL 3.0 Capable\n");
    }
    data++;

    // 2.6+
    if (major_version > 2 || (major_version == 2 && minor_version >= 6)) {
        uint16_t segment_group_number = *((uint16_t*)(void*)data);
        data += 2;

        uint8_t bus_number = *data;
        data++;

        uint8_t device_function_number = *data;
        data++;

        printf("  Device Address: %04x:%02x:%02x.%02x\n", segment_group_number, bus_number, device_function_number >> 3, device_function_number & 0x07);
    }

    if(data - (uint8_t*)header >= header->length) {
        // No more data to read
        return 0;
    }

    // 3.2+
    if (major_version > 3 || (major_version == 3 && minor_version >= 2)) {
        uint8_t data_bus_width = *data;
        printf("  Data Bus Width: %u\n", data_bus_width);
        data++;

        uint8_t peer_grouping_count = *data;
        printf("  Peer Grouping Count: %u\n", peer_grouping_count);
        data++;

        for(uint8_t i = 0; i < peer_grouping_count; i++) {
            uint16_t peer_group_number = *((uint16_t*)(void*)data);
            data += 2;

            uint8_t peer_bus_number = *data;
            data++;

            uint8_t peer_device_function_number = *data;
            data++;

            printf("  Peer Device %u Address: %04x:%02x:%02x.%02x\n", i + 1, peer_group_number, peer_bus_number, peer_device_function_number >> 3, peer_device_function_number & 0x07);

            uint8_t peer_bus_width = *data;
            printf("  Peer Device %u Bus Width: %u\n", i + 1, peer_bus_width);
            data++;
        }
    }

    if(data - (uint8_t*)header >= header->length) {
        // No more data to read
        return 0;
    }

    // TODO: 3.4 and 3.5 additions

    return 0;
}

static int8_t smbios_print_type_10(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("On Board Devices Information (type %i) handle: 0x%04x\n", header->type, header->handle);

    /*
     * Type 10 is a variable length structure containing N device entries.
     * Each entry is 2 bytes long.
     * Number of entries = (header->length - sizeof(header)) / 2
     */
    uint8_t entry_count = (header->length - sizeof(smbios_structure_header_t)) / 2;

    for (uint8_t i = 0; i < entry_count; i++) {
        uint8_t device_type_byte = *data;
        data++;
        uint8_t description_string_index = *data;
        data++;

        boolean_t enabled   = (device_type_byte & 0x80) >> 7;
        uint8_t device_type = device_type_byte & 0x7F;

        printf("  Device %u:\n", i + 1);
        printf("    Type: ");
        switch (device_type) {
        case 0x01: printf("Other\n"); break;
        case 0x02: printf("Unknown\n"); break;
        case 0x03: printf("Video\n"); break;
        case 0x04: printf("SCSI Controller\n"); break;
        case 0x05: printf("Ethernet\n"); break;
        case 0x06: printf("Token Ring\n"); break;
        case 0x07: printf("Sound\n"); break;
        case 0x08: printf("PATA Controller\n"); break;
        case 0x09: printf("SATA Controller\n"); break;
        case 0x0A: printf("SAS Controller\n"); break;
        default: printf("Undefined (0x%02x)\n", device_type); break;
        }

        printf("    Status: %s\n", enabled ? "Enabled" : "Disabled");
        printf("    Description: %s\n", (description_string_index && description_string_index <= string_count)
               ? strings[description_string_index - 1] : "Not Specified");
    }

    if (entry_count == 0) {
        printf("  No devices specified.\n");
    }

    return 0;
}

static int8_t smbios_print_type_11(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("OEM Strings (type %i) handle: 0x%04x\n", header->type, header->handle);

    if (header->length < 5) {
        PRINTLOG(KERNEL, LOG_ERROR, "OEM Strings structure too short");
        return -1;
    }

    uint8_t count = *data;
    printf("  Count: %u\n", count);

    for (uint8_t i = 0; i < count; i++) {
        printf("    String %u: %s\n", i + 1, (i < string_count) ? strings[i] : "Not Specified");
    }

    if (count == 0) {
        printf("  No OEM strings specified.\n");
    }

    return 0;
}

static int8_t smbios_print_type_12(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("System Configuration Options (type %i) handle: 0x%04x\n", header->type, header->handle);

    if (header->length < 5) {
        PRINTLOG(KERNEL, LOG_ERROR, "System Configuration Options structure too short");
        return -1;
    }

    uint8_t count = *data;
    printf("  Count: %u\n", count);

    for (uint8_t i = 0; i < count; i++) {
        printf("    Option %u: %s\n", i + 1, (i < string_count) ? strings[i] : "Not Specified");
    }

    if (count == 0) {
        printf("  No configuration options specified.\n");
    }

    return 0;
}

static int8_t smbios_print_type_13(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("BIOS Language Information (type %i) handle: 0x%04x\n", header->type, header->handle);

    if (header->length < 5) {
        PRINTLOG(KERNEL, LOG_ERROR, "BIOS Language Information structure too short");
        return -1;
    }

    uint8_t installable_languages = *data;
    data++;

    uint8_t flags = *data;
    data++;

    // Reserved 15 bytes
    data += 15;

    uint8_t current_language_index = *data;
    data++;

    printf("  Installable Languages: %u\n", installable_languages);
    printf("  Flags:\n");
    printf("    - Abbreviated format: %s\n", (flags & (1 << 0)) ? "Yes" : "No");

    printf("  Current Language: %s\n", (current_language_index && current_language_index <= string_count)
           ? strings[current_language_index - 1] : "Not Specified");

    printf("  Available Languages:\n");
    for (uint8_t i = 0; i < installable_languages; i++) {
        printf("    - %s\n", strings[i]);
    }

    return 0;
}

static int8_t smbios_print_type_16(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    if (header->length < 0x0F) {
        PRINTLOG(KERNEL, LOG_ERROR, "Physical Memory Array structure too short");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("Physical Memory Array (type %i) handle: 0x%04x\n", header->type, header->handle);

    uint8_t location = *data;
    printf("  Location: ");
    switch (location) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("System board or motherboard\n"); break;
    case 0x04: printf("ISA add-on card\n"); break;
    case 0x05: printf("EISA add-on card\n"); break;
    case 0x06: printf("PCI add-on card\n"); break;
    case 0x07: printf("MCA add-on card\n"); break;
    case 0x08: printf("PCMCIA add-on card\n"); break;
    case 0x09: printf("Proprietary add-on card\n"); break;
    case 0x0A: printf("NuBus\n"); break;
    case 0xA0: printf("PC-98/C20 add-on card\n"); break;
    case 0xA1: printf("PC-98/C24 add-on card\n"); break;
    case 0xA2: printf("PC-98/E add-on card\n"); break;
    case 0xA3: printf("PC-98/Local bus add-on card\n"); break;
    case 0xA4: printf("CXL add-on card\n"); break;
    default: printf("Undefined (0x%02x)\n", location); break;
    }
    data++;

    uint8_t use = *data;
    printf("  Use: ");
    switch (use) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("System memory\n"); break;
    case 0x04: printf("Video memory\n"); break;
    case 0x05: printf("Flash memory\n"); break;
    case 0x06: printf("Non-volatile RAM\n"); break;
    case 0x07: printf("Cache memory\n"); break;
    default: printf("Undefined (0x%02x)\n", use); break;
    }
    data++;

    uint8_t error_correction = *data;
    printf("  Memory Error Correction: ");
    switch (error_correction) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("None\n"); break;
    case 0x04: printf("Parity\n"); break;
    case 0x05: printf("Single-bit ECC\n"); break;
    case 0x06: printf("Multi-bit ECC\n"); break;
    case 0x07: printf("CRC\n"); break;
    default: printf("Undefined (0x%02x)\n", error_correction); break;
    }
    data++;

    uint32_t maximum_capacity = *((uint32_t*)(void*)data);
    if (maximum_capacity == 0x80000000) {
        // Use Extended Maximum Capacity field (SMBIOS 2.7+)
        uint64_t extended_capacity = *((uint64_t*)(void*)((uint8_t*)header + 0x0F));
        printf("  Maximum Capacity: %llu KB (%llu GB)\n", extended_capacity, extended_capacity / (1024 * 1024));
    } else {
        printf("  Maximum Capacity: %u KB (%u GB)\n", maximum_capacity, maximum_capacity / (1024 * 1024));
    }
    data += 4;

    uint16_t error_info_handle = *((uint16_t*)(void*)data);
    printf("  Memory Error Information Handle: ");
    if (error_info_handle == 0xFFFE) {
        printf("Not Provided\n");
    }else if (error_info_handle == 0xFFFF) {
        printf("No Error\n");
    }else {
        printf("0x%04x\n", error_info_handle);
    }
    data += 2;

    uint16_t number_of_devices = *((uint16_t*)(void*)data);
    printf("  Number of Memory Devices: %u\n", number_of_devices);
    data += 2;

    return 0;
}

static int8_t smbios_print_type_17(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);
    UNUSED(string_count);

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("Memory Device (type %i) handle: 0x%04x\n", header->type, header->handle);

    uint16_t array_handle = *((uint16_t*)(void*)data);
    printf("  Physical Memory Array Handle: 0x%04x\n", array_handle);
    data += 2;

    uint16_t error_info_handle = *((uint16_t*)(void*)data);
    printf("  Memory Error Information Handle: ");
    if (error_info_handle == 0xFFFE) {
        printf("Not Provided\n");
    }else if (error_info_handle == 0xFFFF) {
        printf("No Error\n");
    }else {
        printf("0x%04x\n", error_info_handle);
    }
    data += 2;

    uint16_t total_width = *((uint16_t*)(void*)data);
    if (total_width != 0xFFFF) {
        printf("  Total Width: %u bits\n", total_width);
    }else{
        printf("  Total Width: Unknown\n");
    }
    data += 2;

    uint16_t data_width = *((uint16_t*)(void*)data);
    if (data_width != 0xFFFF) {
        printf("  Data Width: %u bits\n", data_width);
    }else{
        printf("  Data Width: Unknown\n");
    }
    data += 2;

    uint16_t size = *((uint16_t*)(void*)data);
    printf("  Size: ");
    if (size == 0) {
        printf("No Device Installed\n");
    }else if (size == 0xFFFF) {
        printf("Unknown\n");
    }else if (size == 0x7FFF) {
        // Use Extended Size field (SMBIOS 2.7+)
        uint32_t extended_size = *((uint32_t*)(void*)((uint8_t*)header + 0x1C));
        printf("%u MB\n", extended_size & 0x7FFFFFFF);
    } else {
        boolean_t size_in_kb = size & 0x8000;
        uint16_t size_val    = size & 0x7FFF;
        if (size_in_kb) {
            printf("%u KB\n", size_val);
        }else{
            printf("%u MB\n", size_val);
        }
    }
    data += 2;

    uint8_t form_factor = *data;
    printf("  Form Factor: ");
    switch (form_factor) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("SIMM\n"); break;
    case 0x04: printf("SIP\n"); break;
    case 0x05: printf("Chip\n"); break;
    case 0x06: printf("DIP\n"); break;
    case 0x07: printf("ZIP\n"); break;
    case 0x08: printf("Proprietary Card\n"); break;
    case 0x09: printf("DIMM\n"); break;
    case 0x0A: printf("TSOP\n"); break;
    case 0x0B: printf("Row of chips\n"); break;
    case 0x0C: printf("RIMM\n"); break;
    case 0x0D: printf("SODIMM\n"); break;
    case 0x0E: printf("SRIMM\n"); break;
    case 0x0F: printf("FB-DIMM\n"); break;
    case 0x10: printf("Die\n"); break;
    default: printf("Undefined (0x%02x)\n", form_factor); break;
    }
    data++;

    uint8_t device_set = *data;
    if (device_set == 0) {
        printf("  Device Set: None\n");
    }else if (device_set == 0xFF) {
        printf("  Device Set: Unknown\n");
    }else {
        printf("  Device Set: %u\n", device_set);
    }
    data++;

    printf("  Device Locator: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    printf("  Bank Locator: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint8_t memory_type = *data;
    printf("  Memory Type: ");
    switch (memory_type) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("DRAM\n"); break;
    case 0x04: printf("EDRAM\n"); break;
    case 0x05: printf("VRAM\n"); break;
    case 0x06: printf("SRAM\n"); break;
    case 0x07: printf("RAM\n"); break;
    case 0x08: printf("ROM\n"); break;
    case 0x09: printf("FLASH\n"); break;
    case 0x0A: printf("EEPROM\n"); break;
    case 0x0B: printf("FEPROM\n"); break;
    case 0x0C: printf("EPROM\n"); break;
    case 0x0D: printf("CDRAM\n"); break;
    case 0x0E: printf("3DRAM\n"); break;
    case 0x0F: printf("SDRAM\n"); break;
    case 0x10: printf("SGRAM\n"); break;
    case 0x11: printf("RDRAM\n"); break;
    case 0x12: printf("DDR\n"); break;
    case 0x13: printf("DDR2\n"); break;
    case 0x14: printf("DDR2 FB-DIMM\n"); break;
    case 0x15: printf("Reserved\n"); break;
    case 0x16: printf("Reserved\n"); break;
    case 0x17: printf("Reserved\n"); break;
    case 0x18: printf("DDR3\n"); break;
    case 0x19: printf("FBD2\n"); break;
    case 0x1A: printf("DDR4\n"); break;
    case 0x1B: printf("LPDDR\n"); break;
    case 0x1C: printf("LPDDR2\n"); break;
    case 0x1D: printf("LPDDR3\n"); break;
    case 0x1E: printf("LPDDR4\n"); break;
    case 0x1F: printf("Logical non-volatile device\n"); break;
    case 0x20: printf("HBM\n"); break;
    case 0x21: printf("HBM2\n"); break;
    case 0x22: printf("DDR5\n"); break;
    case 0x23: printf("LPDDR5\n"); break;
    case 0x24: printf("HBM3\n"); break;
    default: printf("Undefined (0x%02x)\n", memory_type); break;
    }
    data++;

    uint16_t type_detail = *((uint16_t*)(void*)data);
    printf("  Type Detail:\n");
    if (type_detail & (1 << 1)) {
        printf("    - Other\n");
    }
    if (type_detail & (1 << 2)) {
        printf("    - Unknown\n");
    }
    if (type_detail & (1 << 3)) {
        printf("    - Fast-paged\n");
    }
    if (type_detail & (1 << 4)) {
        printf("    - Static column\n");
    }
    if (type_detail & (1 << 5)) {
        printf("    - Pseudo-static\n");
    }
    if (type_detail & (1 << 6)) {
        printf("    - RAMBUS\n");
    }
    if (type_detail & (1 << 7)) {
        printf("    - Synchronous\n");
    }
    if (type_detail & (1 << 8)) {
        printf("    - CMOS\n");
    }
    if (type_detail & (1 << 9)) {
        printf("    - EDO\n");
    }
    if (type_detail & (1 << 10)) {
        printf("    - Window DRAM\n");
    }
    if (type_detail & (1 << 11)) {
        printf("    - Cache DRAM\n");
    }
    if (type_detail & (1 << 12)) {
        printf("    - Non-volatile\n");
    }
    if (type_detail & (1 << 13)) {
        printf("    - Registered (Buffered)\n");
    }
    if (type_detail & (1 << 14)) {
        printf("    - Unbuffered (Unregistered)\n");
    }
    if (type_detail & (1 << 15)) {
        printf("    - LRDIMM\n");
    }
    data += 2;

    // 2.3+
    if (major_version > 2 || (major_version == 2 && minor_version >= 3)) {
        uint16_t speed = *((uint16_t*)(void*)data);
        if (speed == 0) {
            printf("  Speed: Unknown\n");
        }else{
            printf("  Speed: %u MT/s\n", speed);
        }
        data += 2;

        printf("  Manufacturer: %s\n", *data ? strings[*data - 1] : "Not Specified");
        data++;

        printf("  Serial Number: %s\n", *data ? strings[*data - 1] : "Not Specified");
        data++;

        printf("  Asset Tag: %s\n", *data ? strings[*data - 1] : "Not Specified");
        data++;

        printf("  Part Number: %s\n", *data ? strings[*data - 1] : "Not Specified");
        data++;
    }

    // 2.6+
    if (major_version > 2 || (major_version == 2 && minor_version >= 6)) {
        uint8_t attributes = *data;
        printf("  Attributes: 0x%02x\n", attributes);
        if (attributes & 0x0F) {
            printf("    - Rank: %u\n", attributes & 0x0F);
        }
        data++;
    }

    // 2.7+
    if (major_version > 2 || (major_version == 2 && minor_version >= 7)) {
        uint32_t extended_size = *((uint32_t*)(void*)data);
        if (extended_size != 0) {
            printf("  Extended Size: %u MB\n", extended_size & 0x7FFFFFFF);
        }
        data += 4;

        uint16_t conf_speed = *((uint16_t*)(void*)data);
        if (conf_speed == 0) {
            printf("  Configured Memory Speed: Unknown\n");
        }else{
            printf("  Configured Memory Speed: %u MT/s\n", conf_speed);
        }
        data += 2;
    }

    // 2.8+
    if (major_version > 2 || (major_version == 2 && minor_version >= 8)) {
        uint16_t min_voltage = *((uint16_t*)(void*)data);
        if (min_voltage == 0) {
            printf("  Minimum Voltage: Unknown\n");
        }else{
            printf("  Minimum Voltage: %u.%u V\n", min_voltage / 1000, min_voltage % 1000);
        }
        data += 2;

        uint16_t max_voltage = *((uint16_t*)(void*)data);
        if (max_voltage == 0) {
            printf("  Maximum Voltage: Unknown\n");
        }else{
            printf("  Maximum Voltage: %u.%u V\n", max_voltage / 1000, max_voltage % 1000);
        }
        data += 2;

        uint16_t conf_voltage = *((uint16_t*)(void*)data);
        if (conf_voltage == 0) {
            printf("  Configured Voltage: Unknown\n");
        }else{
            printf("  Configured Voltage: %u.%u V\n", conf_voltage / 1000, conf_voltage % 1000);
        }
        data += 2;
    }

    // 3.2+
    if (major_version > 3 || (major_version == 3 && minor_version >= 2)) {
        uint8_t memory_technology = *data;
        printf("  Memory Technology: ");
        switch (memory_technology) {
        case 0x01: printf("Other\n"); break;
        case 0x02: printf("Unknown\n"); break;
        case 0x03: printf("DRAM\n"); break;
        case 0x04: printf("NVDIMM-N\n"); break;
        case 0x05: printf("NVDIMM-F\n"); break;
        case 0x06: printf("NVDIMM-P\n"); break;
        case 0x07: printf("Intel Optane persistent memory\n"); break;
        default: printf("Undefined (0x%02x)\n", memory_technology); break;
        }
        data++;

        uint16_t operating_mode_cap = *((uint16_t*)(void*)data);
        printf("  Memory Operating Mode Capability: 0x%04x\n", operating_mode_cap);
        if (operating_mode_cap & (1 << 1)) {
            printf("    - Other\n");
        }
        if (operating_mode_cap & (1 << 2)) {
            printf("    - Unknown\n");
        }
        if (operating_mode_cap & (1 << 3)) {
            printf("    - Volatile memory\n");
        }
        if (operating_mode_cap & (1 << 4)) {
            printf("    - Byte-addressable persistent memory\n");
        }
        if (operating_mode_cap & (1 << 5)) {
            printf("    - Block-addressable persistent memory\n");
        }
        data += 2;

        printf("  Firmware Version: %s\n", *data ? strings[*data - 1] : "Not Specified");
        data++;

        uint16_t module_manuf_id = *((uint16_t*)(void*)data);
        printf("  Module Manufacturer ID: 0x%04x\n", module_manuf_id);
        data += 2;

        uint16_t module_product_id = *((uint16_t*)(void*)data);
        printf("  Module Product ID: 0x%04x\n", module_product_id);
        data += 2;

        uint16_t memory_subsystem_id = *((uint16_t*)(void*)data);
        printf("  Memory Subsystem Controller Product ID: 0x%04x\n", memory_subsystem_id);
        data += 2;

        uint16_t memory_subsystem_manuf_id = *((uint16_t*)(void*)data);
        printf("  Memory Subsystem Controller Manufacturer ID: 0x%04x\n", memory_subsystem_manuf_id);
        data += 2;

        uint64_t non_volatile_size = *((uint64_t*)(void*)data);
        if (non_volatile_size != 0xFFFFFFFFFFFFFFFFULL) {
            printf("  Non-volatile Size: %llu bytes\n", non_volatile_size);
        }
        data += 8;

        uint64_t volatile_size = *((uint64_t*)(void*)data);
        if (volatile_size != 0xFFFFFFFFFFFFFFFFULL) {
            printf("  Volatile Size: %llu bytes\n", volatile_size);
        }
        data += 8;

        uint64_t cache_size = *((uint64_t*)(void*)data);
        if (cache_size != 0xFFFFFFFFFFFFFFFFULL) {
            printf("  Cache Size: %llu bytes\n", cache_size);
        }
        data += 8;

        uint64_t logical_size = *((uint64_t*)(void*)data);
        if (logical_size != 0xFFFFFFFFFFFFFFFFULL) {
            printf("  Logical Size: %llu bytes\n", logical_size);
        }
        data += 8;
    }

    // 3.3+
    if (major_version > 3 || (major_version == 3 && minor_version >= 3)) {
        uint32_t extended_speed = *((uint32_t*)(void*)data);
        if (extended_speed != 0) {
            printf("  Extended Speed: %u MT/s\n", extended_speed);
        }
        data += 4;

        uint32_t extended_conf_speed = *((uint32_t*)(void*)data);
        if (extended_conf_speed != 0) {
            printf("  Extended Configured Memory Speed: %u MT/s\n", extended_conf_speed);
        }
        data += 4;
    }

    // TODO: 3.7+ fields

    return 0;
}

static int8_t smbios_print_type_18(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    if (header->length < 0x17) {
        PRINTLOG(KERNEL, LOG_ERROR, "32-bit Memory Error Information structure too short");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("32-bit Memory Error Information (type %i) handle: 0x%04x\n", header->type, header->handle);

    uint8_t error_type = *data;
    printf("  Error Type: ");
    switch (error_type) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("OK\n"); break;
    case 0x04: printf("Bad read\n"); break;
    case 0x05: printf("Parity error\n"); break;
    case 0x06: printf("Single-bit error\n"); break;
    case 0x07: printf("Double-bit error\n"); break;
    case 0x08: printf("Multi-bit error\n"); break;
    case 0x09: printf("Nibble error\n"); break;
    case 0x0A: printf("Checksum error\n"); break;
    case 0x0B: printf("CRC error\n"); break;
    case 0x0C: printf("Corrected single-bit error\n"); break;
    case 0x0D: printf("Corrected error\n"); break;
    case 0x0E: printf("Uncorrectable error\n"); break;
    default: printf("Undefined (0x%02x)\n", error_type); break;
    }
    data++;

    uint8_t error_granularity = *data;
    printf("  Error Granularity: ");
    switch (error_granularity) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("Device level\n"); break;
    case 0x04: printf("Memory partition level\n"); break;
    default: printf("Undefined (0x%02x)\n", error_granularity); break;
    }
    data++;

    uint8_t error_operation = *data;
    printf("  Error Operation: ");
    switch (error_operation) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("Read\n"); break;
    case 0x04: printf("Write\n"); break;
    case 0x05: printf("Partial write\n"); break;
    default: printf("Undefined (0x%02x)\n", error_operation); break;
    }
    data++;

    uint32_t vendor_syndrome = *((uint32_t*)(void*)data);
    if (vendor_syndrome == 0) {
        printf("  Vendor-specific Syndrome: Unknown\n");
    } else {
        printf("  Vendor-specific Syndrome: 0x%08x\n", vendor_syndrome);
    }
    data += 4;

    uint32_t bus_address = *((uint32_t*)(void*)data);
    if (bus_address == 0x80000000) {
        printf("  Memory Array Error Address: Unknown\n");
    } else {
        printf("  Memory Array Error Address: 0x%08x\n", bus_address);
    }
    data += 4;

    uint32_t device_address = *((uint32_t*)(void*)data);
    if (device_address == 0x80000000) {
        printf("  Device Error Address: Unknown\n");
    } else {
        printf("  Device Error Address: 0x%08x\n", device_address);
    }
    data += 4;

    uint32_t error_resolution = *((uint32_t*)(void*)data);
    if (error_resolution == 0x80000000) {
        printf("  Error Resolution: Unknown\n");
    } else {
        printf("  Error Resolution: %u bytes\n", error_resolution);
    }
    data += 4;

    return 0;
}

static int8_t smbios_print_type_19(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    if (header->length < 0x0F) {
        PRINTLOG(KERNEL, LOG_ERROR, "Memory Array Mapped Address structure too short");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("Memory Array Mapped Address (type %i) handle: 0x%04x\n", header->type, header->handle);

    uint32_t starting_address = *((uint32_t*)(void*)data);
    data += 4;
    uint32_t ending_address = *((uint32_t*)(void*)data);
    data += 4;

    uint16_t memory_array_handle = *((uint16_t*)(void*)data);
    data += 2;

    uint8_t partition_width = *data;
    data++;

    uint64_t start_addr_final = starting_address;
    uint64_t end_addr_final   = ending_address;

    if (starting_address == 0xFFFFFFFF && ending_address == 0xFFFFFFFF && header->length >= 0x1F) {
        // Use Extended addresses (SMBIOS 2.7+)
        start_addr_final = *((uint64_t*)(void*)((uint8_t*)header + 0x0F));
        end_addr_final   = *((uint64_t*)(void*)((uint8_t*)header + 0x17));
        printf("  Starting Address: 0x%016llx\n", start_addr_final);
        printf("  Ending Address:   0x%016llx\n", end_addr_final);
    } else {
        // Standard 32-bit addresses are in KB
        printf("  Starting Address: 0x%08x (%u MB)\n", starting_address, starting_address / 1024);
        printf("  Ending Address:   0x%08x (%u MB)\n", ending_address, ending_address / 1024);
    }

    printf("  Memory Array Handle: 0x%04x\n", memory_array_handle);
    printf("  Partition Width: %u\n", partition_width);

    return 0;
}

static int8_t smbios_print_type_20(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    if (header->length < 0x13) {
        PRINTLOG(KERNEL, LOG_ERROR, "Memory Device Mapped Address structure too short");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("Memory Device Mapped Address (type %i) handle: 0x%04x\n", header->type, header->handle);

    uint32_t starting_address = *((uint32_t*)(void*)data);
    data += 4;
    uint32_t ending_address = *((uint32_t*)(void*)data);
    data += 4;

    uint16_t memory_device_handle = *((uint16_t*)(void*)data);
    data += 2;

    uint16_t memory_array_mapped_address_handle = *((uint16_t*)(void*)data);
    data += 2;

    uint8_t partition_row_position = *data;
    data++;

    uint8_t interleave_position = *data;
    data++;

    uint8_t interleaved_data_depth = *data;
    data++;

    uint64_t start_addr_final = starting_address;
    uint64_t end_addr_final   = ending_address;

    if (starting_address == 0xFFFFFFFF && ending_address == 0xFFFFFFFF && header->length >= 0x23) {
        // Use Extended addresses (SMBIOS 2.7+)
        start_addr_final = *((uint64_t*)(void*)((uint8_t*)header + 0x13));
        end_addr_final   = *((uint64_t*)(void*)((uint8_t*)header + 0x1B));
        printf("  Starting Address: 0x%016llx\n", start_addr_final);
        printf("  Ending Address:   0x%016llx\n", end_addr_final);
    } else {
        // Standard 32-bit addresses are in KB
        printf("  Starting Address: 0x%08x (%u MB)\n", starting_address, starting_address / 1024);
        printf("  Ending Address:   0x%08x (%u MB)\n", ending_address, ending_address / 1024);
    }

    printf("  Memory Device Handle: 0x%04x\n", memory_device_handle);
    printf("  Memory Array Mapped Address Handle: 0x%04x\n", memory_array_mapped_address_handle);

    if (partition_row_position == 0) {
        printf("  Partition Row Position: Unknown\n");
    } else if (partition_row_position == 0xFF) {
        printf("  Partition Row Position: Reserved\n");
    } else {
        printf("  Partition Row Position: %u\n", partition_row_position);
    }

    if (interleave_position == 0) {
        printf("  Interleave Position: Non-interleaved\n");
    } else if (interleave_position == 0xFF) {
        printf("  Interleave Position: Unknown\n");
    } else {
        printf("  Interleave Position: %u\n", interleave_position);
    }

    if (interleaved_data_depth == 0) {
        printf("  Interleaved Data Depth: Non-interleaved\n");
    } else if (interleaved_data_depth == 0xFF) {
        printf("  Interleaved Data Depth: Unknown\n");
    } else {
        printf("  Interleaved Data Depth: %u\n", interleaved_data_depth);
    }

    return 0;
}

static int8_t smbios_print_type_32(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    if (header->length < 0x0B) {
        PRINTLOG(KERNEL, LOG_ERROR, "System Boot Information structure too short");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("System Boot Information (type %i) handle: 0x%04x\n", header->type, header->handle);

    // Reserved 6 bytes
    data += 6;

    uint16_t boot_status = *data;
    printf("  Boot Status: ");
    switch (boot_status) {
    case 0: printf("No errors detected (Success)\n"); break;
    case 1: printf("No bootable media\n"); break;
    case 2: printf("Operating system load failed\n"); break;
    case 3: printf("Hardware firmware error\n"); break;
    case 4: printf("Operating system-specific error\n"); break;
    case 5: printf("User-requested boot, no errors detected\n"); break;
    case 6: printf("System security violation\n"); break;
    case 7: printf("Previous-boot image is corrupt\n"); break;
    case 8: printf("System configuration error\n"); break;
    default:
        if (boot_status >= 128 && boot_status <= 191) {
            printf("OEM-specific boot status (0x%02x)\n", boot_status);
        } else if (boot_status >= 192 && boot_status <= 255) {
            printf("Product-specific boot status (0x%02x)\n", boot_status);
        } else {
            printf("Undefined (0x%02x)\n", boot_status);
        }
        break;
    }

    return 0;
}

static int8_t smbios_print_type_33(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    if (header->length < 0x1F) {
        PRINTLOG(KERNEL, LOG_ERROR, "64-bit Memory Error Information structure too short");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("64-bit Memory Error Information (type %i) handle: 0x%04x\n", header->type, header->handle);

    uint8_t error_type = *data;
    printf("  Error Type: ");
    switch (error_type) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("OK\n"); break;
    case 0x04: printf("Bad read\n"); break;
    case 0x05: printf("Parity error\n"); break;
    case 0x06: printf("Single-bit error\n"); break;
    case 0x07: printf("Double-bit error\n"); break;
    case 0x08: printf("Multi-bit error\n"); break;
    case 0x09: printf("Nibble error\n"); break;
    case 0x0A: printf("Checksum error\n"); break;
    case 0x0B: printf("CRC error\n"); break;
    case 0x0C: printf("Corrected single-bit error\n"); break;
    case 0x0D: printf("Corrected error\n"); break;
    case 0x0E: printf("Uncorrectable error\n"); break;
    default: printf("Undefined (0x%02x)\n", error_type); break;
    }
    data++;

    uint8_t error_granularity = *data;
    printf("  Error Granularity: ");
    switch (error_granularity) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("Device level\n"); break;
    case 0x04: printf("Memory partition level\n"); break;
    default: printf("Undefined (0x%02x)\n", error_granularity); break;
    }
    data++;

    uint8_t error_operation = *data;
    printf("  Error Operation: ");
    switch (error_operation) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("Read\n"); break;
    case 0x04: printf("Write\n"); break;
    case 0x05: printf("Partial write\n"); break;
    default: printf("Undefined (0x%02x)\n", error_operation); break;
    }
    data++;

    uint32_t vendor_syndrome = *((uint32_t*)(void*)data);
    if (vendor_syndrome == 0) {
        printf("  Vendor-specific Syndrome: Unknown\n");
    } else {
        printf("  Vendor-specific Syndrome: 0x%08x\n", vendor_syndrome);
    }
    data += 4;

    uint64_t bus_address = *((uint64_t*)(void*)data);
    if (bus_address == 0x8000000000000000ULL) {
        printf("  Memory Array Error Address: Unknown\n");
    } else {
        printf("  Memory Array Error Address: 0x%016llx\n", bus_address);
    }
    data += 8;

    uint64_t device_address = *((uint64_t*)(void*)data);
    if (device_address == 0x8000000000000000ULL) {
        printf("  Device Error Address: Unknown\n");
    } else {
        printf("  Device Error Address: 0x%016llx\n", device_address);
    }
    data += 8;

    uint32_t error_resolution = *((uint32_t*)(void*)data);
    if (error_resolution == 0x80000000) {
        printf("  Error Resolution: Unknown\n");
    } else {
        printf("  Error Resolution: %u bytes\n", error_resolution);
    }
    data += 4;

    return 0;
}

static int8_t smbios_print_type_34(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    if (string_count < 1) {
        PRINTLOG(KERNEL, LOG_ERROR, "Failed to collect SMBIOS strings for Management Device structure");
        return -1;
    }

    if (header->length < 0x0B) {
        PRINTLOG(KERNEL, LOG_ERROR, "Management Device structure too short");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("Management Device (type %i) handle: 0x%04x\n", header->type, header->handle);

    printf("  Description: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint8_t type = *data;
    printf("  Type: ");
    switch (type) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("National Semiconductor LM75\n"); break;
    case 0x04: printf("National Semiconductor LM78\n"); break;
    case 0x05: printf("National Semiconductor LM79\n"); break;
    case 0x06: printf("National Semiconductor LM80\n"); break;
    case 0x07: printf("National Semiconductor LM81\n"); break;
    case 0x08: printf("Analog Devices ADM9240\n"); break;
    case 0x09: printf("Dallas Semiconductor DS1780\n"); break;
    case 0x0A: printf("Maxim 1617\n"); break;
    case 0x0B: printf("Genesys GL518SM\n"); break;
    case 0x0C: printf("Winbond W83781D\n"); break;
    case 0x0D: printf("Holtek HT82H791\n"); break;
    default: printf("Undefined (0x%02x)\n", type); break;
    }
    data++;

    uint32_t address = *((uint32_t*)(void*)data);
    printf("  Address: 0x%08x\n", address);
    data += 4;

    uint8_t address_type = *data;
    printf("  Address Type: ");
    switch (address_type) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("I/O Port\n"); break;
    case 0x04: printf("Memory Address\n"); break;
    case 0x05: printf("Bus Address (SMBus)\n"); break;
    default: printf("Undefined (0x%02x)\n", address_type); break;
    }
    data++;

    return 0;
}

static int8_t smbios_print_type_40(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    if (header->length < 0x05) {
        PRINTLOG(KERNEL, LOG_ERROR, "Additional Information structure too short");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("Additional Information (type %i) handle: 0x%04x\n", header->type, header->handle);

    uint8_t number_of_entries = *data;
    data++;

    printf("  Number of Entries: %u\n", number_of_entries);

    /*
     * Each entry is variable length:
     * 1 byte: Entry Length
     * 2 bytes: Referenced Handle
     * 1 byte: Referenced Offset
     * 1 byte: String Number
     * n bytes: Value
     */
    for (uint8_t i = 0; i < number_of_entries; i++) {
        uint8_t entry_length = *data;
        if (entry_length < 0x05) {
            PRINTLOG(KERNEL, LOG_ERROR, "Additional Information entry %u too short", i);
            break;
        }

        uint16_t ref_handle = *((uint16_t*)(void*)(data + 1));
        uint8_t ref_offset  = *(data + 3);
        uint8_t string_num  = *(data + 4);

        printf("  Entry %u:\n", i + 1);
        printf("    Referenced Handle: 0x%04x\n", ref_handle);
        printf("    Referenced Offset: 0x%02x\n", ref_offset);
        printf("    String Value: %s\n", (string_num && string_num <= string_count) ? strings[string_num - 1] : "Not Specified");

        if (entry_length > 5) {
            printf("    Value Data: ");
            for (uint8_t j = 5; j < entry_length; j++) {
                printf("%02x ", data[j]);
            }
            printf("\n");
        }

        data += entry_length;
    }

    return 0;
}

static int8_t smbios_print_type_41(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    if (string_count < 1) {
        PRINTLOG(KERNEL, LOG_ERROR, "Failed to collect SMBIOS strings for Onboard Devices Extended Information structure");
        return -1;
    }

    if (header->length < 0x0B) {
        PRINTLOG(KERNEL, LOG_ERROR, "Onboard Devices Extended Information structure too short");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("Onboard Devices Extended Information (type %i) handle: 0x%04x\n", header->type, header->handle);

    printf("  Reference Designation: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint8_t device_type_byte = *data;
    boolean_t enabled        = (device_type_byte & 0x80) >> 7;
    uint8_t device_type      = device_type_byte & 0x7F;

    printf("  Device Type: ");
    switch (device_type) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("Video\n"); break;
    case 0x04: printf("SCSI Controller\n"); break;
    case 0x05: printf("Ethernet\n"); break;
    case 0x06: printf("Token Ring\n"); break;
    case 0x07: printf("Sound\n"); break;
    case 0x08: printf("PATA Controller\n"); break;
    case 0x09: printf("SATA Controller\n"); break;
    case 0x0A: printf("SAS Controller\n"); break;
    case 0x0B: printf("Wireless LAN\n"); break;
    case 0x0C: printf("Bluetooth\n"); break;
    case 0x0D: printf("WWAN\n"); break;
    case 0x0E: printf("eMMC (Embedded Multi-Media Controller)\n"); break;
    case 0x0F: printf("NVMe Controller\n"); break;
    case 0x10: printf("UFS Controller\n"); break;
    default: printf("Undefined (0x%02x)\n", device_type); break;
    }

    printf("  Status: %s\n", enabled ? "Enabled" : "Disabled");
    data++;

    uint8_t device_type_instance = *data;
    printf("  Device Type Instance: %u\n", device_type_instance);
    data++;

    uint16_t segment_group_number = *((uint16_t*)(void*)data);
    data += 2;

    uint8_t bus_number = *data;
    data++;

    uint8_t device_function_number = *data;
    data++;

    if (segment_group_number == 0xFFFF && bus_number == 0xFF && device_function_number == 0xFF) {
        printf("  Device Address: Not Applicable\n");
    } else {
        printf("  Device Address: %04x:%02x:%02x.%01x\n",
               segment_group_number, bus_number,
               device_function_number >> 3,
               device_function_number & 0x07);
    }

    return 0;
}

static int8_t smbios_print_type_43(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    if (header->length < 0x1F) {
        PRINTLOG(KERNEL, LOG_ERROR, "TPM Device structure too short");
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("TPM Device (type %i) handle: 0x%04x\n", header->type, header->handle);

    // Vendor ID (4 bytes)
    char_t vendor_id[5];
    memory_memcopy(data, vendor_id, 4);
    vendor_id[4] = '\0';
    printf("  Vendor ID: %s\n", vendor_id);
    data += 4;

    // Major Spec Version
    uint8_t major_spec = *data;
    data++;
    // Minor Spec Version
    uint8_t minor_spec = *data;
    data++;
    printf("  Spec Version: %u.%u\n", major_spec, minor_spec);

    // Firmware Version
    uint32_t fw_1 = *((uint32_t*)(void*)data);
    data += 4;
    uint32_t fw_2 = *((uint32_t*)(void*)data);
    data += 4;
    printf("  Firmware Version: %u.%u (0x%08x 0x%08x)\n", fw_1 >> 16, fw_1 & 0xFFFF, fw_1, fw_2);

    // Description String
    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    if (string_count < 1) {
        PRINTLOG(KERNEL, LOG_ERROR, "Failed to collect SMBIOS strings for TPM Device structure");
        return -1;
    }

    printf("  Description: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    // Characteristics
    uint64_t characteristics = *((uint64_t*)(void*)data);
    printf("  Characteristics: 0x%016llx\n", characteristics);
    if (characteristics & (1ULL << 2)) {
        printf("    - TPM Device characteristics not supported\n");
    }
    if (characteristics & (1ULL << 3)) {
        printf("    - Family configurable via firmware update\n");
    }
    if (characteristics & (1ULL << 4)) {
        printf("    - Family configurable via platform software\n");
    }
    if (characteristics & (1ULL << 5)) {
        printf("    - Family configurable via OEM proprietary method\n");
    }
    data += 8;

    // OEM Defined
    uint32_t oem_defined = *((uint32_t*)(void*)data);
    printf("  OEM Defined: 0x%08x\n", oem_defined);
    data += 4;

    return 0;
}

static int8_t smbios_print_type_45(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version) {
    UNUSED(major_version);
    UNUSED(minor_version);

    char_t* strings[SMBIOS_MAX_STRINGS];
    int32_t string_count = smbios_collect_strings(header, strings, SMBIOS_MAX_STRINGS);

    if (string_count < 6) {
        PRINTLOG(KERNEL, LOG_ERROR, "Failed to collect SMBIOS strings for Firmware Inventory Information structure. Expected at least 6 strings, got %d", string_count);
        return -1;
    }

    if (header->length < 0x1A) {
        PRINTLOG(KERNEL, LOG_ERROR, "Firmware Inventory Information structure too short. Expected at least 0x1B bytes, got 0x%02x", header->length);
        return -1;
    }

    uint8_t* data = (uint8_t*)header;
    data += sizeof(smbios_structure_header_t); // Skip the header

    printf("Firmware Inventory Information (type %i) handle: 0x%04x\n", header->type, header->handle);

    printf("  Firmware Component Name: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    printf("  Firmware Version: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint8_t version_format = *data;
    printf("  Firmware Version Format: ");
    switch (version_format) {
    case 0x00: printf("Free Form\n"); break;
    case 0x01: printf("Major.Minor\n"); break;
    case 0x02: printf("32-bit Hex\n"); break;
    case 0x03: printf("64-bit Hex\n"); break;
    default: printf("Undefined (0x%02x)\n", version_format); break;
    }
    data++;

    printf("  Firmware ID: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint8_t firmware_id_format = *data;
    printf("  Firmware ID Format: ");
    switch (firmware_id_format) {
    case 0x00: printf("Free Form\n"); break;
    case 0x01: printf("UUID\n"); break;
    default: printf("Undefined (0x%02x)\n", firmware_id_format); break;
    }
    data++;

    printf("  Release Date: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    printf("  Manufacturer: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    printf("  Lowest Supported Firmware Version: %s\n", *data ? strings[*data - 1] : "Not Specified");
    data++;

    uint64_t image_size = *((uint64_t*)(void*)data);
    if (image_size != 0xFFFFFFFFFFFFFFFFULL) {
        printf("  Image Size: %llu bytes\n", image_size);
    } else {
        printf("  Image Size: Unknown\n");
    }
    data += 8;

    uint16_t characteristics = *((uint16_t*)(void*)data);
    printf("  Characteristics: 0x%04x\n", characteristics);
    if (characteristics & (1 << 0)) {
        printf("    - Updatable\n");
    } else {
        printf("    - Not Updatable\n");
    }
    if (characteristics & (1 << 1)) {
        printf("    - Write-Protect\n");
    } else {
        printf("    - Not Write-Protected\n");
    }
    data += 2;

    uint8_t state = *data;
    printf("  State: ");
    switch (state) {
    case 0x01: printf("Other\n"); break;
    case 0x02: printf("Unknown\n"); break;
    case 0x03: printf("Disabled\n"); break;
    case 0x04: printf("Enabled\n"); break;
    case 0x05: printf("Absent\n"); break;
    case 0x06: printf("Standby Offline\n"); break;
    case 0x07: printf("Standby Spare\n"); break;
    case 0x08: printf("Unavailable Offline\n"); break;
    default: printf("Undefined (0x%02x)\n", state); break;
    }
    data++;

    uint8_t associated_component_count = *data;
    data++;

    for (uint8_t i = 0; i < associated_component_count; i++) {
        uint16_t component_handle = *((uint16_t*)(void*)data);
        printf("  Associated Component Handle %u: 0x%04x\n", i + 1, component_handle);
        data += 2;
    }

    return 0;
}

typedef int8_t (*smbios_structure_printer_f)(smbios_structure_header_t* header, uint8_t major_version, uint8_t minor_version);

static const smbios_structure_printer_f smbios_structure_printers[256] = {
    [SMBIOS_STRUCTURE_TYPE_BIOS_INFORMATION]                     = smbios_print_type_0,
    [SMBIOS_STRUCTURE_TYPE_SYSTEM_INFORMATION]                   = smbios_print_type_1,
    [SMBIOS_STRUCTURE_TYPE_BASEBOARD_INFORMATION]                = smbios_print_type_2,
    [SMBIOS_STRUCTURE_TYPE_SYSTEM_ENCLOSURE_OR_CHASSIS]          = smbios_print_type_3,
    [SMBIOS_STRUCTURE_TYPE_PROCESSOR_INFORMATION]                = smbios_print_type_4,
    [SMBIOS_STRUCTURE_TYPE_CACHE_INFORMATION]                    = smbios_print_type_7,
    [SMBIOS_STRUCTURE_TYPE_PORT_CONNECTOR_INFORMATION]           = smbios_print_type_8,
    [SMBIOS_STRUCTURE_TYPE_SYSTEM_SLOTS]                         = smbios_print_type_9,
    [SMBIOS_STRUCTURE_TYPE_ONBOARD_DEVICES_INFORMATION]          = smbios_print_type_10,
    [SMBIOS_STRUCTURE_TYPE_OEM_STRINGS]                          = smbios_print_type_11,
    [SMBIOS_STRUCTURE_TYPE_SYSTEM_CONFIGURATION_OPTIONS]         = smbios_print_type_12,
    [SMBIOS_STRUCTURE_TYPE_BIOS_LANGUAGE_INFORMATION]            = smbios_print_type_13,
    [SMBIOS_STRUCTURE_TYPE_PHYSICAL_MEMORY_ARRAY]                = smbios_print_type_16,
    [SMBIOS_STRUCTURE_TYPE_MEMORY_DEVICE]                        = smbios_print_type_17,
    [SMBIOS_STRUCTURE_TYPE_32_BIT_MEMORY_ERROR_INFORMATION]      = smbios_print_type_18,
    [SMBIOS_STRUCTURE_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS]          = smbios_print_type_19,
    [SMBIOS_STRUCTURE_TYPE_MEMORY_DEVICE_MAPPED_ADDRESS]         = smbios_print_type_20,
    [SMBIOS_STRUCTURE_TYPE_SYSTEM_BOOT_INFORMATION]              = smbios_print_type_32,
    [SMBIOS_STRUCTURE_TYPE_64_BIT_MEMORY_ERROR_INFORMATION]      = smbios_print_type_33,
    [SMBIOS_STRUCTURE_TYPE_MANAGEMENT_DEVICE]                    = smbios_print_type_34,
    [SMBIOS_STRUCTURE_TYPE_ADDITIONAL_INFORMATION]               = smbios_print_type_40,
    [SMBIOS_STRUCTURE_TYPE_ONBOARD_DEVICES_EXTENDED_INFORMATION] = smbios_print_type_41,
    [SMBIOS_STRUCTURE_TYPE_TPM_DEVICE]                           = smbios_print_type_43,
    [SMBIOS_STRUCTURE_TYPE_FIRMWARE_INVENTORY_INFORMATION]       = smbios_print_type_45,
};

int8_t smbios_print_structure(smbios_structure_type_t type) {
    uint8_t major_version = 0;
    uint8_t minor_version = 0;
    uint8_t* smbios_table = NULL;

    if(SYSTEM_INFO->smbios_table_v3) {
        smbios_entrypoint_64_t* entrypoint = (smbios_entrypoint_64_t*)SYSTEM_INFO->smbios_table_v3;
        major_version = entrypoint->major_version;
        minor_version = entrypoint->minor_version;
        smbios_table  = (uint8_t*)(entrypoint->structure_table_address);
    } else if(SYSTEM_INFO->smbios_table_v2) {
        smbios_entrypoint_32_t* entrypoint = (smbios_entrypoint_32_t*)SYSTEM_INFO->smbios_table_v2;
        major_version = entrypoint->major_version;
        minor_version = entrypoint->minor_version;
        smbios_table  = (uint8_t*)(uintptr_t)(entrypoint->structure_table_address);
    } else {
        PRINTLOG(KERNEL, LOG_ERROR, "no smbios table found");
        return -1;
    }

    if(!smbios_table) {
        PRINTLOG(KERNEL, LOG_ERROR, "smbios structure table address is null");
        return -1;
    }

    boolean_t found = false;

    while(true) {
        smbios_structure_header_t* header = (smbios_structure_header_t*)smbios_table;

        if(header->type == SMBIOS_STRUCTURE_TYPE_END_OF_TABLE) {
            break;
        }

        if(header->type == type) {
            found = true;
            smbios_structure_printer_f printer = smbios_structure_printers[header->type];

            if(printer) {
                if(printer(header, major_version, minor_version) != 0) {
                    PRINTLOG(KERNEL, LOG_ERROR, "cannot print smbios structure type %i", header->type);
                    return -1;
                }
            } else {
                PRINTLOG(KERNEL, LOG_WARNING, "no printer for smbios structure type %i", header->type);
                return -1;
            }
            break;
        }

        // Move to the next structure
        smbios_table += header->length;

        // Skip the string section of the structure
        while(!(smbios_table[0] == 0 && smbios_table[1] == 0)) {
            smbios_table++;
        }
        smbios_table += 2; // Skip the double null terminator
    }

    if(!found) {
        PRINTLOG(KERNEL, LOG_WARNING, "smbios structure type %i not found", type);
        return -1;
    }

    return 0;
}

int8_t smbios_print_all_structures(void) {
    uint8_t major_version = 0;
    uint8_t minor_version = 0;
    uint8_t* smbios_table = NULL;

    if(SYSTEM_INFO->smbios_table_v3) {
        PRINTLOG(KERNEL, LOG_DEBUG, "smbios v3 table at 0x%p", SYSTEM_INFO->smbios_table_v3);
        smbios_entrypoint_64_t* entrypoint = (smbios_entrypoint_64_t*)SYSTEM_INFO->smbios_table_v3;
        major_version = entrypoint->major_version;
        minor_version = entrypoint->minor_version;
        smbios_table  = (uint8_t*)(entrypoint->structure_table_address);
    } else if(SYSTEM_INFO->smbios_table_v2) {
        PRINTLOG(KERNEL, LOG_DEBUG, "smbios v2 table at 0x%p", SYSTEM_INFO->smbios_table_v2);
        smbios_entrypoint_32_t* entrypoint = (smbios_entrypoint_32_t*)SYSTEM_INFO->smbios_table_v2;
        major_version = entrypoint->major_version;
        minor_version = entrypoint->minor_version;
        smbios_table  = (uint8_t*)(uintptr_t)(entrypoint->structure_table_address);
    } else {
        PRINTLOG(KERNEL, LOG_ERROR, "no smbios table found");
        return -1;
    }

    return smbios_print_all_structures_from_raw_data(smbios_table, major_version, minor_version);
}

int8_t smbios_print_all_structures_from_raw_data(uint8_t* smbios_table, uint8_t major_version, uint8_t minor_version) {
    if(!smbios_table) {
        PRINTLOG(KERNEL, LOG_ERROR, "smbios structure table address is null");
        return -1;
    }

    while(true) {
        smbios_structure_header_t* header = (smbios_structure_header_t*)smbios_table;

        if(header->type == SMBIOS_STRUCTURE_TYPE_END_OF_TABLE) {
            break;
        }

        smbios_structure_printer_f printer = smbios_structure_printers[header->type];

        if(printer) {
            if(printer(header, major_version, minor_version) != 0) {
                PRINTLOG(KERNEL, LOG_ERROR, "cannot print smbios structure type %i", header->type);
            }
        } else {
            PRINTLOG(KERNEL, LOG_WARNING, "no printer for smbios structure type %i handle 0x%04x", header->type, header->handle);
            PRINTLOG(KERNEL, LOG_WARNING, "printing raw data with length %u", header->length);
            uint8_t* data = (uint8_t*)header;
            for(uint8_t i = 0; i < header->length; i++) {
                if(i % 16 == 0) {
                    printf("\n  %04x: ", i);
                }
                printf("%02x ", data[i]);
            }
            printf("\n");
        }

        printf("\n");

        // Move to the next structure
        smbios_table += header->length;

        // Skip the string section of the structure
        while(!(smbios_table[0] == 0 && smbios_table[1] == 0)) {
            smbios_table++;
        }
        smbios_table += 2; // Skip the double null terminator
    }


    return 0;
}
