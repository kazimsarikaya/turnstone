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
#define EFI_PART_TYPE_UNUSED_GUID           { 0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} }
#define EFI_PART_TYPE_EFI_SYSTEM_PART_GUID  { 0xc12a7328, 0xf81f, 0x11d2, {0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b} }
#define EFI_PART_TYPE_LEGACY_MBR_GUID       { 0x024dee41, 0x33e7, 0x11d3, {0x9d, 0x69, 0x00, 0x08, 0xc7, 0x81, 0xf3, 0x9f} }

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

#define EFIWARN(a)                            (a)
#define EFI_ERROR(a)           (((int64_t) a) < 0)
#define EFI_SUCCESS                            0
#define EFI_LOAD_ERROR                  EFIERR(1)
#define EFI_INVALID_PARAMETER           EFIERR(2)
#define EFI_UNSUPPORTED                 EFIERR(3)
#define EFI_BAD_BUFFER_SIZE             EFIERR(4)
#define EFI_BUFFER_TOO_SMALL            EFIERR(5)
#define EFI_NOT_READY                   EFIERR(6)
#define EFI_DEVICE_ERROR                EFIERR(7)
#define EFI_WRITE_PROTECTED             EFIERR(8)
#define EFI_OUT_OF_RESOURCES            EFIERR(9)
#define EFI_VOLUME_CORRUPTED            EFIERR(10)
#define EFI_VOLUME_FULL                 EFIERR(11)
#define EFI_NO_MEDIA                    EFIERR(12)
#define EFI_MEDIA_CHANGED               EFIERR(13)
#define EFI_NOT_FOUND                   EFIERR(14)
#define EFI_ACCESS_DENIED               EFIERR(15)
#define EFI_NO_RESPONSE                 EFIERR(16)
#define EFI_NO_MAPPING                  EFIERR(17)
#define EFI_TIMEOUT                     EFIERR(18)
#define EFI_NOT_STARTED                 EFIERR(19)
#define EFI_ALREADY_STARTED             EFIERR(20)
#define EFI_ABORTED                     EFIERR(21)
#define EFI_ICMP_ERROR                  EFIERR(22)
#define EFI_TFTP_ERROR                  EFIERR(23)
#define EFI_PROTOCOL_ERROR              EFIERR(24)
#define EFI_INCOMPATIBLE_VERSION        EFIERR(25)
#define EFI_SECURITY_VIOLATION          EFIERR(26)
#define EFI_CRC_ERROR                   EFIERR(27)
#define EFI_END_OF_MEDIA                EFIERR(28)
#define EFI_END_OF_FILE                 EFIERR(31)
#define EFI_INVALID_LANGUAGE            EFIERR(32)
#define EFI_COMPROMISED_DATA            EFIERR(33)

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
	EFI_RESERVED_MEMORY_TYPE,
	EFI_LOADER_CODE,
	EFI_LOADER_DATA,
	EFI_BOOT_SERVICES_CODE,
	EFI_BOOT_SERVICES_DATA,
	EFI_RUNTIME_SERVICES_CODE,
	EFI_RUNTIME_SERVICES_DATA,
	EFI_CONVENTIONAL_MEMORY,
	EFI_UNUSABLE_MEMORY,
	EFI_ACPI_RECLAIM_MEMORY,
	EFI_ACPI_MEMORY_NVS,
	EFI_MEMORY_MAPPED_IO,
	EFI_MEMORY_MAPPED_IO_PORT_SPACE,
	EFI_PAL_CODE,
	EFI_MAX_MEMORY_TYPE
} efi_memory_type_t;

typedef enum {
	EFI_TIMER_CANCEL,
	EFI_TIMER_PERIODIC,
	EFI_TIMER_RELATIVE,
	EFI_TIMER_TYPE_MAX
} efi_timer_delay_t;


typedef struct {
	uint8_t type;
	uint8_t sub_type;
	uint8_t length[2];
} efi_device_path_t;

typedef struct {
	uint32_t type;
	uint32_t padding;
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

	uint64_t table_entry_count;
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


#endif
