/*
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#ifndef ___EFI_H
#define ___EFI_H 0

#include <types.h>
#include <disk.h>

#define EFIAPI __attribute__((ms_abi))

typedef struct {
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_high_version;
	uint8_t data[8];
}__attribute__((packed))  efi_guid_t;

typedef struct {
	uint8_t head;
	uint8_t sector;
	uint8_t cylinder;
}__attribute__((packed))  efi_pmbr_chs_t;

#define EFI_PMBR_PART_TYPE 0xEE

typedef struct {
	uint8_t status;
	efi_pmbr_chs_t first_chs;
	uint8_t part_type;
	efi_pmbr_chs_t last_chs;
	uint32_t first_lba;
	uint32_t sector_count;
} __attribute__((packed))  efi_pmbr_partition_t;

typedef struct {
	uint64_t signature;
	uint32_t revision;
	uint32_t header_size;
	uint32_t crc32;
	uint32_t reserved;
}__attribute__((packed))  efi_table_header_t;

typedef struct {
	efi_table_header_t header;
	uint64_t my_lba;
	uint64_t alternate_lba;
	uint64_t first_usable_lba;
	uint64_t last_usable_lba;
	efi_guid_t disk_guid;
	uint64_t partition_entry_lba;
	uint32_t partition_entry_count;
	uint32_t partition_entry_size;
	uint32_t partition_entries_crc32;
}__attribute__((packed))  efi_partition_table_header_t;

typedef struct {
	efi_guid_t partition_type_guid;
	efi_guid_t unique_partition_guid;
	uint64_t starting_lba;
	uint64_t ending_lba;
	uint64_t attributes;
	wchar_t partition_name[36];
}__attribute__((packed))  efi_partition_entry_t;

#define EFI_PART_TABLE_HEADER_SIGNATURE     0x5452415020494645ULL
#define EFI_PART_TABLE_HEADER_REVISION      0x00010000

#define EFI_PART_TYPE_UNUSED_GUID                     { 0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} }
#define EFI_PART_TYPE_EFI_SYSTEM_PART_GUID            { 0xc12a7328, 0xf81f, 0x11d2, {0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b} }
#define EFI_PART_TYPE_LEGACY_MBR_GUID                 { 0x024dee41, 0x33e7, 0x11d3, {0x9d, 0x69, 0x00, 0x08, 0xc7, 0x81, 0xf3, 0x9f} }
#define EFI_PART_TYPE_TURNSTONE_KERNEL_PART_GUID      {0x1DF53B77, 0xCDFC, 0x4903, {0x9B, 0xE6, 0x08, 0x9C, 0xE0, 0x25, 0x7A, 0x09} }

#define EFI_ACPI_TABLE_GUID                 { 0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d} }
#define EFI_ACPI_20_TABLE_GUID              { 0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81} }

int8_t efi_create_guid(efi_guid_t* guid);
int8_t efi_guid_equal(efi_guid_t guid1, efi_guid_t guid2);

typedef struct {
	disk_t underlaying_disk;
	disk_t* underlaying_disk_pointer;
	efi_partition_table_header_t* gpt_header;
	efi_partition_entry_t* partitions;
} gpt_disk_t;

disk_partition_context_t* gpt_create_partition_context(efi_guid_t* type, char_t* name, uint64_t start, uint64_t end);

disk_t* gpt_get_or_create_gpt_disk(disk_t* underlaying_disk);

#define EFI_ERROR_CODE(a)               (0x8000000000000000LL | a)
#define EFIWARN(a)                      (a)
#define EFI_ERROR(a)                    (((int64_t) a) < 0)
#define EFI_SUCCESS                     0
#define EFI_LOAD_ERROR                  EFI_ERROR_CODE (1)
#define EFI_INVALID_PARAMETER           EFI_ERROR_CODE (2)
#define EFI_UNSUPPORTED                 EFI_ERROR_CODE (3)
#define EFI_BAD_BUFFER_SIZE             EFI_ERROR_CODE (4)
#define EFI_BUFFER_TOO_SMALL            EFI_ERROR_CODE (5)
#define EFI_NOT_READY                   EFI_ERROR_CODE (6)
#define EFI_DEVICE_ERROR                EFI_ERROR_CODE (7)
#define EFI_WRITE_PROTECTED             EFI_ERROR_CODE (8)
#define EFI_OUT_OF_RESOURCES            EFI_ERROR_CODE (9)
#define EFI_VOLUME_CORRUPTED            EFI_ERROR_CODE (10)
#define EFI_VOLUME_FULL                 EFI_ERROR_CODE (11)
#define EFI_NO_MEDIA                    EFI_ERROR_CODE (12)
#define EFI_MEDIA_CHANGED               EFI_ERROR_CODE (13)
#define EFI_NOT_FOUND                   EFI_ERROR_CODE (14)
#define EFI_ACCESS_DENIED               EFI_ERROR_CODE (15)
#define EFI_NO_RESPONSE                 EFI_ERROR_CODE (16)
#define EFI_NO_MAPPING                  EFI_ERROR_CODE (17)
#define EFI_TIMEOUT                     EFI_ERROR_CODE (18)
#define EFI_NOT_STARTED                 EFI_ERROR_CODE (19)
#define EFI_ALREADY_STARTED             EFI_ERROR_CODE (20)
#define EFI_ABORTED                     EFI_ERROR_CODE (21)
#define EFI_ICMP_ERROR                  EFI_ERROR_CODE (22)
#define EFI_TFTP_ERROR                  EFI_ERROR_CODE (23)
#define EFI_PROTOCOL_ERROR              EFI_ERROR_CODE (24)
#define EFI_INCOMPATIBLE_VERSION        EFI_ERROR_CODE (25)
#define EFI_SECURITY_VIOLATION          EFI_ERROR_CODE (26)
#define EFI_CRC_ERROR                   EFI_ERROR_CODE (27)
#define EFI_END_OF_MEDIA                EFI_ERROR_CODE (28)
#define EFI_END_OF_FILE                 EFI_ERROR_CODE (31)
#define EFI_INVALID_LANGUAGE            EFI_ERROR_CODE (32)
#define EFI_COMPROMISED_DATA            EFI_ERROR_CODE (33)

#define EFI_WARN_UNKOWN_GLYPH           EFIWARN(1)
#define EFI_WARN_UNKNOWN_GLYPH          EFIWARN(1)
#define EFI_WARN_DELETE_FAILURE         EFIWARN(2)
#define EFI_WARN_WRITE_FAILURE          EFIWARN(3)
#define EFI_WARN_BUFFER_TOO_SMALL       EFIWARN(4)


typedef void* efi_handle_t;
typedef void* efi_event_t;
typedef uint64_t efi_tpl_t;
typedef uint64_t efi_status_t;
typedef uint64_t efi_physical_address_t;
typedef uint64_t efi_virtual_address_t;

typedef enum {
	EFI_ALLOCATE_ANY_PAGES,
	EFI_ALLOCATE_MAX_ADDRESS,
	EFI_ALLOCATE_ADDRESS,
	EFI_MAX_ALLOCATE_TYPE
} efi_allocate_type_t;


typedef enum {
	EFI_ALL_HANDLES,
	EFI_BY_REGISTER_NOTIFY,
	EFI_BY_PROTOCOL
} efi_locate_search_type_t;

typedef enum {
	EFI_RESET_COLD,
	EFI_RESET_WARM,
	EFI_RESET_SHUTDOWN
} efi_reset_type_t;

typedef enum {
	EFI_RESERVED_MEMORY_TYPE, // 0
	EFI_LOADER_CODE,
	EFI_LOADER_DATA,
	EFI_BOOT_SERVICES_CODE,
	EFI_BOOT_SERVICES_DATA,
	EFI_RUNTIME_SERVICES_CODE, // 5
	EFI_RUNTIME_SERVICES_DATA,
	EFI_CONVENTIONAL_MEMORY,
	EFI_UNUSABLE_MEMORY,
	EFI_ACPI_RECLAIM_MEMORY,
	EFI_ACPI_MEMORY_NVS,  // A
	EFI_MEMORY_MAPPED_IO,
	EFI_MEMORY_MAPPED_IO_PORT_SPACE,
	EFI_PAL_CODE,
	EFI_PERSISTENT_MEMORY,
	EFI_UNACCEPTED_MEMORY, // F
	EFI_MAX_MEMORY_TYPE,
} efi_memory_type_t;

#define EFI_MEMORY_UC            0x0000000000000001
#define EFI_MEMORY_WC            0x0000000000000002
#define EFI_MEMORY_WT            0x0000000000000004
#define EFI_MEMORY_WB            0x0000000000000008
#define EFI_MEMORY_UCE           0x0000000000000010
#define EFI_MEMORY_WP            0x0000000000001000
#define EFI_MEMORY_RP            0x0000000000002000
#define EFI_MEMORY_XP            0x0000000000004000
#define EFI_MEMORY_NV            0x0000000000008000
#define EFI_MEMORY_MORE_RELIABLE 0x0000000000010000
#define EFI_MEMORY_RO            0x0000000000020000
#define EFI_MEMORY_SP            0x0000000000040000
#define EFI_MEMORY_CPU_CRYPTO    0x0000000000080000
#define EFI_MEMORY_RUNTIME       0x8000000000000000

typedef enum {
	EFI_TIMER_CANCEL,
	EFI_TIMER_PERIODIC,
	EFI_TIMER_RELATIVE,
	EFI_TIMER_TYPE_MAX
} efi_timer_delay_t;

typedef enum {
	EFI_DEVICE_PATH_TYPE_HARDWARE=1,
	EFI_DEVICE_PATH_TYPE_ACPI=2,
	EFI_DEVICE_PATH_TYPE_MESSAGING=3,
	EFI_DEVICE_PATH_TYPE_MEDIA=4,
	EFI_DEVICE_PATH_TYPE_BIOS_BOOT=5,
	EFI_DEVICE_PATH_TYPE_EOF=0x7F,
}efi_device_path_type_t;

typedef enum {
	EFI_DEVICE_PATH_SUBTYPE_HARDWARE_PCI=1,
	EFI_DEVICE_PATH_SUBTYPE_HARDWARE_PCCARD=2,
	EFI_DEVICE_PATH_SUBTYPE_HARDWARE_MEMORY_MAPPED=3,
	EFI_DEVICE_PATH_SUBTYPE_HARDWARE_VENDOR=4,
	EFI_DEVICE_PATH_SUBTYPE_HARDWARE_CONTROLLER=5,
	EFI_DEVICE_PATH_SUBTYPE_HARDWARE_BMC=6,
}efi_device_path_subtype_hardware_t;

typedef enum {
	EFI_DEVICE_PATH_SUBTYPE_ACPI_ACPI=1,
	EFI_DEVICE_PATH_SUBTYPE_ACPI_ACPI_EX=2,
	EFI_DEVICE_PATH_SUBTYPE_ACPI_ADR=3,
	EFI_DEVICE_PATH_SUBTYPE_ACPI_NVDIMM=4,
}efi_device_path_subtype_acpi_t;

typedef enum {
	EFI_DEVICE_PATH_SUBTYPE_EOF_EOI=0x01,
	EFI_DEVICE_PATH_SUBTYPE_EOF_EOF=0xFF,
} efi_device_path_subtype_eof_t;

typedef enum {
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_ATAPI=1,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_SCSI=2,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_FC=3,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_1394=4,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_USB=5,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_I2O=6,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_IB=9,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_VENDOR=10,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_MAC=11,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_IPV4=12,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_IPV6=13,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_UART=14,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_USB_CLASS=15,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_USB_WWID=16,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_LUN=17,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_SATA=18,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_ISCSI=19,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_VLAN=20,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_FC_EX=21,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_SAS_EX=22,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_NVME=23,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_URI=24,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_UFS=25,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_SD=26,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_BLUETOOTH=27,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_WIFI=28,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_EMMC=29,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_BLUETOOTHLE=30,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_DNS=31,
	EFI_DEVICE_PATH_SUBTYPE_MESSAGING_WTF=32, ///< NVDIMM_NS & REST uses it
}efi_device_path_subtype_messaging_t;

typedef enum {
	EFI_DEVICE_PATH_SUBTYPE_MEDIA_HDD=1,
	EFI_DEVICE_PATH_SUBTYPE_MEDIA_CDROM=2,
	EFI_DEVICE_PATH_SUBTYPE_MEDIA_VENDOR=3,
	EFI_DEVICE_PATH_SUBTYPE_MEDIA_FILEPATH=4,
	EFI_DEVICE_PATH_SUBTYPE_MEDIA_MEDIA=5,
	EFI_DEVICE_PATH_SUBTYPE_MEDIA_PWING=6,
	EFI_DEVICE_PATH_SUBTYPE_MEDIA_PWING_FW_VOL=7,
	EFI_DEVICE_PATH_SUBTYPE_MEDIA_RELATIVE_OFFSET=8,
	EFI_DEVICE_PATH_SUBTYPE_MEDIA_RAMDISK=9,
}efi_device_path_subtype_media_t;

typedef struct {
	efi_device_path_type_t type : 8;
	uint8_t sub_type;
	uint16_t length;
}__attribute__((packed)) efi_device_path_t;

typedef struct {
	uint32_t type;
	efi_physical_address_t physical_start;
	efi_virtual_address_t virtual_start;
	uint64_t page_count;
	uint64_t attribute;
} efi_memory_descriptor_t;


typedef struct {
	uint16_t scan_code;
	wchar_t unicode_char;
} efi_input_key_t;


typedef efi_status_t (EFIAPI* efi_input_reset_t)(void* this, boolean_t extended_verification);
typedef efi_status_t (EFIAPI* efi_input_read_key_t)(void* this, efi_input_key_t* Key);


typedef struct {
	efi_input_reset_t reset;
	efi_input_read_key_t read_key_stroke;
	efi_event_t wait_for_key;
} simple_input_interface_t;


typedef efi_status_t (EFIAPI* efi_text_reset_t)(void* this, boolean_t extended_verification);
typedef efi_status_t (EFIAPI* efi_text_output_string_t)(void* this, wchar_t* string);
typedef efi_status_t (EFIAPI* efi_text_test_string_t)(void* this, wchar_t* string);
typedef efi_status_t (EFIAPI* efi_text_query_mode_t)(void* this, uint64_t mode_number, uint64_t* column, uint64_t* row);
typedef efi_status_t (EFIAPI* efi_text_set_mode_t)(void* this, uint64_t mode_number);
typedef efi_status_t (EFIAPI* efi_text_set_attribute_t)(void* this, uint64_t attribute);
typedef efi_status_t (EFIAPI* efi_text_clear_screen_t)(void* this);
typedef efi_status_t (EFIAPI* efi_text_set_cursor_t)(void* this, uint64_t column, uint64_t row);
typedef efi_status_t (EFIAPI* efi_text_enable_cursor_t)(void* this,  boolean_t enable);

typedef struct {
	int32_t max_mode;
	int32_t mode;
	int32_t attribute;
	int32_t cursor_column;
	int32_t cursor_row;
	boolean_t cursor_visible;
} simple_text_output_mode_t;


typedef struct {
	efi_text_reset_t reset;
	efi_text_output_string_t output_string;
	efi_text_test_string_t test_string;
	efi_text_query_mode_t query_mode;
	efi_text_set_mode_t set_mode;
	efi_text_set_attribute_t set_attribute;
	efi_text_clear_screen_t clear_screen;
	efi_text_set_cursor_t set_cursor_position;
	efi_text_enable_cursor_t enable_cursor;
	simple_text_output_mode_t* mode;
} simple_text_output_interface_t;

typedef struct {
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	uint8_t padding1;
	uint32_t nano_second;
	int16_t time_zone;
	uint8_t day_light;
	uint8_t padding2;
} efi_time_t;

typedef struct {
	uint32_t resolution;
	uint32_t accuracy;
	boolean_t sets_to_zero;
} efi_time_capabilities_t;

typedef struct {
	efi_guid_t capsule_guid;
	uint32_t header_size;
	uint32_t flags;
	uint32_t capsule_image_size;
} efi_capsule_header_t;

typedef struct {
	uint64_t length;
	efi_physical_address_t continuation_pointer;
} efi_capsule_block_descriptor_t;

typedef efi_status_t (EFIAPI* efi_get_time_t)(efi_time_t* time, efi_time_capabilities_t* capabilities);
typedef efi_status_t (EFIAPI* efi_set_time_t)(efi_time_t* time);
typedef efi_status_t (EFIAPI* efi_get_wakeup_time_t)(boolean_t* enable, boolean_t* pending, efi_time_t* time);
typedef efi_status_t (EFIAPI* efi_set_wakeup_time_t)(boolean_t enable, efi_time_t* time);
typedef efi_status_t (EFIAPI* efi_set_virtual_address_map_t)(uint64_t memory_map_size, uint64_t descriptor_size, uint32_t descriptor_version, efi_memory_descriptor_t* virtual_map);
typedef efi_status_t (EFIAPI* efi_convert_pointer_t)(uint64_t debug_disposition, void** address);
typedef efi_status_t (EFIAPI* efi_get_variable_t)(wchar_t* variable_name, efi_guid_t* vendor_guid, uint32_t* attributes, uint64_t* data_size, void* data);
typedef efi_status_t (EFIAPI* efi_get_next_variable_name_t)(uint64_t* variable_name_size, wchar_t* variable_name, efi_guid_t* vendor_guid);
typedef efi_status_t (EFIAPI* efi_set_variable_t)(wchar_t* variable_name, efi_guid_t* vendor_guid, uint32_t attributes, uint64_t data_size, void* data);
typedef efi_status_t (EFIAPI* efi_get_next_high_mono_t)(uint64_t* count);
typedef efi_status_t (EFIAPI* efi_reset_system_t)(efi_reset_type_t reset_type, efi_status_t reset_status, uint64_t data_size, wchar_t* reset_data);
typedef efi_status_t (EFIAPI* efi_update_capsule_t)(efi_capsule_header_t** capsule_header_array, uint64_t capsule_count, efi_physical_address_t scatter_gather_list);
typedef efi_status_t (EFIAPI* efi_query_capsule_capabilities_t)(efi_capsule_header_t** capsule_header_array, uint64_t capsule_count, uint64_t* maximum_capsule_size, efi_reset_type_t* reset_type);
typedef efi_status_t (EFIAPI* efi_query_variable_info_t)(uint32_t attributes, uint64_t* maximum_variable_storage_size,  uint64_t* remaining_variable_storage_size, uint64_t* maximum_variable_size);


typedef struct {
	efi_table_header_t header;

	efi_get_time_t get_time;
	efi_set_time_t set_time;
	efi_get_wakeup_time_t get_wakeup_time;
	efi_set_wakeup_time_t set_wakeup_time;

	efi_set_virtual_address_map_t set_virtual_addressmap;
	efi_convert_pointer_t convert_pointer;

	efi_get_variable_t get_variable;
	efi_get_next_variable_name_t get_next_variable_name;
	efi_set_variable_t set_variable;

	efi_get_next_high_mono_t get_next_high_monotonic_count;
	efi_reset_system_t reset_system;

	efi_update_capsule_t update_capsule;
	efi_query_capsule_capabilities_t query_capsule_capabilities;
	efi_query_variable_info_t query_variable_info;
} efi_runtime_services_t;

typedef struct {
	efi_handle_t AgentHandle;
	efi_handle_t ControllerHandle;
	uint32_t Attributes;
	uint32_t OpenCount;
} efi_open_protocol_information_entry_t;

typedef efi_tpl_t (EFIAPI* efi_raise_tpl_t)(efi_tpl_t new_tpl);
typedef efi_tpl_t (EFIAPI* efi_restore_tpl_t)(efi_tpl_t old_tpl);
typedef efi_status_t (EFIAPI* efi_allocate_pages_t)(efi_allocate_type_t type, efi_memory_type_t memory_type, uint64_t num_pages, efi_physical_address_t* memory);
typedef efi_status_t (EFIAPI* efi_free_pages_t)(efi_physical_address_t memory, uint64_t num_pages);
typedef efi_status_t (EFIAPI* efi_get_memory_map_t)(uint64_t* memory_map_size, efi_memory_descriptor_t* memory_map, uint64_t* map_key, uint64_t* descriptor_size, uint32_t* descriptor_version);
typedef efi_status_t (EFIAPI* efi_allocate_pool_t)(efi_memory_type_t pool_type, uint64_t size, void** buffer);
typedef efi_status_t (EFIAPI* efi_free_pool_t)(void* buffer);
typedef void (EFIAPI* efi_event_notify_t)(efi_event_t event, void* context);
typedef efi_status_t (EFIAPI* efi_create_event_t)(uint32_t type, efi_tpl_t notify_tpl, efi_event_notify_t notify_function, void* next_context, efi_event_t* event);
typedef efi_status_t (EFIAPI* efi_set_timer_t)(efi_event_t event, efi_timer_delay_t Type, uint64_t trigger_time);
typedef efi_status_t (EFIAPI* efi_wait_for_event_t)(uint64_t number_of_events, efi_event_t* event, uint64_t* index);
typedef efi_status_t (EFIAPI* efi_signal_event_t)(efi_event_t event);
typedef efi_status_t (EFIAPI* efi_close_event_t)(efi_event_t event);
typedef efi_status_t (EFIAPI* efi_check_event_t)(efi_event_t event);
typedef efi_status_t (EFIAPI* efi_handle_protocol_t)(efi_handle_t handle, efi_guid_t* protocol, void** interface);
typedef efi_status_t (EFIAPI* efi_register_protocol_notify_t)(efi_guid_t* protocol, efi_event_t event, void** registration);
typedef efi_status_t (EFIAPI* efi_locate_handle_t)(efi_locate_search_type_t search_type, efi_guid_t* protocol, void* search_key, uint64_t* buffer_size, efi_handle_t* buffer);
typedef efi_status_t (EFIAPI* efi_locate_device_path_t)(efi_guid_t* protocol, efi_device_path_t** device_path, efi_handle_t* device);
typedef efi_status_t (EFIAPI* efi_install_configuration_table_t)(efi_guid_t* guid, void* table);
typedef efi_status_t (EFIAPI* efi_image_load_t)(boolean_t BootPolicy, efi_handle_t parent_image_handle, efi_device_path_t* file_path, void* source_buffer, uint64_t source_size, efi_handle_t* image_handle);
typedef efi_status_t (EFIAPI* efi_image_start_t)(efi_handle_t image_handle, uint64_t* exit_data_size, wchar_t** exit_data);
typedef efi_status_t (EFIAPI* efi_exit_t)(efi_handle_t image_handle, efi_status_t exit_status, uint64_t exit_data_size, wchar_t* exit_data);
typedef efi_status_t (EFIAPI* efi_exit_boot_services_t)(efi_handle_t image_handle, uint64_t map_key);
typedef efi_status_t (EFIAPI* efi_get_next_monotonic_t)(uint64_t* count);
typedef efi_status_t (EFIAPI* efi_stall_t)(uint64_t microseconds);
typedef efi_status_t (EFIAPI* efi_set_watchdog_timer_t)(uint64_t timeout, uint64_t watchdog_code, uint64_t data_size, wchar_t* watchdog_data);
typedef efi_status_t (EFIAPI* efi_connect_controller_t)(efi_handle_t controller_handle, efi_handle_t* driver_image_handle, efi_device_path_t* remaining_device_path, boolean_t recursive);
typedef efi_status_t (EFIAPI* efi_disconnect_controller_t)(efi_handle_t controller_handle, efi_handle_t driver_image_handle, efi_handle_t child_handle);
typedef efi_status_t (EFIAPI* efi_open_protocol_t)(efi_handle_t handle, efi_guid_t* protocol, void** interface, efi_handle_t agent_handle, efi_handle_t controller_handle, uint32_t attributes);
typedef efi_status_t (EFIAPI* efi_close_protocol_t)(efi_handle_t handle, efi_guid_t* protocol, efi_handle_t agent_handle, efi_handle_t controller_handle);
typedef efi_status_t (EFIAPI* efi_open_protocol_information_t)(efi_handle_t handle, efi_guid_t* protocol, efi_open_protocol_information_entry_t** entry_buffer, uint64_t* entry_count);
typedef efi_status_t (EFIAPI* efi_protocols_per_handle_t)(efi_handle_t handle, efi_guid_t*** protocol_buffer, uint64_t* protocol_buffer_count);
typedef efi_status_t (EFIAPI* efi_locate_handle_buffer_t)(efi_locate_search_type_t search_type, efi_guid_t* protocol, void* search_key, uint64_t num_handles, efi_handle_t** handles);
typedef efi_status_t (EFIAPI* efi_locate_protocol_t)(efi_guid_t* Protocol, void* registration, void** interface);
typedef efi_status_t (EFIAPI* efi_calculate_crc32_t)(void* Data, uint64_t data_size, uint32_t* crc32);

typedef struct {
	efi_table_header_t header;

	efi_raise_tpl_t raise_tpl;
	efi_restore_tpl_t restore_tpl;

	efi_allocate_pages_t allocate_pages;
	efi_free_pages_t free_pages;
	efi_get_memory_map_t get_memory_map;
	efi_allocate_pool_t allocate_pool;
	efi_free_pool_t free_pool;

	efi_create_event_t create_event;
	efi_set_timer_t set_timer;
	efi_wait_for_event_t wait_for_event;
	efi_signal_event_t signal_event;
	efi_close_event_t close_event;
	efi_check_event_t check_event;

	void* install_protocol_interface;
	void* reinstall_protocol_interface;
	void* uninstall_protocol_interface;
	efi_handle_protocol_t handle_protocol;
	efi_handle_protocol_t pc_handle_protocol;
	efi_register_protocol_notify_t register_protocol_notify;
	efi_locate_handle_t locate_handle;
	efi_locate_device_path_t locate_device_path;
	efi_install_configuration_table_t install_configuration_table;

	efi_image_load_t load_image;
	efi_image_start_t start_image;
	efi_exit_t exit;
	void* unload_image;
	efi_exit_boot_services_t exit_boot_services;

	efi_get_next_monotonic_t get_next_high_monotonic_count;
	efi_stall_t stall;
	efi_set_watchdog_timer_t set_watchdog_timer;

	efi_connect_controller_t connect_controller;
	efi_disconnect_controller_t disconnect_controller;

	efi_open_protocol_t open_protocol;
	efi_close_protocol_t close_protocol;
	efi_open_protocol_information_t open_protocol_information;

	efi_protocols_per_handle_t protocols_per_handle;
	efi_locate_handle_buffer_t locate_handle_buffer;
	efi_locate_protocol_t locate_protocol;
	void* install_multiple_protocol_interfaces;
	void* uninstall_multiple_protocol_interfaces;

	efi_calculate_crc32_t calculate_crc32;
} efi_boot_services_t;


typedef struct {
	efi_guid_t vendor_guid;
	void* vendor_table;
} efi_configuration_table_t;

typedef struct {
	efi_table_header_t header;

	wchar_t* firmware_vendor;
	uint32_t firmware_revision;

	efi_handle_t console_input_handle;
	simple_input_interface_t* console_input;

	efi_handle_t console_output_handle;
	simple_text_output_interface_t* console_output;

	efi_handle_t console_error_handle;
	simple_text_output_interface_t* console_error;

	efi_runtime_services_t* runtime_services;
	efi_boot_services_t* boot_services;

	uint64_t configuration_table_entry_count;
	efi_configuration_table_t* configuration_table;
} efi_system_table_t;


#define EFI_BLOCK_IO_PROTOCOL_GUID { 0x964e5b21, 0x6459, 0x11d2, {0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b} }

#define EFI_BLOCK_IO_PROTOCOL_REVISION    0x00010000
#define EFI_BLOCK_IO_INTERFACE_REVISION   EFI_BLOCK_IO_PROTOCOL_REVISION
#define EFI_BLOCK_IO_PROTOCOL_REVISION2 0x00020001
#define EFI_BLOCK_IO_PROTOCOL_REVISION3 ((2 << 16) | (31))

typedef struct {
	uint32_t media_id;
	boolean_t removable_media;
	boolean_t media_present;
	boolean_t logical_partition;
	boolean_t readonly;
	boolean_t write_caching;
	uint32_t block_size;
	uint32_t io_align;
	uint64_t last_block;
} efi_block_io_media_t;

typedef efi_status_t (EFIAPI* efi_block_reset_t)(efi_handle_t self, boolean_t extended_verification);
typedef efi_status_t (EFIAPI* efi_block_read_t)(efi_handle_t self, uint32_t media_id, uint64_t lba, uint64_t buffer_size, void* buffer);
typedef efi_status_t (EFIAPI* efi_block_write_t)(efi_handle_t self, uint32_t media_id, uint64_t lba, uint64_t buffer_size, void* buffer);
typedef efi_status_t (EFIAPI* efi_block_flush_t)(efi_handle_t self);

typedef struct {
	uint64_t revision;
	efi_block_io_media_t* media;
	efi_block_reset_t reset;
	efi_block_read_t read;
	efi_block_write_t write;
	efi_block_flush_t flush;
} efi_block_io_t;

typedef struct {
	uint64_t offset;
	efi_block_io_t* bio;
} block_file_t;


#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID { 0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } }

typedef enum {
	EFI_PIXEL_RED_GREEN_BLUE_RESERVED_8BIT_PER_COLOR,
	EFI_PIXEL_BLUE_GREEN_RED_RESERVED_8BIT_PER_COLOR,
	EFI_PIXEL_BIT_MASK,
	EFI_PIXEL_BLT_ONLY,
	EFI_PIXEL_FORMAT_MAX
} efi_gop_pixel_format_t;

typedef enum {
	EFI_BLT_VIDEO_FILL,
	EFI_BLT_VIDEO_TO_BLT_BUFFER,
	EFI_BLT_BUFFER_TO_VIDEO,
	EFI_BLT_VIDEO_TO_VIDEO,
	EFI_GRAPHICS_OUTPUT_BLT_OPERATION_MAX
} efi_gop_blt_operation_t;

typedef struct {
	uint32_t red_mask;
	uint32_t green_mask;
	uint32_t blue_mask;
	uint32_t reserved_mask;
} efi_gop_pixel_bitmask_t;

typedef struct {
	uint32_t version;
	uint32_t horizontal_resolution;
	uint32_t vertical_resolution;
	efi_gop_pixel_format_t pixel_format;
	efi_gop_pixel_bitmask_t pixel_information;
	uint32_t pixels_per_scanline;
} efi_gop_mode_info_t;

typedef struct {
	uint32_t max_mode;
	uint32_t mode;
	efi_gop_mode_info_t* information;
	uint64_t size_of_info;
	efi_physical_address_t frame_buffer_base;
	uint64_t frame_buffer_size;
} efi_gop_mode_t;

typedef efi_status_t (EFIAPI* efi_gop_query_mode_t)(void* self, uint32_t mode_number, uint64_t* size_of_info,
                                                    efi_gop_mode_info_t** info);
typedef efi_status_t (EFIAPI* efi_gop_set_mode_t)(void* self, uint32_t mode_number);
typedef efi_status_t (EFIAPI* efi_gop_blt_t)(void* self, uint32_t* blt_buffer, efi_gop_blt_operation_t blt_operation,
                                             uint64_t source_x, uint64_t source_y, uint64_t destination_x, uint64_t destionation_y, uint64_t width, uint64_t height, uint64_t delta);

typedef struct {
	efi_gop_query_mode_t query_mode;
	efi_gop_set_mode_t set_mode;
	efi_gop_blt_t blt;
	efi_gop_mode_t* mode;
} efi_gop_t;

#define EFI_LOADED_IMAGE_PROTOCOL_GUID { 0x5B1B31A1, 0x9562, 0x11d2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} }
#define EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID { 0xBC62157E, 0x3E33, 0x4FEC, {0x99, 0x20, 0x2D, 0x3B, 0x36, 0xD7, 0x50, 0xDF} }

typedef efi_status_t (EFIAPI* efi_image_unload_t)(efi_handle_t image_handle);

typedef struct {
	uint32_t revision;
	efi_handle_t parent_handle;
	efi_system_table_t* system_table;
	efi_handle_t device_handle;
	efi_device_path_t* file_path;
	void* reserved;
	uint32_t load_options_size;
	void* load_options;
	void* image_base;
	uint64_t image_size;
	efi_memory_type_t image_code_type;
	efi_memory_type_t image_data_type;
	efi_image_unload_t unload;
} efi_loaded_image_t;

#define EFI_GLOBAL_VARIABLE { 0x8BE4DF61, 0x93CA, 0x11d2, {0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B, 0x8C } }

#define EFI_PXE_BASE_CODE_PROTOCOL_GUID { 0x03C4E603, 0xAC28, 0x11D3, {0x9A, 0x2D, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D} }
#define EFI_PXE_BASE_CODE_PROTOCOL_REVISION 0x00010000

#define EFI_PXE_BASE_CODE_MAX_ARP_ENTRIES 8
#define EFI_PXE_BASE_CODE_MAX_ROUTE_ENTRIES 8
#define EFI_PXE_BASE_CODE_MAX_IPCNT 8

#define EFI_PXE_BASE_CODE_DEFAULT_TTL 16
#define EFI_PXE_BASE_CODE_DEFAULT_TOS 0

typedef uint16_t efi_pxe_base_code_udp_port_t;

typedef struct {
	uint8_t addr[4];
}efi_ipv4_address_t;

typedef struct {
	uint8_t addr[16];
}efi_ipv6_address_t;

typedef union {
	uint32_t addr[4];
	efi_ipv4_address_t v4;
	efi_ipv6_address_t v6;
} efi_ip_address_t;

typedef struct {
	uint8_t addr[32];
} efi_mac_address_t;

typedef enum {
	EFI_PXE_BASE_CODE_IP_FILTER_STATION_IP = 1,
	EFI_PXE_BASE_CODE_IP_FILTER_BROADCAST = 2,
	EFI_PXE_BASE_CODE_IP_FILTER_PROMISCUOUS = 4,
	EFI_PXE_BASE_CODE_IP_FILTER_PROMISCUOUS_MULTICAT = 8,
} efi_pxe_base_code_ip_filter_type_t;

typedef struct {
	efi_pxe_base_code_ip_filter_type_t filters : 8;
	uint8_t ip_cnt;
	uint16_t reserved;
	efi_ip_address_t ip_list[EFI_PXE_BASE_CODE_MAX_IPCNT];
} efi_pxe_base_code_ip_filter_t;

typedef struct {
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	union {
		uint32_t reserved;
		uint32_t mtu;
		uint32_t pointer;
		struct {
			uint16_t identifier;
			uint16_t sequence;
		} echo;
	} u;
	uint8_t data[494];
} __attribute__((packed)) efi_pxe_base_code_icmp_error_t;

typedef struct {
	uint8_t error_code;
	char_t error_string[127];
} __attribute__((packed)) efi_pxe_base_code_tftp_error_t;

typedef struct {
	efi_ip_address_t ip_address;
	efi_mac_address_t mac_address;
} efi_pxe_base_code_arp_entry_t;

typedef struct {
	efi_ip_address_t ip_address;
	efi_ip_address_t subnet_mask;
	efi_ip_address_t gateway_address;
} efi_pxe_base_code_route_entry_t;

typedef enum {
	EFI_PXE_BASE_CODE_UDP_OPFLAGS_ANY_SRC_IP=1,
	EFI_PXE_BASE_CODE_UDP_OPFLAGS_ANY_SRC_PORT=2,
	EFI_PXE_BASE_CODE_UDP_OPFLAGS_ANY_DST_IP=4,
	EFI_PXE_BASE_CODE_UDP_OPFLAGS_ANY_DST_PORT=8,
	EFI_PXE_BASE_CODE_UDP_OPFLAGS_ANY_FILTER=0x10,
	EFI_PXE_BASE_CODE_UDP_OPFLAGS_ANY_FRAGMENT=0x20,
} efi_pxe_base_code_udp_opflags_t;

typedef struct {
	uint8_t bootp_opcode;
	uint8_t bootp_hw_len;
	uint8_t bootp_hw_addr_len;
	uint8_t bootp_gate_hops;
	uint32_t bootp_ident;
	uint16_t bootp_seconds;
	uint16_t bootp_flags;
	uint8_t bootp_ci_addr[4];
	uint8_t bootp_yi_addr[4];
	uint8_t bootp_si_addr[4];
	uint8_t bootp_gi_addr[4];
	uint8_t bootp_hw_addr[16];
	uint8_t bootp_srv_name[64];
	uint8_t bootp_boot_file[128];
	uint32_t dhcp_magic;
	uint8_t dhcp_options[56];
} __attribute__((packed)) efi_pxe_base_code_dhcpv4_packet_t;

typedef struct {
	uint32_t message_type : 8;
	uint32_t transaction_id : 24;
	uint8_t dhcp_options[1024];
} __attribute__((packed)) efi_pxe_base_code_dhcpv6_packet_t;

typedef union {
	uint8_t raw[1472];
	efi_pxe_base_code_dhcpv4_packet_t dhcpv4;
	efi_pxe_base_code_dhcpv6_packet_t dhcpv6;
} __attribute__((packed)) efi_pxe_base_code_packet_t;

typedef struct {
	boolean_t started;
	boolean_t ipv6_available;
	boolean_t ipv6_supported;
	boolean_t ipv6_using;
	boolean_t bis_supported;
	boolean_t bis_detected;
	boolean_t auto_arp;
	boolean_t send_guid;
	boolean_t dhcp_discover_vaild;
	boolean_t dhcp_ack_received;
	boolean_t proxy_offer_received;
	boolean_t pxe_discover_valid;
	boolean_t pxe_reply_received;
	boolean_t pxe_bis_reply_received;
	boolean_t icmp_error_received;
	boolean_t tftp_error_received;
	boolean_t make_callbacks;
	uint8_t ttl;
	uint8_t tos;
	efi_ip_address_t station_ip;
	efi_ip_address_t subnet_mask;
	efi_pxe_base_code_packet_t dhcp_discover;
	efi_pxe_base_code_packet_t dhcp_ack;
	efi_pxe_base_code_packet_t proxy_offer;
	efi_pxe_base_code_packet_t pxe_discover;
	efi_pxe_base_code_packet_t pxe_reply;
	efi_pxe_base_code_packet_t pxe_bis_reply;
	efi_pxe_base_code_ip_filter_t ip_filter;
	uint32_t arp_cache_entries;
	efi_pxe_base_code_arp_entry_t arp_cache[EFI_PXE_BASE_CODE_MAX_ARP_ENTRIES];
	uint32_t route_table_entries;
	efi_pxe_base_code_route_entry_t route_table[EFI_PXE_BASE_CODE_MAX_ROUTE_ENTRIES];
	efi_pxe_base_code_icmp_error_t icmp_error;
	efi_pxe_base_code_tftp_error_t tftp_error;
}efi_base_code_mode_t;

typedef enum {
	EFI_PXE_BASE_CODE_BOOT_TYPE_BOOTSTRAP=0,
	FI_PXE_BASE_CODE_BOOT_TYPE_MS_WINNT_RIS=1,
	EFI_PXE_BASE_CODE_BOOT_TYPE_INTEL_LCM=2,
	EFI_PXE_BASE_CODE_BOOT_TYPE_DOSUNDI=3,
	EFI_PXE_BASE_CODE_BOOT_TYPE_NEC_ESMPRO=4,
	EFI_PXE_BASE_CODE_BOOT_TYPE_IBM_WSOD=5,
	EFI_PXE_BASE_CODE_BOOT_TYPE_IBM_LCCM=6,
	EFI_PXE_BASE_CODE_BOOT_TYPE_CA_UNICENTER_TNG=7,
	EFI_PXE_BASE_CODE_BOOT_TYPE_HP_OPENVIEW=8,
	EFI_PXE_BASE_CODE_BOOT_TYPE_ALTIRIS_9=9,
	EFI_PXE_BASE_CODE_BOOT_TYPE_ALTIRIS_10=10,
	EFI_PXE_BASE_CODE_BOOT_TYPE_ALTIRIS_11=11,
	EFI_PXE_BASE_CODE_BOOT_TYPE_NOT_USED_12=12,
	EFI_PXE_BASE_CODE_BOOT_TYPE_REDHAT_INSTALL=13,
	EFI_PXE_BASE_CODE_BOOT_TYPE_REDHAT_BOOT=14,
	EFI_PXE_BASE_CODE_BOOT_TYPE_REMBO=15,
	EFI_PXE_BASE_CODE_BOOT_TYPE_BEOBOOT=16,
	EFI_PXE_BASE_CODE_BOOT_TYPE_PXETEST=65535,
} efi_pxe_base_code_boot_type_t;

#define EFI_PXE_BASE_CODE_BOOT_LAYER_MASK     0x7FFF
#define EFI_PXE_BASE_CODE_BOOT_LAYER_INITIAL  0x0000

typedef struct {
	uint16_t type;
	boolean_t accept_any_response;
	uint8_t reserved;
	efi_ip_address_t ip_addr;
} efi_pxe_base_code_srvlist_t;

typedef struct {
	boolean_t use_mcast;
	boolean_t use_bcast;
	boolean_t use_ucast;
	boolean_t must_use_list;
	efi_ip_address_t server_mcast_ip;
	uint16_t ip_cnt;
	efi_pxe_base_code_srvlist_t srvlist[EFI_PXE_BASE_CODE_MAX_IPCNT];
} efi_pxe_base_code_discover_info_t;

typedef enum {
	EFI_PXE_BASE_CODE_TFTP_FIRST,
	EFI_PXE_BASE_CODE_TFTP_GET_FILE_SIZE,
	EFI_PXE_BASE_CODE_TFTP_READ_FILE,
	EFI_PXE_BASE_CODE_TFTP_WRITE_FILE,
	EFI_PXE_BASE_CODE_TFTP_READ_DIRECTORY,
	EFI_PXE_BASE_CODE_MTFTP_GET_FILE_SIZE,
	EFI_PXE_BASE_CODE_MTFTP_READ_FILE,
	EFI_PXE_BASE_CODE_MTFTP_READ_DIRECTORY,
	EFI_PXE_BASE_CODE_MTFTP_LAST
} efi_pxe_base_code_tftp_opcode_t;

typedef struct {
	efi_ip_address_t mcast_ip;
	efi_pxe_base_code_udp_port_t cport;
	efi_pxe_base_code_udp_port_t sport;
	uint16_t listen_timeout;
	uint16_t transmit_timeout;
} efi_pxe_base_code_mtftp_info_t;

typedef efi_status_t (EFIAPI* efi_base_code_start_t)(efi_handle_t this, boolean_t use_ipv6);
typedef efi_status_t (EFIAPI* efi_base_code_stop_t)(efi_handle_t this);
typedef efi_status_t (EFIAPI* efi_base_code_dhcp_t)(efi_handle_t this, boolean_t sort_offers);
typedef efi_status_t (EFIAPI* efi_base_code_discover_t)(efi_handle_t this, uint16_t type, uint16_t* layer, boolean_t use_bis, efi_pxe_base_code_discover_info_t* info);
typedef efi_status_t (EFIAPI* efi_base_code_mtftp_t)(efi_handle_t this, efi_pxe_base_code_tftp_opcode_t opcode, void* buffer, boolean_t overwrite, uint64_t* buffer_size, uint64_t* block_size, efi_ip_address_t* server_ip, char_t* file_name, efi_pxe_base_code_mtftp_info_t* info, boolean_t dont_use_buffer);
typedef efi_status_t (EFIAPI* efi_base_code_udp_write_t)(efi_handle_t this, uint16_t op_flags, efi_ip_address_t* dst_ip, efi_pxe_base_code_udp_port_t* dst_port, efi_ip_address_t* gw_ip, efi_ip_address_t* src_ip, efi_pxe_base_code_udp_port_t* src_port, uint64_t* header_size, void* header, uint64_t* buffer_size, void* buffer);
typedef efi_status_t (EFIAPI* efi_base_code_udp_read_t)(efi_handle_t this, uint16_t op_flags, efi_ip_address_t* dst_ip, efi_pxe_base_code_udp_port_t* dst_port, efi_ip_address_t* src_ip, efi_pxe_base_code_udp_port_t* src_port, uint64_t* header_size, void* header, uint64_t* buffer_size, void* buffer);
typedef efi_status_t (EFIAPI* efi_base_code_set_ip_filer_t)(efi_handle_t this, efi_pxe_base_code_ip_filter_t* new_filter);
typedef efi_status_t (EFIAPI* efi_base_code_arp_t)(efi_handle_t this, efi_ip_address_t* ip_addr, efi_mac_address_t* mac_addr);
typedef efi_status_t (EFIAPI* efi_base_code_set_parameters_t)(efi_handle_t this, boolean_t* new_auto_arp, boolean_t* new_send_guid, uint8_t* new_ttl, uint8_t* new_tos, boolean_t* new_make_callback);
typedef efi_status_t (EFIAPI* efi_base_code_set_station_ip_t)(efi_handle_t this, efi_ip_address_t* new_station_ip, efi_ip_address_t* new_subnet_mask);
typedef efi_status_t (EFIAPI* efi_base_code_set_packets_t)(efi_handle_t this, boolean_t* new_dhcp_discover_valid, boolean_t* new_dhcp_ack_received, boolean_t* new_proxy_offer_received, boolean_t* new_pxe_discover_valid, boolean_t* new_pxe_reply_received, boolean_t* new_pxe_bis_reply_received, efi_pxe_base_code_packet_t* new_dhcp_discover, efi_pxe_base_code_packet_t* new_dhcp_ack, efi_pxe_base_code_packet_t* new_proxy_offer, efi_pxe_base_code_packet_t* new_pxe_discover, efi_pxe_base_code_packet_t* new_pxe_reply, efi_pxe_base_code_packet_t* new_pxe_bis_reply);

typedef struct {
	uint64_t revision;
	efi_base_code_start_t start;
	efi_base_code_stop_t stop;
	efi_base_code_dhcp_t dhcp;
	efi_base_code_discover_t discover;
	efi_base_code_mtftp_t mtftp;
	efi_base_code_udp_write_t udp_write;
	efi_base_code_udp_read_t udp_read;
	efi_base_code_set_ip_filer_t set_ip_filer;
	efi_base_code_arp_t arp;
	efi_base_code_set_parameters_t set_parameters;
	efi_base_code_set_station_ip_t set_station_ip;
	efi_base_code_set_packets_t set_packets;
	efi_base_code_mode_t* mode;
} efi_pxe_base_code_protocol_t;

#define EFI_IMAGE_DOSSTUB_HEADER_MAGIC 0x5A4D
#define EFI_IMAGE_DOSSTUB_LENGTH 0x80
#define EFI_IMAGE_DOSSTUB_EFI_IMAGE_OFFSET_LOCATION 0x3C

#define EFI_IMAGE_HEADER_MAGIC 0x00004550
#define EFI_IMAGE_OPTIONAL_HEADER_MAGIC 0x020B

#define EFI_IMAGE_MACHINE_AMD64 0x8664
#define EFI_IMAGE_CHARACTERISTISCS 0x2226

typedef struct efi_image_header_s {
	uint32_t magic;
	uint16_t machine;
	uint16_t number_of_sections;
	uint32_t timedatestamp;
	uint32_t pointer_to_symbol_table;
	uint32_t number_of_symbols;
	uint16_t size_of_optional_header;
	uint16_t characteristics;
} __attribute__((packed)) efi_image_header_t;

typedef struct efi_image_optinal_data_table_s {
	uint32_t virtual_address;
	uint32_t size;
} __attribute__((packed)) efi_image_optinal_data_table_t;

typedef struct efi_image_optional_header_s {
	uint16_t magic;
	uint8_t major_linker_version;
	uint8_t minor_linker_version;
	uint32_t size_of_code;
	uint32_t size_of_initialized_data;
	uint32_t size_of_uninitialized_data;
	uint32_t address_of_entrypoint;
	uint32_t base_of_code;
	uint64_t image_base;
	uint32_t section_alignment;
	uint32_t file_alignment;
	uint16_t major_operating_system_version;
	uint16_t minor_operating_system_version;
	uint16_t major_image_version;
	uint16_t minor_image_version;
	uint16_t major_subsystem_version;
	uint16_t minor_subsystem_version;
	uint32_t win32_version_value;
	uint32_t size_of_image;
	uint32_t size_of_headers;
	uint32_t checksum;
	uint16_t subsystem;
	uint16_t dll_characteristics;
	uint64_t size_of_stack_reserve;
	uint64_t size_of_stack_commit;
	uint64_t size_of_heap_reserve;
	uint64_t size_of_heap_commit;
	uint32_t loader_flags;
	uint32_t number_of_rva_nd_sizes; ///< count of tables below
	efi_image_optinal_data_table_t export_table;
	efi_image_optinal_data_table_t import_table;
	efi_image_optinal_data_table_t resource_table;
	efi_image_optinal_data_table_t exception_table;
	efi_image_optinal_data_table_t certificate_table;
	efi_image_optinal_data_table_t base_relocation_table;
	efi_image_optinal_data_table_t debug_table;
	efi_image_optinal_data_table_t architecture_table;
	efi_image_optinal_data_table_t global_pointer_table;
	efi_image_optinal_data_table_t tls_table;
	efi_image_optinal_data_table_t load_config_table;
	efi_image_optinal_data_table_t bound_import_table;
	efi_image_optinal_data_table_t import_address_table;
	efi_image_optinal_data_table_t delay_import_descriptor_table;
	efi_image_optinal_data_table_t clr_runtime_header_table;
	efi_image_optinal_data_table_t reserved;
} __attribute__((packed)) efi_image_optional_header_t;

typedef struct efi_image_section_header_s {
	char_t name[8];
	uint32_t virtual_size;
	uint32_t virtual_address;
	uint32_t size_of_raw_data;
	uint32_t pointer_to_raw_data;
	uint32_t pointer_to_relocations;  ///< should be zero
	uint32_t pointer_to_line_numbers; ///< should be zero
	uint16_t number_of_relocations; ///< should be zero
	uint16_t number_of_line_numbers; ///< should be zero
	uint32_t characteristics;
} __attribute__((packed)) efi_image_section_header_t;


typedef struct efi_image_relocation_entry_s {
	uint32_t page_rva;
	uint32_t block_size;
	uint16_t offset : 12;
	uint16_t type : 4;
} __attribute__((packed)) efi_image_relocation_entry_t;

#endif
